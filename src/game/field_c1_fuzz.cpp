// BOF3X_SHADOW=field_c1: group FC1's 41 functions through the scenario
// harness's field mode (scenario_harness.h, docs/scenario_harness.md section
// 7), once at start-up. docs/field_c1.md section 4.
//
// The clone table is tools/band_rows.py --group FC1 --clones (2026-09-29, the
// extents read from the code, every jump internal, no jump table inside a
// function, nothing refused), names given. The shapes: every effect state and
// dispatcher a kSprite (void, run on Sprite_Current); the Config row draw and
// the E8 helpers kCall. The five dispatchers of ours read their tables in
// place, swapped for recorders while the fuzz runs (the DataTables below);
// the tables the catalog's dispatchers read are not ours to swap and are not
// read by any function here.
//
// What the harness lacks for this group, listed here and reported for the
// fold after the wave (docs/field_c1.md section 4.2): narrower masks where
// Capcom pushes a whole register for a word or a byte (Menu_DrawBox,
// AreaMap_SetByte, AreaMap_Slope's direction, MoveCmd_Move's object); answers
// in the callee's range (Sprite_FindFree, Sprite_ObjectAt, Party_MemberAt);
// louder stand-ins where the caller reads a cell after the call
// (AreaMap_Slope's DamageScratch, AreaMap_Elevation near the heights compared,
// AreaMap_ByteAt's tested bytes, EventOp_6x / _0x moving Sprite_Current).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_c1.h"
#include "game/field_c1_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_c1 {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group FC1 --clones, 2026-09-29.
constexpr sh::CallSite kCalls461800[] = {{0x5E, 0x57CF60}, {0x9F, 0x516B30}, {0xB7, 0x5905D0}, {0xF0, 0x516E70}, {0xFF, 0x5A7650}, {0x15A, 0x461E50}};
constexpr sh::CallSite kCalls469D10[] = {{0x0, 0x589410}, {0x1C, 0x5890E0}, {0x21, 0x588F20}};
constexpr sh::CallSite kCalls469D40[] = {{0x3C, 0x5893A0}, {0x5A, 0x5890E0}, {0x67, 0x588F20}};
constexpr sh::CallSite kCalls469FB0[] = {{0x14, 0x57C110}, {0x1C, 0x589840}};
constexpr sh::CallSite kCalls46A040[] = {{0x1, 0x469E50}};
constexpr sh::CallSite kCalls46A070[] = {{0x0, 0x469EF0}};
constexpr sh::CallSite kCalls46A0C0[] = {{0x10, 0x469F70}};
constexpr sh::CallSite kCalls46A100[] = {{0x16, 0x589590}, {0x39, 0x5891F0}, {0xC9, 0x578C10}, {0xD1, 0x46A1E0}};
constexpr sh::CallSite kCalls46A1E0[] = {{0xD, 0x5720C0}, {0x9E, 0x5893A0}, {0xA3, 0x588F00}};
constexpr sh::CallSite kCalls46A290[] = {{0xF, 0x5720C0}, {0x48, 0x587740}, {0x71, 0x5893A0}, {0x76, 0x588F00}};
constexpr sh::CallSite kCalls46A310[] = {{0x0, 0x589840}};
constexpr sh::CallSite kCalls46A340[] = {{0xA, 0x589590}, {0x3C, 0x5891F0}};
constexpr sh::CallSite kCalls46A390[] = {{0x0, 0x589410}, {0x40, 0x588F20}};
constexpr sh::CallSite kCalls46A600[] = {{0x1, 0x57CD90}, {0x33, 0x57CD90}, {0x86, 0x57AD10}, {0x8F, 0x589200}, {0xA5, 0x57AD10}, {0xAE, 0x589200}, {0x1AB, 0x454CC0}, {0x1CB, 0x454CC0}};
constexpr sh::CallSite kCalls46A950[] = {{0x1, 0x57CD90}, {0x33, 0x57CD90}, {0x86, 0x57A010}, {0x9C, 0x57A010}, {0x199, 0x454CC0}, {0x1B9, 0x454CC0}};
constexpr sh::CallSite kCalls46ABD0[] = {{0xB, 0x589590}, {0x10, 0x46B380}, {0xA0, 0x5891F0}};
constexpr sh::CallSite kCalls46AC90[] = {{0x1F, 0x46B580}, {0x3F, 0x579F00}, {0x127, 0x5891F0}, {0x140, 0x588F00}, {0x155, 0x579F00}, {0x16F, 0x588F00}, {0x18D, 0x579F00}, {0x195, 0x588F00}};
constexpr sh::CallSite kCalls46AE30[] = {{0x180, 0x5720C0}, {0x1DA, 0x5720C0}, {0x20D, 0x46B400}, {0x227, 0x46B5C0}, {0x248, 0x46B580}, {0x29A, 0x5891F0}, {0x2AC, 0x5893A0}, {0x2B1, 0x588F00}, {0x2BA, 0x46B4B0}, {0x2C2, 0x5893A0}, {0x2C7, 0x588F00}};
constexpr sh::CallSite kCalls46B100[] = {{0x98, 0x5720C0}, {0xE2, 0x46B400}, {0xFF, 0x5720C0}, {0x15A, 0x46B6D0}, {0x178, 0x46B380}, {0x17E, 0x5891F0}, {0x18F, 0x5893A0}, {0x194, 0x588F00}, {0x1A2, 0x5893A0}, {0x1A7, 0x588F00}};
constexpr sh::CallSite kCalls46B2C0[] = {{0x63, 0x46B3C0}, {0x68, 0x589840}, {0x6D, 0x588F00}};
constexpr sh::CallSite kCalls46B340[] = {{0x2C, 0x46B3C0}, {0x31, 0x5893A0}, {0x36, 0x588F00}};
constexpr sh::CallSite kCalls46B380[] = {{0xF, 0x536700}, {0x2E, 0x579F00}};
constexpr sh::CallSite kCalls46B400[] = {{0xF, 0x531CF0}, {0x4C, 0x5893A0}, {0x51, 0x588F00}, {0x68, 0x531F10}, {0x96, 0x5893A0}, {0x9B, 0x588F00}};
constexpr sh::CallSite kCalls46B580[] = {{0x29, 0x46B5C0}};
constexpr sh::CallSite kCalls46B5C0[] = {{0xF, 0x46B6D0}, {0x51, 0x5722D0}, {0x64, 0x5720C0}, {0xAB, 0x5722D0}, {0xBE, 0x5720C0}, {0xEA, 0x46B6D0}};
constexpr sh::CallSite kCalls46B6D0[] = {{0x5C, 0x5720C0}, {0x65, 0x536700}};
constexpr sh::CallSite kCalls46B7C0[] = {{0xE, 0x46D0E0}, {0x2B, 0x5891F0}, {0x37, 0x5366A0}};
constexpr sh::CallSite kCalls46B8A0[] = {{0x6E, 0x46BA90}, {0x87, 0x589810}, {0xB5, 0x5893A0}, {0xBA, 0x588F00}, {0xC3, 0x5891F0}, {0xD3, 0x5893A0}, {0xD8, 0x588F00}};
constexpr sh::CallSite kCalls46B980[] = {{0x0, 0x589410}, {0x9, 0x589840}, {0xE, 0x588F20}};
constexpr sh::CallSite kCalls46B9A0[] = {{0xD4, 0x5891F0}};
constexpr sh::CallSite kCalls46BA90[] = {{0xF, 0x531CF0}, {0x66, 0x5720C0}, {0x91, 0x536700}};
constexpr sh::CallSite kCalls46BB50[] = {{0x30, 0x57BA60}, {0x82, 0x46BF80}, {0x98, 0x57B830}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define FC1_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kS = sh::Shape::kSprite;
constexpr sh::Shape kC = sh::Shape::kCall;
const sh::Clone kClones[] = {
    {"Config_DrawRowLabel", 0x461800, 0x16A, kCalls461800, SH_N(kCalls461800), nullptr, 0, nullptr, 0, FC1_FN(Config_DrawRowLabel), 0, false, kC},
    {"EffectKind06_PlayOnce", 0x469D10, 0x26, kCalls469D10, SH_N(kCalls469D10), nullptr, 0, nullptr, 0, FC1_FN(EffectKind06_PlayOnce), 0, false, kS},
    {"EffectKind06_Fade", 0x469D40, 0x6C, kCalls469D40, SH_N(kCalls469D40), nullptr, 0, nullptr, 0, FC1_FN(EffectKind06_Fade), 0, false, kS},
    {"EffectKind19_Run", 0x469DE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC1_FN(EffectKind19_Run), 0, false, kS},
    {"EffectKind19_Tick", 0x469E00, 0x2D, nullptr, 0, nullptr, 0, nullptr, 0, FC1_FN(EffectKind19_Tick), 0, false, kS},
    {"EffectKind04_HoldTick", 0x469FB0, 0x27, kCalls469FB0, SH_N(kCalls469FB0), nullptr, 0, nullptr, 0, FC1_FN(EffectKind04_HoldTick), 0, false, kS},
    {"EffectKind31_Run", 0x46A020, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, FC1_FN(EffectKind31_Run), 0, false, kS},
    {"CameraZoom_Start", 0x46A040, 0x28, kCalls46A040, SH_N(kCalls46A040), nullptr, 0, nullptr, 0, FC1_FN(CameraZoom_Start), 0, false, kS},
    {"CameraZoom_Step", 0x46A070, 0x4E, kCalls46A070, SH_N(kCalls46A070), nullptr, 0, nullptr, 0, FC1_FN(CameraZoom_Step), 0, false, kS},
    {"CameraZoom_End", 0x46A0C0, 0x15, kCalls46A0C0, SH_N(kCalls46A0C0), nullptr, 0, nullptr, 0, FC1_FN(CameraZoom_End), 0, false, kS},
    {"EffectKind32_Run", 0x46A0E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC1_FN(EffectKind32_Run), 0, false, kS},
    {"EffectKind32_Throw", 0x46A100, 0xDB, kCalls46A100, SH_N(kCalls46A100), nullptr, 0, nullptr, 0, FC1_FN(EffectKind32_Throw), 0, false, kS},
    {"EffectKind32_Arc", 0x46A1E0, 0xA8, kCalls46A1E0, SH_N(kCalls46A1E0), nullptr, 0, nullptr, 0, FC1_FN(EffectKind32_Arc), 0, false, kS},
    {"EffectKind32_Settle", 0x46A290, 0x7E, kCalls46A290, SH_N(kCalls46A290), nullptr, 0, nullptr, 0, FC1_FN(EffectKind32_Settle), 0, false, kS},
    {"Effect_StateRelease", 0x46A310, 0x5, kCalls46A310, SH_N(kCalls46A310), nullptr, 0, nullptr, 0, FC1_FN(Effect_StateRelease), 0, false, kS},
    {"EffectKind37_Start", 0x46A340, 0x4F, kCalls46A340, SH_N(kCalls46A340), nullptr, 0, nullptr, 0, FC1_FN(EffectKind37_Start), 0, false, kS},
    {"EffectKind37_Play", 0x46A390, 0x45, kCalls46A390, SH_N(kCalls46A390), nullptr, 0, nullptr, 0, FC1_FN(EffectKind37_Play), 0, false, kS},
    {"EffectKind14_Start", 0x46A600, 0x1E9, kCalls46A600, SH_N(kCalls46A600), nullptr, 0, nullptr, 0, FC1_FN(EffectKind14_Start), 0, false, kS},
    {"EffectKind14_Hold", 0x46A7F0, 0x5E, nullptr, 0, nullptr, 0, nullptr, 0, FC1_FN(EffectKind14_Hold), 0, false, kS},
    {"EffectKind3C_Start", 0x46A950, 0x1D7, kCalls46A950, SH_N(kCalls46A950), nullptr, 0, nullptr, 0, FC1_FN(EffectKind3C_Start), 0, false, kS},
    {"EffectKind3C_Hold", 0x46AB30, 0x7A, nullptr, 0, nullptr, 0, nullptr, 0, FC1_FN(EffectKind3C_Hold), 0, false, kS},
    {"EffectKind17_Start", 0x46ABD0, 0xB1, kCalls46ABD0, SH_N(kCalls46ABD0), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_Start), 0, false, kS},
    {"EffectKind17_Push", 0x46AC90, 0x19C, kCalls46AC90, SH_N(kCalls46AC90), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_Push), 0, false, kS},
    {"EffectKind17_Slide", 0x46AE30, 0x2CF, kCalls46AE30, SH_N(kCalls46AE30), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_Slide), 0, false, kS},
    {"EffectKind17_Settle", 0x46B100, 0x1B1, kCalls46B100, SH_N(kCalls46B100), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_Settle), 0, false, kS},
    {"EffectKind17_Grow", 0x46B2C0, 0x73, kCalls46B2C0, SH_N(kCalls46B2C0), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_Grow), 0, false, kS},
    {"EffectKind17_Rest", 0x46B340, 0x3B, kCalls46B340, SH_N(kCalls46B340), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_Rest), 0, false, kS},
    {"EffectKind17_TakeCell", 0x46B380, 0x37, kCalls46B380, SH_N(kCalls46B380), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_TakeCell), 0, false, kC},
    {"EffectKind17_CameraBack", 0x46B3C0, 0x38, nullptr, 0, nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_CameraBack), 0, false, kC},
    {"EffectKind17_Bump", 0x46B400, 0xA6, kCalls46B400, SH_N(kCalls46B400), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_Bump), 0xFF, false, kC},
    {"EffectKind17_AlignTarget", 0x46B4B0, 0xC5, nullptr, 0, nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_AlignTarget), 0, false, kC},
    {"EffectKind17_BlockedAhead", 0x46B580, 0x33, kCalls46B580, SH_N(kCalls46B580), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_BlockedAhead), 0xFF, false, kC},
    {"EffectKind17_BlockedAt", 0x46B5C0, 0x106, kCalls46B5C0, SH_N(kCalls46B5C0), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_BlockedAt), 0xFF, false, kC},
    {"EffectKind17_CellBlocked", 0x46B6D0, 0xCA, kCalls46B6D0, SH_N(kCalls46B6D0), nullptr, 0, nullptr, 0, FC1_FN(EffectKind17_CellBlocked), 0xFF, false, kC},
    {"EffectKind1B_Start", 0x46B7C0, 0xD1, kCalls46B7C0, SH_N(kCalls46B7C0), nullptr, 0, nullptr, 0, FC1_FN(EffectKind1B_Start), 0, false, kS},
    {"EffectKind1B_Fly", 0x46B8A0, 0xDF, kCalls46B8A0, SH_N(kCalls46B8A0), nullptr, 0, nullptr, 0, FC1_FN(EffectKind1B_Fly), 0, false, kS},
    {"EffectKind1B_End", 0x46B980, 0x13, kCalls46B980, SH_N(kCalls46B980), nullptr, 0, nullptr, 0, FC1_FN(EffectKind1B_End), 0, false, kS},
    {"EffectKind1B_Trail", 0x46B9A0, 0xE8, kCalls46B9A0, SH_N(kCalls46B9A0), nullptr, 0, nullptr, 0, FC1_FN(EffectKind1B_Trail), 0, false, kS},
    {"EffectKind1B_Hit", 0x46BA90, 0x9F, kCalls46BA90, SH_N(kCalls46BA90), nullptr, 0, nullptr, 0, FC1_FN(EffectKind1B_Hit), 0xFF, false, kC},
    {"EffectKind30_Run", 0x46BB30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FC1_FN(EffectKind30_Run), 0, false, kS},
    {"EffectKind30_Start", 0x46BB50, 0x9F, kCalls46BB50, SH_N(kCalls46BB50), nullptr, 0, nullptr, 0, FC1_FN(EffectKind30_Start), 0, false, kS},
};
#undef FC1_FN
#undef SH_N

