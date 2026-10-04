// BOF3X_SHADOW=effect_1e: group E1E's 48 functions through the scenario
// harness's field mode (scenario_harness.h, docs/scenario_harness.md sections
// 7 and 8), once at start-up. docs/effect_1e.md section 4.
//
// The clone table is tools/band_rows.py --group E1E --clones --harness
// scenario (2026-09-29) with the names given; every extent agrees with the
// code (the cut's sizes are the catalog's, padding included). All 48 are
// kSprite: the leader's state 9 runs them with Sprite_Current the leader's
// ObjTrio record (Field_LeaderFrame), not an effect record, so the group is not
// in effect mode - its cells are listed here (docs/effect_1e.md section 4).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1e.h"
#include "game/effect_1e_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_1e {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Shape;
using U = std::uint32_t;

// band_rows.py's call sites (2026-09-29), each checked against the capstone read.
constexpr sh::CallSite kCalls528CD0[] = {{0x4, 0x52D610}, {0x1B, 0x587740}, {0x43, 0x587740}, {0x5B, 0x587740}, {0x85, 0x5905D0}, {0x8A, 0x52D5C0}, {0x9C, 0x469750}, {0xBF, 0x516B30}};
constexpr sh::CallSite kCalls528DA0[] = {{0x27, 0x52D610}, {0x65, 0x52D5C0}, {0x91, 0x469750}, {0xC5, 0x516B30}};
constexpr sh::CallSite kCalls528E70[] = {{0x23, 0x52D750}, {0x43, 0x52D5C0}};
constexpr sh::CallSite kCalls528EC0[] = {{0x4, 0x52D750}, {0x2A, 0x52D5C0}};
constexpr sh::CallSite kCalls528EF0[] = {{0x27, 0x52D750}, {0x3F, 0x587740}, {0x5B, 0x52D5C0}};
constexpr sh::CallSite kCalls528F70[] = {{0x10, 0x589330}};
constexpr sh::CallSite kCalls528FA0[] = {{0x1E, 0x587740}, {0x31, 0x589330}, {0x87, 0x5893A0}};
constexpr sh::CallSite kCalls529030[] = {{0x0, 0x5893A0}};
constexpr sh::CallSite kCalls529070[] = {{0xD0, 0x5893A0}};
constexpr sh::CallSite kCalls529170[] = {{0x47, 0x589330}};
constexpr sh::CallSite kCalls5291D0[] = {{0x1B, 0x52B2A0}, {0x39, 0x587740}, {0x74, 0x587740}, {0xA4, 0x5720C0}, {0xB5, 0x5B93D2}, {0x122, 0x5720C0}, {0x130, 0x52B2E0}, {0x135, 0x52B370}, {0x266, 0x589330}, {0x26E, 0x52B1B0}, {0x2EF, 0x5893A0}};
constexpr sh::CallSite kCalls529550[] = {{0x37, 0x587BE0}};
constexpr sh::CallSite kCalls5295A0[] = {{0xB, 0x587B40}, {0x14, 0x587AE0}, {0x1B, 0x52B2A0}, {0x42, 0x5893A0}};
constexpr sh::CallSite kCalls5295F0[] = {{0x58, 0x587740}, {0x5F, 0x587B40}, {0x68, 0x587AE0}, {0x6F, 0x589330}, {0xBC, 0x589330}, {0xE6, 0x5893A0}, {0x101, 0x589330}, {0x140, 0x589330}, {0x148, 0x529860}, {0x153, 0x52B1B0}, {0x166, 0x5298A0}, {0x16B, 0x529860}, {0x170, 0x5893A0}, {0x17B, 0x52B1B0}, {0x1B3, 0x589330}, {0x1C7, 0x52B1B0}, {0x208, 0x5298A0}, {0x225, 0x589330}, {0x256, 0x589330}, {0x25E, 0x52B1B0}};
constexpr sh::CallSite kCalls5298F0[] = {{0x4A, 0x587740}, {0x64, 0x5893A0}};
constexpr sh::CallSite kCalls529960[] = {{0x26, 0x52D080}, {0x6A, 0x587740}, {0x84, 0x5893A0}, {0x89, 0x52D5C0}};
constexpr sh::CallSite kCalls5299F0[] = {{0xC, 0x52D080}, {0x40, 0x52D0C0}, {0x60, 0x5893A0}, {0x65, 0x52D5C0}};
constexpr sh::CallSite kCalls529A60[] = {{0xC, 0x52D080}, {0x2D, 0x52D0C0}, {0x53, 0x52D140}, {0x6A, 0x5893A0}, {0x6F, 0x52D5C0}};
constexpr sh::CallSite kCalls529AE0[] = {{0x34, 0x52D080}, {0x55, 0x52D0C0}, {0x7B, 0x52D140}, {0xB2, 0x587740}, {0xBF, 0x587740}, {0xD9, 0x5893A0}, {0xDE, 0x52D5C0}};
constexpr sh::CallSite kCalls529BD0[] = {{0xC, 0x52D080}, {0x2D, 0x52D0C0}, {0x52, 0x52D140}, {0x75, 0x52D320}, {0x7D, 0x52D8C0}, {0x91, 0x5893A0}, {0x96, 0x52D5C0}};
constexpr sh::CallSite kCalls529C70[] = {{0x4, 0x52D080}, {0x25, 0x52D0C0}, {0x4A, 0x52D140}, {0x59, 0x52D320}, {0x61, 0x52D8C0}, {0xBB, 0x590BB0}, {0xEB, 0x587740}, {0xFB, 0x5893A0}, {0x100, 0x52D5C0}};
constexpr sh::CallSite kCalls529D80[] = {{0xC, 0x52D080}, {0x2D, 0x52D0C0}, {0x52, 0x52D140}, {0x61, 0x52D320}, {0xA9, 0x52D560}, {0xDD, 0x5893A0}, {0xE2, 0x52D5C0}};
constexpr sh::CallSite kCalls529E70[] = {{0x4, 0x52D080}, {0x25, 0x52D0C0}, {0x4A, 0x52D140}, {0x59, 0x52D320}, {0x67, 0x52D560}, {0x82, 0x5893A0}, {0x87, 0x52D5C0}};
constexpr sh::CallSite kCalls529F00[] = {{0x4, 0x52D080}, {0x25, 0x52D0C0}, {0x4A, 0x52D140}, {0x59, 0x52D320}, {0x67, 0x52D560}, {0x8B, 0x5893A0}, {0x90, 0x52D5C0}};
constexpr sh::CallSite kCalls529FA0[] = {{0x4, 0x52D080}, {0x25, 0x52D0C0}, {0x4A, 0x52D140}, {0x59, 0x52D320}, {0x67, 0x52D560}, {0x7E, 0x587740}, {0xAF, 0x587740}, {0xB9, 0x587740}, {0xE3, 0x5905D0}, {0xEB, 0x5893A0}, {0xF0, 0x52D5C0}};
constexpr sh::CallSite kCalls52A0A0[] = {{0x26, 0x52D080}, {0x59, 0x52D0C0}, {0x86, 0x52D140}, {0xA9, 0x52D320}, {0xF0, 0x52D560}, {0x142, 0x587B40}, {0x14B, 0x587AE0}, {0x155, 0x587740}, {0x15D, 0x5893A0}};
constexpr sh::CallSite kCalls52A230[] = {{0xC, 0x591B60}, {0x22, 0x52B200}};
constexpr sh::CallSite kCalls52A260[] = {{0xB, 0x52B2A0}, {0x1B, 0x5893A0}};
constexpr sh::CallSite kCalls52A280[] = {{0x2, 0x52B330}, {0xA, 0x5893A0}};
constexpr sh::CallSite kCalls52A2B0[] = {{0xB, 0x587B40}, {0x14, 0x587AE0}, {0x1E, 0x587740}, {0x25, 0x52B2A0}, {0x35, 0x5893A0}};
constexpr sh::CallSite kCalls52A310[] = {{0xB, 0x587B40}, {0x14, 0x587AE0}, {0x1E, 0x587740}, {0x25, 0x52B2A0}, {0x35, 0x5893A0}};
constexpr sh::CallSite kCalls52A370[] = {{0xC, 0x5A9949}, {0x25, 0x56F670}, {0x2A, 0x52CE20}, {0x31, 0x495040}, {0x4C, 0x589200}, {0x61, 0x5891F0}};
constexpr sh::CallSite kCalls52A420[] = {{0x0, 0x52D5C0}};
constexpr sh::CallSite kCalls52A440[] = {{0xB, 0x587BE0}, {0x15, 0x587740}};
constexpr sh::CallSite kCalls52A4A0[] = {{0xC, 0x461EB0}, {0x1E, 0x587740}, {0x4E, 0x587740}, {0x87, 0x587BA0}, {0x91, 0x587740}, {0xEF, 0x587740}, {0x10E, 0x587740}, {0x139, 0x587740}, {0x15B, 0x587740}, {0x171, 0x587740}, {0x1EA, 0x516B30}, {0x208, 0x5905D0}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E_FN(name) reinterpret_cast<const void*>(&::name)
#define E_CLONE(name, base, size, calls) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, E_FN(name), 0, false, Shape::kSprite
#define E_LEAF(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, E_FN(name), 0, false, Shape::kSprite
const sh::Clone kClones[] = {
    {E_CLONE(LeaderPanel_S1Choose, 0x528CD0, 0xC8, kCalls528CD0)},
    {E_CLONE(LeaderPanel_S1Out, 0x528DA0, 0xCE, kCalls528DA0)},
    {E_CLONE(LeaderPanel_S1Box2In, 0x528E70, 0x48, kCalls528E70)},
    {E_CLONE(LeaderPanel_S1Box2Wait, 0x528EC0, 0x2F, kCalls528EC0)},
    {E_CLONE(LeaderPanel_S1Box2Out, 0x528EF0, 0x60, kCalls528EF0)},
    {E_LEAF(LeaderPanel_S2, 0x528F50, 0x12)},
    {E_CLONE(LeaderPanel_S2Begin, 0x528F70, 0x22, kCalls528F70)},
    {E_CLONE(LeaderPanel_S2Wait, 0x528FA0, 0x8C, kCalls528FA0)},
    {E_CLONE(LeaderPanel_S2Anim, 0x529030, 0x37, kCalls529030)},
    {E_CLONE(LeaderPanel_S2Steer, 0x529070, 0xD5, kCalls529070)},
    {E_LEAF(LeaderPanel_S3, 0x529150, 0x12)},
    {E_CLONE(LeaderPanel_S3Begin, 0x529170, 0x58, kCalls529170)},
    {E_CLONE(LeaderPanel_S3Run, 0x5291D0, 0x2F6, kCalls5291D0)},
    {E_LEAF(LeaderPanel_S3Hold, 0x5294D0, 0x25)},
    {E_LEAF(LeaderPanel_S3End, 0x529500, 0x26)},
    {E_LEAF(LeaderPanel_S4, 0x529530, 0x12)},
    {E_CLONE(LeaderPanel_S4Begin, 0x529550, 0x4F, kCalls529550)},
    {E_CLONE(LeaderPanel_S4Music, 0x5295A0, 0x47, kCalls5295A0)},
    {E_CLONE(LeaderPanel_S4Run, 0x5295F0, 0x265, kCalls5295F0)},
    {E_LEAF(LeaderPanel_S4Pose, 0x529860, 0x3B)},
    {E_LEAF(LeaderPanel_S4Blink, 0x5298A0, 0x44)},
    {E_CLONE(LeaderPanel_S4Best, 0x5298F0, 0x69, kCalls5298F0)},
    {E_CLONE(LeaderPanel_S4HeaderIn, 0x529960, 0x8E, kCalls529960)},
    {E_CLONE(LeaderPanel_S4IconIn, 0x5299F0, 0x6A, kCalls5299F0)},
    {E_CLONE(LeaderPanel_S4RowIn, 0x529A60, 0x74, kCalls529A60)},
    {E_CLONE(LeaderPanel_S4Count, 0x529AE0, 0xE3, kCalls529AE0)},
    {E_CLONE(LeaderPanel_S4TotalIn, 0x529BD0, 0x9B, kCalls529BD0)},
    {E_CLONE(LeaderPanel_S4Take, 0x529C70, 0x105, kCalls529C70)},
    {E_CLONE(LeaderPanel_S4Result, 0x529D80, 0xE7, kCalls529D80)},
    {E_CLONE(LeaderPanel_S4ResultWait, 0x529E70, 0x8C, kCalls529E70)},
    {E_CLONE(LeaderPanel_S4AnyKey, 0x529F00, 0x95, kCalls529F00)},
    {E_CLONE(LeaderPanel_S4Again, 0x529FA0, 0xF5, kCalls529FA0)},
    {E_CLONE(LeaderPanel_S4Out, 0x52A0A0, 0x162, kCalls52A0A0)},
    {E_LEAF(LeaderPanel_S5, 0x52A210, 0x12)},
    {E_CLONE(LeaderPanel_UseItem, 0x52A230, 0x27, kCalls52A230)},
    {E_CLONE(LeaderPanel_S5Wait, 0x52A260, 0x20, kCalls52A260)},
    {E_CLONE(LeaderPanel_Leave, 0x52A280, 0xF, kCalls52A280)},
    {E_LEAF(LeaderPanel_S6, 0x52A290, 0x12)},
    {E_CLONE(LeaderPanel_S6Wait, 0x52A2B0, 0x3A, kCalls52A2B0)},
    {E_LEAF(LeaderPanel_S7, 0x52A2F0, 0x12)},
    {E_CLONE(LeaderPanel_S7Wait, 0x52A310, 0x3A, kCalls52A310)},
    {E_LEAF(LeaderPanel_S8, 0x52A350, 0x12)},
    {E_CLONE(LeaderPanel_S8Leave, 0x52A370, 0x74, kCalls52A370)},
    {E_LEAF(LeaderPanel_S8End, 0x52A3F0, 0x28)},
    {E_CLONE(LeaderPanel_S9, 0x52A420, 0x17, kCalls52A420)},
    {E_CLONE(LeaderPanel_S9Begin, 0x52A440, 0x3D, kCalls52A440)},
    {E_LEAF(LeaderPanel_S9Wait, 0x52A480, 0x12)},
    {E_CLONE(LeaderPanel_S9Menu, 0x52A4A0, 0x211, kCalls52A4A0)},
};
#undef E_LEAF
#undef E_CLONE
#undef E_FN
#undef SH_N

enum : unsigned {
    kS1Choose, kS1Out, kS1Box2In, kS1Box2Wait, kS1Box2Out, kS2, kS2Begin, kS2Wait, kS2Anim, kS2Steer, kS3, kS3Begin,
    kS3Run, kS3Hold, kS3End, kS4, kS4Begin, kS4Music, kS4Run, kS4Pose, kS4Blink, kS4Best, kS4HeaderIn, kS4IconIn,
    kS4RowIn, kS4Count, kS4TotalIn, kS4Take, kS4Result, kS4ResultWait, kS4AnyKey, kS4Again, kS4Out, kS5, kUseItem,
    kS5Wait, kLeave, kS6, kS6Wait, kS7, kS7Wait, kS8, kS8Leave, kS8End, kS9, kS9Begin, kS9Wait, kS9Menu, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define E_RAW(address) #address, address, address
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kAll = 0xFFFFFFFFu;

unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char* Sc() { return Sprite_Current; }

template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the stand-ins' effects (Noise() and the state only: the same on both passes) ---

// AreaMap_Elevation: two answers in three at effect record 0's kept height
// +0x3E, give or take one (stage 3 compares the two as words).
U ElevationEffect(const U*, U answer) {
    const U h = sh::Noise();
    if (h % 3 == 0) return answer;
    return (answer & 0xFFFF0000u) | ((Word(Mem(at::kEff0Height)) + (h >> 4) % 3 - 1) & 0xFFFFu);
}
// 0x52B2E0: the sloped byte 0x903850 it writes, which stage 3 reads after it.
U HoldTestEffect(const U*, U answer) {
    Mem(0x903850)[0] = static_cast<unsigned char>(sh::Noise() & 1);
    return answer;
}
// 0x52B370: record 0's +0xB and the pose, which stage 3 reads after it (the
// original's 0x52B460 moves the effect records).
U EffectsStepEffect(const U*, U answer) {
    const U h = sh::Noise();
    if (h & 1) Mem(at::kEff0Hold)[0] = static_cast<unsigned char>(h & 2 ? 0 : h >> 8);
    if (h & 4) Mem(at::kPose)[0] = static_cast<unsigned char>((h >> 16) % 3);
    return answer;
}

const sh::Callee kCallees[] = {
    // this group's own, called directly (E8) by LeaderPanel_S4Run
    {E_OURS(LeaderPanel_S4Pose), 0, {}, sh::Answer::kPhase, 0, 0},
    {E_OURS(LeaderPanel_S4Blink), 0, {}, sh::Answer::kPhase, 0, 0},
    // FE1's panel draws (the effect-standard rows, which this field-mode group
    // does not register), with the width each reads (docs/effect_1e.md
    // section 3): x and y reach 0x52CFE0's movsx and Text_DrawAt's words, the
    // kind, count and row are bytes, the message id a word
    {E_OURS(FieldPanel_DrawBox3), 2, {kAll, kAll}, kG, 0, 0},
    {E_OURS(FieldPanel_DrawBox2), 2, {kAll, kAll}, kG, 0, 0},
    {E_OURS(FieldPanel_DrawHeader), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {E_OURS(FieldPanel_DrawKindIcon), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},
    {E_OURS(FieldPanel_DrawKindRow), 3, {0xFF, 0xFF, 0xFF}, kG, 0, 0},
    {E_OURS(FieldPanel_DrawTotal), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {E_OURS(FieldPanel_DrawMessage), 3, {0xFFFF, 0xFFFF, 0xFFFF}, kG, 0, 0},
    {E_OURS(FieldPanel_DrawShade), 0, {}, kG, 0, 0},
    {E_OURS(FieldPanel_DrawBlink), 0, {}, kG, 0, 0},
    // standard ones re-listed with the width the callee reads
    {E_OURS(Text_DrawAt), 5, {0xFFFF, 0xFFFF, 0xFF, kAll, kAll}, kG, 0, 0},   // x, y words (0x516B30), colour & 0xFF
    {E_OURS(Inventory_Remove), 3, {0xFF, 0xFF, 0xFF}, kF, 0, 0},              // 0x591B60 reads three bytes
    {E_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0, {}, &ElevationEffect},
    // E1B's and E1F's, by address (docs/effect_1e.md section 8)
    {"0x469750", at::kBoxPrims, at::kBoxPrims, 5, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},   // 0x469790 / 0x469960 read x, y, w, h & 0xFFFF
    {"0x52CE20", at::kClearEffects, at::kClearEffects, 0, {}, kG, 0, 0},
    // nobody's, by address
    {E_RAW(0x52B2A0), 1, {0xFF}, kG, 0, 0},
    {E_RAW(0x52B1B0), 0, {}, kG, 0, 0},
    {E_RAW(0x52B200), 0, {}, kG, 0, 0},
    {E_RAW(0x52B2E0), 0, {}, kG, 0, 0, {}, &HoldTestEffect},
    {E_RAW(0x52B370), 0, {}, kG, 0, 0, {}, &EffectsStepEffect},
    {E_RAW(0x52B330), 1, {0xFFFF}, kF, 0, 0},   // and eax, edx; test ax, ax
};
#undef E_RAW
#undef E_OURS

// The steps tables the dispatchers read in place, swapped for recorders on both
// sides; each count is the run of code pointers to the next table a dispatcher
// indexes (docs/effect_1e.md section 2).
const sh::DataTable kTables[] = {
    {Key(LeaderPanel_Stage2Steps), LeaderPanel_Stage2Steps_count},
    {Key(LeaderPanel_Stage3Steps), LeaderPanel_Stage3Steps_count},
    {Key(LeaderPanel_Stage4Steps), LeaderPanel_Stage4Steps_count},
    {Key(LeaderPanel_Stage5Steps), LeaderPanel_Stage5Steps_count},
    {Key(LeaderPanel_Stage6Steps), LeaderPanel_Stage6Steps_count},
    {Key(LeaderPanel_Stage7Steps), LeaderPanel_Stage7Steps_count},
    {Key(LeaderPanel_Stage8Steps), LeaderPanel_Stage8Steps_count},
    {Key(LeaderPanel_Stage9Steps), LeaderPanel_Stage9Steps_count},
};

// Beyond field mode's standard regions: the cells 0x939A00.. (the sprite
// index, the two record pointers, the blink), 0x6BC700.. (the pose bytes, the
// counter), MessagePools (the offset words the prompts read), Game_Mode /
// Game_Step, Field_MenuButton and the confirm / cancel words after it.
const sh::Region kRegions[] = {
    {0x939A00, 0x30},
    {0x6BC700, 0x20},
    {at::kPools, 0xE8},
    {0x66C7E8, 4},
    {0x903584, 0x10},
};

// --- the seed ---------------------------------------------------------------------------

unsigned char Small() { return static_cast<unsigned char>(PickOf(0, 0, 1, 1, 2, 3, 4, 5, 8, sh::Next())); }

// The steps each dispatcher's table holds.
unsigned Entries(unsigned k) {
    switch (k) {
    case kS2: return LeaderPanel_Stage2Steps_count;
    case kS3: return LeaderPanel_Stage3Steps_count;
    case kS4: return LeaderPanel_Stage4Steps_count;
    case kS5: return LeaderPanel_Stage5Steps_count;
    case kS6: return LeaderPanel_Stage6Steps_count;
    case kS7: return LeaderPanel_Stage7Steps_count;
    case kS8: return LeaderPanel_Stage8Steps_count;
    case kS9: return LeaderPanel_Stage9Steps_count;
    default: return 0;
    }
}

void SeedRecord(unsigned char* s) {
    s[2] = static_cast<unsigned char>(sh::Next());
    s[3] = static_cast<unsigned char>(sh::Next() % 5);   // below the smallest span a dispatcher reads after a call (stage 9's)
    s[6] = static_cast<unsigned char>(PickOf(0, 1, 0, 1, sh::Next()));
    s[7] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    s[9] = Small();
    s[0xB] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
    SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0x8000, 0x2000, 0xA000, sh::Next())));
    SetLong(s + 0x38, static_cast<std::int32_t>(sh::Next()));
    SetWord(s + 0x58, PickOf(6, 6, sh::Next()));
}

void Seed(unsigned k) {
    // the leader (Field_LeaderFrame runs ObjTrio's first record); the
    // disturbance moves Sprite_Current among the first four sprite records
    Sprite_Current = sh::Often() ? sh::ObjectOf(0) : sh::SpriteRecord(sh::Next());
    for (unsigned i = 0; i < 3; ++i) SeedRecord(sh::ObjectOf(i));
    for (unsigned i = 0; i < 4; ++i) SeedRecord(sh::SpriteRecord(i));
    unsigned char* const s = Sc();
    if (const unsigned n = Entries(k)) s[3] = static_cast<unsigned char>(sh::Next() % n);
    // the picked sprite record, its kind below 0x74 (the best counts 0x9040EC +
    // kind stay in the save block's region) and its count near +9
    const unsigned pick = sh::Next() % at::kSpriteCount;
    Mem(at::kSpriteIndex)[0] = static_cast<unsigned char>(pick);
    unsigned char* const p = Sprite_Objects + pick * at::kSpriteStride;
    p[6] = static_cast<unsigned char>(PickOf(0x16, 0x16, sh::Next() % 0x16, sh::Next() % 0x16, sh::Next() % 0x74));
    SetWord(p + 0x9C, PickOf(s[9], s[9] + 1u, s[9] - 1u, sh::Next() & 0xFF, sh::Next()));
    Mem(at::kBestCounts + p[6])[0] = static_cast<unsigned char>(PickOf(p[0x9C], p[0x9C] - 1u, p[0x9C] + 1u, 0, sh::Next()));
    // the record pointers into the scratch buffers
    sh::SetPointer(at::kRecordA, sh::Scratch(0) + sh::Next() % 0x30);
    sh::SetPointer(at::kRecordB, sh::Scratch(1) + sh::Next() % 0x28);
    unsigned char* const a = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(at::kRecordA)))));
    unsigned char* const b = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(at::kRecordB)))));
    a[3] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
    b[0x11] = static_cast<unsigned char>(PickOf(0, 1, 8, 0xF, 0x10, sh::Next()));
    Mem(at::kBlink)[0] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    // the effect records' bytes the stages compare
    Mem(at::kEff0State)[0] = static_cast<unsigned char>(PickOf(3, 3, 0, sh::Next()));
    Mem(at::kEff3State)[0] = static_cast<unsigned char>(PickOf(0, 0, 4, 6, sh::Next()));
    Mem(at::kEff4State)[0] = static_cast<unsigned char>(PickOf(0, 3, 4, 3, sh::Next()));
    Mem(at::kEff6State)[0] = static_cast<unsigned char>(PickOf(3, 4, 8, 0xE, 5, sh::Next()));
    Mem(at::kEff6Choice)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0, 0xFF, sh::Next()));
    Mem(at::kEff0Hold)[0] = static_cast<unsigned char>(PickOf(0, 0, sh::Next()));
    SetLong(Mem(at::kEff0StepZ), static_cast<std::int32_t>(PickOf(0, 0, sh::Next())));
    SetLong(Mem(at::kEff0StepY), static_cast<std::int32_t>(PickOf(0, 0x110000, 0x110001, 0x114000, 0x100, 0xFFFFFFFFu, 0x2000, sh::Next())));
    SetLong(Mem(at::kEff0StepX), static_cast<std::int32_t>(PickOf(0xFFFFFFFFu, 0, 1, 0x800, sh::Next())));
    SetLong(Mem(at::kEff0X), static_cast<std::int32_t>(PickOf(0x60000, 0x60001, 0x80000, 0x80001, 0x120000, 0x11FFFF, 0x150000, 0x14FFFF, sh::Next())));
    const U z = PickOf(0x3E8000, 0x3E7FFF, 0x150000, 0x14FFFF, 0x3C0000, 0x3C0001, 0x200000, sh::Next());
    SetLong(Mem(at::kEff0Z), static_cast<std::int32_t>(z));
    Field_Kind2Z = static_cast<long>(PickOf(z, z - 1, z + 1, sh::Next()));
    SetWord(Mem(at::kEff0Height), sh::Next());
    Frame_Counter = sh::Half() ? sh::Next() & ~0xFu : sh::Next();
    Mem(0x903850)[0] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    // the pad: the bits the stages test, often alone, the buttons' words overlapping them
    Input_Pressed = static_cast<unsigned short>(PickOf(0x20, 0x40, 0x60, 0x1000, 0x4000, 0x5000, 0x8000, 0x2000, 0xC000, 0, sh::Next()));
    Input_Held = static_cast<unsigned short>(PickOf(0x20, 0x8000, 0x2000, 0xA000, 0x4000, 0xC000, 0x1000, 0x3000, 0, sh::Next()));
    Field_ConfirmButtons = static_cast<unsigned short>(PickOf(0x40, 0x43, 0x20, sh::Next()));
    Field_CancelButtons = static_cast<unsigned short>(PickOf(0x10, 0x20, 0x40, sh::Next()));
    MoveScript_WaitWordDA = static_cast<unsigned short>(PickOf(0, 0, sh::Next()));
    Field_Kind2Hold = static_cast<unsigned char>(PickOf(0, 0, sh::Next()));
    Field_State[0x89] = static_cast<unsigned char>(PickOf(0, sh::Next()));
    sh::SetRandHint(PickOf(0, 1, 8, 0xF, sh::Next()));
}