enum : unsigned {
    kRowLabel, k06Once, k06Fade, k19Run, k19Tick, k04Hold, k31Run, kZoomStart, kZoomStep, kZoomEnd, k32Run, k32Throw,
    k32Arc, k32Settle, kRelease, k37Start, k37Play, k14Start, k14Hold, k3CStart, k3CHold, k17Start, k17Push, k17Slide,
    k17Settle, k17Grow, k17Rest, k17Take, k17Camera, k17Bump, k17Align, k17Ahead, k17At, k17Cell, k1BStart, k1BFly,
    k1BEnd, k1BTrail, k1BHit, k30Run, k30Start, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// The tables our five dispatchers read in place (the harness swaps the entries
// for recorders while the fuzz runs). EffectKind30_States' four are the
// entries its states are known to set (symbols.toml).
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
const sh::DataTable kTables[] = {
    {Key(EffectKind19_States), 3}, {Key(EffectKind19_Ticks), 6}, {Key(CameraZoom_States), 3}, {Key(EffectKind32_States), 4},
    {Key(EffectKind30_States), 4},
};
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define FC1_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kAll = 0xFFFFFFFFu;

unsigned char* Cur() { return Sprite_Current; }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
// The same chosen by a hash (the disturbance and the stand-ins' effects draw
// only from the hash they are given, never the harness's Next(), or the two
// passes would draw differently).
template <typename... T> U PickBy(U h, T... v) {
    const U values[] = {static_cast<U>(v)...};
    return values[h % sizeof...(v)];
}
unsigned char* Eff(unsigned i) { return Effect_Objects + (i % at::kEffectCount) * at::kEffectStride; }

// --- the moves ---------------------------------------------------------------------
//
// What the functions read again after a call: Sprite_Current's counters and
// state (+6..+0xB), its speeds, brakes and fall (+0xC..+0x20), its position's
// fractions, its height word, its scale; Field_Kind2Hold; DamageScratch; the
// leader's height and point; the view's kind-2 point and elevation; Camera_
// Distance. Every index byte stays inside the records it indexes.
unsigned g_k;   // the function being fuzzed (Seed's k)

void Move(U h) {
    const U v = (h >> 8) & 0xFF;
    const U w = h >> 16;
    unsigned char* const o = Cur();
    if (!sh::InRegions(o, 0x80)) return;
    switch (h % 16) {
    case 0:
        // never 0 under CameraZoom_Start, which divides by it after its call (as CameraTurn_Start does before)
        o[9] = static_cast<unsigned char>(g_k == kZoomStart ? 1 + v % 0xFF : PickBy(w, 0, 1, 2, v));
        break;
    case 1: o[0xA] = static_cast<unsigned char>(PickBy(w, 0, 1, 2, v)); break;
    case 2: o[6 + v % 2] = static_cast<unsigned char>(w % 3); break;
    case 3: o[8] = static_cast<unsigned char>(w % 8); break;
    case 4: {
        static const unsigned char kOff[] = {0xC, 0x10, 0x14, 0x18, 0x1C, 0x20};
        SetLong(o + kOff[v % 6], static_cast<std::int32_t>(PickBy(h >> 24, 0, w, 0u - w, 0x1000, 0xFFFFF000u, 0x4000, 0x4001)));
        break;
    }
    case 5: SetWord(o + (v & 1 ? 0x34 : 0x38), w & 1 ? 0 : w); break;
    case 6: SetWord(o + 0x3E, w); break;
    case 7: Field_Kind2Hold = static_cast<unsigned char>(v & 1); break;
    case 8: Mem(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(v % 3 == 0 ? 0 : v); break;
    case 9: SetWord(ObjTrio + 0x3E, w); break;
    case 10: Field_Kind2X = Long(ObjTrio + 0x34); Field_Kind2Z = v & 1 ? Long(ObjTrio + 0x38) : static_cast<long>(w); break;
    case 11: Camera_Distance = static_cast<short>(w); break;
    case 12: SetLong(o + 0x40, static_cast<std::int32_t>(PickBy(v, 0x1C000, 0x1FFFF, 0x20000, w << 1))); break;
    case 13: o[0xB] = static_cast<unsigned char>(w % 20); break;
    case 14: o[3 + v % 2] = static_cast<unsigned char>(w % 20); break;
    default: MapView_Elevation = static_cast<long>(static_cast<short>(w)); break;
    }
}

std::uint32_t Stir(const std::uint32_t*, std::uint32_t answer) {
    Move(sh::Noise());
    return answer;
}

// AreaMap_Elevation near what its callers compare it with: Sprite_Current's
// height word, the leader's height + 0x40 (EffectKind1B_Hit), the view's
// elevation (EffectKind17_Slide) - at, a step off, 0x80 / 0x100 off.
std::uint32_t Ground(const std::uint32_t*, std::uint32_t answer) {
    const U n = sh::Noise();
    unsigned char* const o = Cur();
    U anchor;
    switch (n % 5) {
    case 0: anchor = sh::InRegions(o, 0x40) ? Word(o + 0x3E) : 0; break;
    case 1: anchor = Word(ObjTrio + 0x3E) + 0x40u; break;
    case 2: anchor = static_cast<U>(MapView_Elevation); break;
    case 3: anchor = sh::InRegions(o, 0x40) ? Word(o + 0x3E) - 0x80u : 0; break;
    default: Move(sh::Noise()); return answer;
    }
    static const int kDelta[] = {0, 0, 0, 1, -1, 0x7F, 0x80, -0x80, 0x100, 0x101, -0x100, 0x41, 0x3F};
    const int d = kDelta[(n >> 8) % (sizeof kDelta / sizeof kDelta[0])];
    return (answer & 0xFFFF0000u) | ((anchor + static_cast<U>(d)) & 0xFFFFu);
}

// AreaMap_ByteAt: the bytes its callers test (0x10, 0x11, 0x2x, 0xAx, 0xFx).
std::uint32_t CellByte(const std::uint32_t*, std::uint32_t answer) {
    const U n = sh::Noise();
    const U low = (n >> 8) & 0xF;
    const U b = PickBy(n >> 20, 0x10, 0x11, 0x20 | low, 0xA0 | low, 0xF0 | low, (n >> 12) & 0xFF);
    return (answer & 0xFFFFFF00u) | b;
}

// AreaMap_Slope: DamageScratch 0 (flat) or 1 (sloped), which its callers test
// straight after the call.
std::uint32_t Slope(const std::uint32_t*, std::uint32_t answer) {
    Mem(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(sh::Noise() % 3 == 0 ? 0 : 1);
    return answer;
}

// MoveCmd_Move reads bytes +0 and +4 of the object it is handed (EffectKind32_
// Throw's stack object); logged, then a move.
std::uint32_t Mover(const std::uint32_t* a, std::uint32_t answer) {
    const auto* const obj = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    sh::Note(obj[0], obj[4]);
    Move(sh::Noise());
    return answer;
}

// Sprite_FindFree: 0..29 or 0xFF (none) a third of the time.
std::uint32_t FreeSprite(const std::uint32_t*, std::uint32_t answer) {
    const U n = sh::Noise();
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 8) % at::kSpriteCount);
}
// Sprite_ObjectAt: 0..0x1D a record of Sprite_Objects, 0x1E..0x21 of
// Sprite_ObjectsExtra, 0xFF none.
std::uint32_t FoundObject(const std::uint32_t*, std::uint32_t answer) {
    const U n = sh::Noise();
    const U found = n % 3 == 0 ? 0xFFu : n % 4 == 1 ? 0x1E + (n >> 8) % 4 : (n >> 8) % 0x1E;
    return (answer & 0xFFFFFF00u) | found;
}
// Party_MemberAt: 0..2 or 0xFF.
std::uint32_t FoundMember(const std::uint32_t*, std::uint32_t answer) {
    const U n = sh::Noise();
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 8) % 3);
}
// EventOp_6x / _0x place a sprite and leave Sprite_Current on it (the starts
// put it back): moved half the time.
std::uint32_t Placed(const std::uint32_t*, std::uint32_t answer) {
    sh::Note(Word(Mem(bof3::addr::DamageScratch)));   // the object index it places, which it reads
    const U n = sh::Noise();
    if (n & 1) Sprite_Current = sh::SpriteRecord(n >> 1);
    return answer;
}

const sh::Callee kCallees[] = {
    // this group's own, called directly (E8): the void ones log the state they
    // ran with (kPhase), the others their arguments and answer
    {FC1_OURS(EffectKind32_Arc), 0, {}, sh::Answer::kPhase, 0, 0},
    {FC1_OURS(EffectKind17_TakeCell), 0, {}, sh::Answer::kPhase, 0, 0},
    {FC1_OURS(EffectKind17_CameraBack), 0, {}, sh::Answer::kPhase, 0, 0},
    {FC1_OURS(EffectKind17_Bump), 0, {}, kF, 0, 0, {}, &Stir},
    {FC1_OURS(EffectKind17_AlignTarget), 1, {0xFF}, kG, 0, 0, {}, &Stir},
    {FC1_OURS(EffectKind17_BlockedAhead), 0, {}, kF, 0, 0, {}, &Stir},
    {FC1_OURS(EffectKind17_BlockedAt), 2, {kAll, kAll}, kF, 0, 0, {}, &Stir},
    {FC1_OURS(EffectKind17_CellBlocked), 2, {0xFFFF, 0xFFFF}, kF, 0, 0, {}, &Stir},
    {FC1_OURS(EffectKind1B_Hit), 0, {}, kF, 0, 0, {}, &Stir},
    // group FC2's, by address until it merges (kPhase: void, no arguments)
    {"0x46D0E0", at::kFc2Aim, at::kFc2Aim, 0, {}, sh::Answer::kPhase, 0, 0},
    {"0x46BF80", at::kFc2Place, at::kFc2Place, 0, {}, sh::Answer::kPhase, 0, 0},
    // re-listed from the standard sets: the masks of what the callee reads
    // (the original pushes whole registers holding a word or a byte), answers
    // in range, and louder where the caller reads a cell after the call
    {FC1_OURS(Menu_DrawBox), 6, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},   // U16 x y w h, the flags' and colour's bytes (menu_windows.cpp)
    {FC1_OURS(AreaMap_SetByte), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0, {}, &Stir},       // S16 x z, the value's byte (map_field_objects.cpp)
    {FC1_OURS(AreaMap_ByteAt), 2, {0xFFFF, 0xFFFF}, sh::Answer::kByte, 0, 0xFF, {}, &CellByte},
    {FC1_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0, {}, &Ground},
    {FC1_OURS(AreaMap_Slope), 3, {kAll, kAll, 0xFF}, kG, 0, 0, {}, &Slope},             // n the direction's byte (area_slope.cpp)
    {"MoveCmd_Move", KeyOf(MoveCmd_Move), KeyOf(MoveCmd_Move), 2, {0, 0xFF}, kG, 0, 0, {}, &Mover},
    {FC1_OURS(Sprite_FindFree), 0, {}, sh::Answer::kByte, 0, 0x1D, {}, &FreeSprite},
    {FC1_OURS(Sprite_ObjectAt), 3, {kAll, kAll, kAll}, sh::Answer::kByte, 0, 0x21, {}, &FoundObject},
    {FC1_OURS(Party_MemberAt), 3, {kAll, kAll, kAll}, sh::Answer::kByte, 0, 2, {}, &FoundMember},
    {"EventOp_6x", 0x57AD10, KeyOf(EventOp_6x), 1, {kAll}, kG, 0, 0, {}, &Placed},
    {FC1_OURS(EventOp_0x), 1, {kAll}, kG, 0, 0, {16}, &Placed, nullptr, true},
};
#undef FC1_OURS