// --- the disturbance: a cell these read again after a call ------------------------------
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 24);
    switch (h % 12) {
    case 0: Mem(at::kEff4State)[0] = static_cast<unsigned char>(b % 5); break;
    case 1: Mem(0x903850)[0] = static_cast<unsigned char>(b & 1); break;
    case 2: Mem(at::kEff0Hold)[0] = static_cast<unsigned char>(b & 1 ? 0 : b); break;
    case 3: Mem(at::kPose)[0] = static_cast<unsigned char>(b % 3); break;
    case 4: Sc()[9] = static_cast<unsigned char>(b % 9); break;
    case 5: Sc()[0xB] = static_cast<unsigned char>(b % 3); break;
    case 6: Sc()[6] = static_cast<unsigned char>(b & 1); break;
    case 7: Mem(at::kEff6Choice)[0] = static_cast<unsigned char>(b % 4); break;
    case 8: {
        static const unsigned char kStates[] = {3, 4, 8, 0xE};
        Mem(at::kEff6State)[0] = kStates[(h >> 8) % 4];
        break;
    }
    case 9: SetLong(Mem(at::kEff0StepY), static_cast<std::int32_t>(b & 1 ? 0u : h >> 4)); break;
    case 10: Mem(at::kEff3State)[0] = static_cast<unsigned char>(b & 1 ? 0 : b); break;
    case 11: Mem(at::kSpriteIndex)[0] = static_cast<unsigned char>(b % at::kSpriteCount); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    sh::Group g = {"effect_1e", kClones, kCount, kCallees, sizeof kCallees / sizeof kCallees[0],
                   kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   &Seed, &Disturb, 3000};
    g.field = true;
    g.sprite_span = 5;   // +1..+4 below stage 9's five, which its dispatcher reads after a call
    sh::Run(g);
}

}  // namespace effect_1e