// --- the state ---------------------------------------------------------------------

// EffectKind30_Start's area descriptors: Area_Descriptors[0..3] pointed at
// these for the fuzz (and put back), each +8 at the entry list below, a
// region of its own (random every round; Sprite_InitFromEntry's deref hashes
// the entry).
constexpr unsigned kDescs = 4;
alignas(16) unsigned char g_entries[0x800];
alignas(16) unsigned char g_desc[kDescs][0x10];

// The cells EffectKind17_CellBlocked is handed (chosen, and the cell word
// planted, by the seed).
U g_cell[2];

// Beyond the harness's standard and field regions (which hold Sprite_Current,
// Sprite_Objects, Sprite_ObjectsExtra, Effect_Objects, ObjTrio, Camera_
// Distance and DamageScratch, Field_Request, Game_AreaNumber / MoveScript_
// FAWord, Field_Kind2Z / X, MapView_Redraw, MapView_Elevation, the menu block
// with Field_Kind2Hold, the style byte, the packet buffer and the area block).
const sh::Region kRegions[] = {
    {Key(&Game_Mode), 4},
    {Key(g_entries), sizeof g_entries},
};

void Disturb(U h) { Move(h); }

// A position whose whole part is a cell of the area block's 0x20 x 0x20, its
// fraction 0 a third of the time.
U Cell16() {
    const U cell = sh::Next() % 0x20;
    const U frac = sh::Often() ? PickOf(sh::Next() & 0xFFFF, 0x8000, 0x1000, sh::Next() & 0xFFFF) : 0;
    return cell << 16 | frac;
}

// The index bytes of the effect record every function might read, kept inside
// the records they index; its position on the area block's cells.
void SeedRecord(unsigned char* o) {
    o[3] = static_cast<unsigned char>(sh::Next() % 20);
    o[4] = static_cast<unsigned char>(sh::Next() % 20);
    o[6] = static_cast<unsigned char>(sh::Next() % at::kSpriteCount);
    o[0xB] = static_cast<unsigned char>(sh::Next() % 20);
    if (sh::Often()) o[8] = static_cast<unsigned char>(sh::Next() % 8);
    if (sh::Often()) o[2] = static_cast<unsigned char>(sh::Next() % 3);
    SetLong(o + 0x34, static_cast<std::int32_t>(Cell16()));
    SetLong(o + 0x38, static_cast<std::int32_t>(Cell16()));
}

// The area block's cell word for (x, z) - CellWord's index in field_c1.cpp -
// planted as 0 or as not 0.
void PlantCell(short cx, short cz, bool empty) {
    const int width = static_cast<int>(AreaMap_Header[0]);
    const int index = width * cz + static_cast<int>(Word(Mem(at::kAreaCells))) * 2 + cx;
    unsigned char* const w = Mem(Key(AreaMap_Header) + static_cast<U>(index) * 2u);
    if (!sh::InRegions(w, 2)) return;
    SetWord(w, empty ? 0u : 1u + sh::Next() % 0xFFFF);
}

std::int32_t Near(std::int32_t base) {
    return base + static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, 0x100, 0xFFFFFF00u, sh::Next() % 0x10000, sh::Next()));
}

void Seed(unsigned k) {
    g_k = k;
    // Sprite_Current: one of the first four sprite records (the harness's), or
    // an effect record half the time - the pool these handlers run on
    if (sh::Half()) Sprite_Current = Eff(sh::Next());
    unsigned char* const o = Cur();
    // the records a disturbance or EventOp's stand-in can move Sprite_Current
    // to (the first four sprite records) get index bytes inside the records too
    for (unsigned i = 0; i < 4; ++i) SeedRecord(sh::SpriteRecord(i));
    SeedRecord(o);
    ObjTrio[0xB] = static_cast<unsigned char>(sh::Next() % 20);
    if (sh::Half()) Field_Kind2Hold = 0;
    if (sh::Half()) Mem(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(sh::Half() ? 0 : 1);
    switch (k) {
    case kRowLabel:
        if (sh::Half()) Mem(0x929F02)[0] = 1;
        break;
    case k06Once:
    case k06Fade:
        if (sh::Half()) o[5] = 6;
        o[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
        break;
    case k19Run: o[1] = static_cast<unsigned char>(sh::Next() % 3); break;
    case k19Tick: o[6] = static_cast<unsigned char>(sh::Next() % 6); break;
    case k04Hold: o[9] = static_cast<unsigned char>(PickOf(0, 1, sh::Next())); break;
    case k31Run: o[1] = static_cast<unsigned char>(sh::Next() % 3); break;
    case kZoomStart:
        // +9 not 0: the original divides by it (ours aborts)
        o[9] = static_cast<unsigned char>(PickOf(1, 2, 0x10, 1 + sh::Next() % 0xFF));
        SetLong(o + 0xC, Near(Camera_Distance));
        break;
    case kZoomStep:
    case kZoomEnd: {
        const std::int32_t step = static_cast<std::int32_t>(PickOf(0, 0x10000, 0xFFFF0000u, 0x8000, 0xFFFF8000u, sh::Next()));
        SetLong(o + 0x10, step);
        SetLong(o + 0xC, Near(Camera_Distance + (step >> 16)));
        break;
    }
    case k32Run: o[1] = static_cast<unsigned char>(sh::Next() % 4); break;
    case k32Arc:
    case k32Settle:
        if (sh::Often()) o[0] = static_cast<unsigned char>(o[0] & 0x7F);
        if (sh::Half()) o[1] = 0;
        break;
    case k37Start: o[6] = static_cast<unsigned char>(sh::Next()); break;   // bit 7 into +0x2A, the rest the animation
    case k37Play:
        o[9] = static_cast<unsigned char>(PickOf(1, 2, sh::Next()));
        o[7] = static_cast<unsigned char>(PickOf(0, 1 + sh::Next() % 0xFF));
        if (sh::Half()) SetWord(o + 0x58, o[7]);
        break;
    case k14Hold:
    case k3CHold: o[9] = static_cast<unsigned char>(PickOf(0, 1, sh::Next())); break;
    case k17Push:
        if (sh::Often()) Game_Mode = static_cast<unsigned short>(PickOf(2, 3, 5, sh::Next()));
        else Game_Mode = 1;
        o[0xA] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
        if (sh::Half()) Field_Request = 5;
        break;
    case k17Slide: {
        // a speed and brake that land on 0x1000 (the brake's end) as well as around it
        const bool along_x = sh::Half();
        SetLong(o + 0xC, along_x ? static_cast<std::int32_t>(PickOf(0x1000, 0x800, 0xFFFFF000u, 0x4000, 0x1400, 0xFFFFEC00u, sh::Next())) : 0);
        SetLong(o + 0x10, static_cast<std::int32_t>(PickOf(0, 0x1000, 0xFFFFF800u, 0x4000, 0x1400, 0xFFFFEC00u, sh::Next())));
        SetLong(o + 0x18, static_cast<std::int32_t>(PickOf(0, 0xFFFFFC00u, 0x400, sh::Next())));
        SetLong(o + 0x1C, static_cast<std::int32_t>(PickOf(0, 0xFFFFFC00u, 0x400, sh::Next())));
        if (sh::Half()) {   // the view on the leader, the block at, around and past 0x40000 from it after its move
            const U axis = along_x ? 0x34 : 0x38, cross = along_x ? 0x38 : 0x34;
            const U speed = static_cast<U>(Long(o + (along_x ? 0xC : 0x10)));
            const U brake = static_cast<U>(Long(o + (along_x ? 0x18 : 0x1C)));
            U moved = speed + brake;
            const std::int32_t m = static_cast<std::int32_t>(moved);
            if ((m < 0 ? -static_cast<std::int64_t>(m) : m) < 0x1000) moved = speed;
            const U at = static_cast<U>(Long(o + axis)) + moved;
            SetLong(ObjTrio + axis, static_cast<std::int32_t>(at - PickOf(0x40000, 0x40001, 0x3FFFF, 0xFFFC0000u, 0x10000, 0x50000)));
            if (sh::Half()) SetLong(ObjTrio + cross, static_cast<std::int32_t>(at - PickOf(0x10000, 0x50000)));
            Field_Kind2X = Long(ObjTrio + 0x34);
            Field_Kind2Z = Long(ObjTrio + 0x38);
        }
        if (sh::Half()) SetWord(o + 0x34, 0);
        if (sh::Half()) SetWord(o + 0x38, 0);
        o[7] = static_cast<unsigned char>(sh::Next() % 4);
        break;
    }
    case k17Settle: {
        SetLong(o + 0x18, sh::Half() ? 0 : Near(Long(o + 0x34)));
        SetLong(o + 0xC, sh::Half() ? 0 : static_cast<std::int32_t>(PickOf(0x1000, 0xFFFFF000u, 0x4000, 0x4001, 0xFFFFBFFFu, sh::Next())));
        SetLong(o + 0x10, static_cast<std::int32_t>(PickOf(0, 0x1000, 0xFFFFF000u, 0x4000, 0x4001, sh::Next())));
        SetLong(o + 0x14, sh::Half() ? 0 : static_cast<std::int32_t>(sh::Next()));
        if (sh::Half()) {   // the target exactly where the step lands
            const bool along_x = Long(o + 0xC) != 0;
            SetLong(o + 0x18, Long(o + (along_x ? 0x34 : 0x38)) + Long(o + (along_x ? 0xC : 0x10)));
        }
        break;
    }
    case k17Grow:
    case k17Rest:
        o[9] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
        SetLong(o + 0x40, static_cast<std::int32_t>(PickOf(0x1C000, 0x1BFFF, 0x20000, sh::Next())));
        break;
    case k17Align:
        if (sh::Half()) SetWord(o + 0x34, 0);
        if (sh::Half()) SetWord(o + 0x38, 0);
        SetLong(o + 0xC, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, sh::Next())));
        SetLong(o + 0x10, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, sh::Next())));
        break;
    case k17At:
        SetLong(o + 0xC, static_cast<std::int32_t>(PickOf(0, 0, 1, 0xFFFFFFFFu, sh::Next())));
        SetLong(o + 0x10, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, sh::Next())));
        break;
    case k17Cell: {
        g_cell[0] = (sh::Next() & 0xFFFF0000u) | (sh::Next() % 0x20);
        g_cell[1] = (sh::Next() & 0xFFFF0000u) | (sh::Next() % 0x20);
        PlantCell(static_cast<short>(g_cell[0]), static_cast<short>(g_cell[1]), sh::Next() % 3 == 0);
        break;
    }
    case k1BStart: if (sh::Half()) ObjTrio[0x89] = 1; break;
    case k1BFly: {
        o[0xA] = static_cast<unsigned char>(PickOf(1, 2, 3, sh::Next()));
        SetLong(o + 0xC, static_cast<std::int32_t>(PickOf(0, 0x8000, 0xFFFF8000u, 0x10000)));
        SetLong(o + 0x10, static_cast<std::int32_t>(PickOf(0, 0x8000, 0xFFFF8000u, 0x10000)));
        const U x = static_cast<U>(Long(o + 0x34)) + static_cast<U>(Long(o + 0xC));
        const U z = static_cast<U>(Long(o + 0x38)) + static_cast<U>(Long(o + 0x10));
        PlantCell(static_cast<short>(x >> 16), static_cast<short>(z >> 16), sh::Next() % 3 == 0);
        break;
    }
    case k30Run: o[1] = static_cast<unsigned char>(sh::Next() % 4); break;
    case k30Start: Game_AreaNumber = static_cast<unsigned short>(sh::Next() % kDescs); break;
    default: break;
    }
}

// the arguments each kCall function reads (garbage above what it masks)
void Args(unsigned k, std::uint32_t* a) {
    switch (k) {
    case kRowLabel:
        a[0] = sh::Often() ? sh::Next() % 0x200 : sh::Next();
        a[1] = sh::Often() ? sh::Next() % 0x200 : sh::Next();
        a[2] = (a[2] & 0xFFFFFF00u) | (sh::Next() % 6);
        a[3] = (a[3] & 0xFFFFFF00u) | PickOf(3, 3, 0, 1, 2, 4, sh::Next() & 0xFF);
        break;
    case k17Align: a[0] = (a[0] & 0xFFFFFF00u) | PickOf(0, 0, 1, sh::Next() & 0xFF); break;
    case k17At:
        a[0] = Cell16();
        a[1] = Cell16();
        break;
    case k17Cell:
        a[0] = g_cell[0];
        a[1] = g_cell[1];
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // EffectKind30_Start's descriptors in Area_Descriptors[0..3] for the run
    unsigned char* saved[kDescs];
    for (unsigned i = 0; i < kDescs; ++i) {
        saved[i] = Area_Descriptors[i];
        std::memset(g_desc[i], 0, sizeof g_desc[i]);
        SetLong(g_desc[i] + 8, static_cast<std::int32_t>(Key(g_entries + 0x40 * i)));
        Area_Descriptors[i] = g_desc[i];
    }
    sh::Group group = {
        "field_c1", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 6000,
    };
    group.args = &Args;
    group.field = true;
    group.sprite_span = 20;   // +1..+4 below 20: every index byte inside Sprite_Objects / Effect_Objects
    sh::Run(group);
    for (unsigned i = 0; i < kDescs; ++i) Area_Descriptors[i] = saved[i];
}

}  // namespace field_c1
