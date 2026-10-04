// BOF3X_SHADOW=effect_6b: group E6B's 50 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_6b.md section 5. BOF3X_E6B_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E6B --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; every extent is the tool's. Shapes: the nine sub-state
// dispatchers, the states and the two draws without arguments kEffect
// (Sprite_Current one of the 20 Effect_Objects records, +5 0x18, a
// dispatcher's +2 below its table's length); the two draws with arguments
// kCall. No function answers.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6b.h"
#include "game/effect_6b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_6b {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E6B --clones, 2026-10-03.
constexpr sh::CallSite kCalls50E420[] = {{0x113, 0x50E1C0}};
constexpr sh::CallSite kCalls50E540[] = {{0xC7, 0x587740}, {0xD8, 0x50E1C0}};
constexpr sh::CallSite kCalls50E620[] = {{0x1A, 0x50E1C0}};
constexpr sh::CallSite kCalls50E640[] = {{0x64, 0x50E1C0}, {0xBB, 0x50E1C0}};
constexpr sh::CallSite kCalls50E710[] = {{0x24, 0x587740}, {0x36, 0x50E1C0}};
constexpr sh::CallSite kCalls50E770[] = {{0x113, 0x50E1C0}};
constexpr sh::CallSite kCalls50E890[] = {{0xC7, 0x587740}, {0xD8, 0x50E1C0}};
constexpr sh::CallSite kCalls50E970[] = {{0x1A, 0x50E1C0}};
constexpr sh::CallSite kCalls50E990[] = {{0x64, 0x50E1C0}, {0xBB, 0x50E1C0}};
constexpr sh::CallSite kCalls50EA60[] = {{0x24, 0x587740}, {0x36, 0x50E1C0}};
constexpr sh::CallSite kCalls50EB40[] = {{0x25, 0x587740}, {0x35, 0x50E1C0}};
constexpr sh::CallSite kCalls50EBA0[] = {{0x113, 0x50EED0}};
constexpr sh::CallSite kCalls50ECC0[] = {{0xC7, 0x587740}, {0xD8, 0x50EED0}};
constexpr sh::CallSite kCalls50EDA0[] = {{0x1A, 0x50EED0}};
constexpr sh::CallSite kCalls50EDC0[] = {{0x64, 0x50EED0}, {0xBB, 0x50EED0}};
constexpr sh::CallSite kCalls50EE90[] = {{0x24, 0x587740}, {0x36, 0x50EED0}};
constexpr sh::CallSite kCalls50EED0[] = {{0x17, 0x5A77C0},  {0x20, 0x461E50},  {0x2C, 0x5A75D0},  {0x34, 0x5A77A0},
                                         {0x13E, 0x5720C0}, {0x15B, 0x5720C0}, {0x198, 0x5720C0}, {0x1B5, 0x5720C0},
                                         {0x1FE, 0x5A85F0}, {0x207, 0x5A9290}, {0x223, 0x572A00}, {0x22C, 0x461E50}};
constexpr sh::CallSite kCalls50F1F0[] = {{0x25, 0x587740}, {0x35, 0x50EED0}};
constexpr sh::CallSite kCalls50F250[] = {{0x126, 0x50F590}};
constexpr sh::CallSite kCalls50F380[] = {{0xC7, 0x587740}, {0xD8, 0x50F590}};
constexpr sh::CallSite kCalls50F460[] = {{0x1A, 0x50F590}};
constexpr sh::CallSite kCalls50F480[] = {{0x64, 0x50F590}, {0xBB, 0x50F590}};
constexpr sh::CallSite kCalls50F550[] = {{0x24, 0x587740}, {0x36, 0x50F590}};
constexpr sh::CallSite kCalls50F590[] = {{0x97, 0x5A77C0},  {0xA0, 0x461E50},  {0xB6, 0x5A75D0},  {0xBE, 0x5A77A0},
                                         {0x196, 0x5A85F0}, {0x19C, 0x5A9290}, {0x1BB, 0x572A00}, {0x1C4, 0x461E50}};
constexpr sh::CallSite kCalls50F780[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x27, 0x5A75B0}, {0x2F, 0x5A7780}, {0x78, 0x461E50}};
constexpr sh::CallSite kCalls50F820[] = {{0x4C, 0x5A7A00}, {0x68, 0x5A7A00}, {0x89, 0x5A7A00}, {0xA9, 0x5A7A00}, {0x1B8, 0x5720C0}};
constexpr sh::CallSite kCalls50FA20[] = {{0x17, 0x587740}};
constexpr sh::CallSite kCalls50FA50[] = {{0x13, 0x50FAE0}};
constexpr sh::CallSite kCalls50FA70[] = {{0xE, 0x587740}, {0x18, 0x587740}, {0x2A, 0x50FAE0}};
constexpr sh::CallSite kCalls50FAB0[] = {{0x16, 0x589840}, {0x27, 0x50FAE0}};
constexpr sh::CallSite kCalls50FAE0[] = {{0x3B, 0x5A77C0},  {0x44, 0x461E50},  {0x68, 0x5A75D0},  {0x6F, 0x5A77A0},
                                         {0x77, 0x5A7780},  {0x1DE, 0x5A85F0}, {0x1E4, 0x5A9290}, {0x1ED, 0x461E50},
                                         {0x230, 0x5A77C0}, {0x239, 0x461E50}};
constexpr sh::CallSite kCalls50FD60[] = {{0x1E, 0x50FF10}};
constexpr sh::CallSite kCalls50FD90[] = {{0x31, 0x50FF10}, {0x59, 0x57C140}, {0x71, 0x587740}};
constexpr sh::CallSite kCalls50FE20[] = {{0x2D, 0x50FF10}, {0x50, 0x57C0F0}, {0x59, 0x572650}, {0x71, 0x57C140},
                                         {0xA8, 0x5100B0}, {0xBC, 0x5101C0}, {0xDD, 0x589840}};
constexpr sh::CallSite kCalls50FF10[] = {{0x17, 0x5A77C0},  {0x20, 0x461E50},  {0x4F, 0x5A7B90},  {0x63, 0x5A8200},
                                         {0x72, 0x5A8060},  {0x86, 0x5A7D70},  {0x90, 0x5A8DE0},  {0x9D, 0x5A8E00},
                                         {0xB9, 0x5A75D0},  {0xC1, 0x5A77A0},  {0x13C, 0x5A85F0}, {0x142, 0x5A9290},
                                         {0x171, 0x572A00}, {0x17A, 0x461E50}, {0x18D, 0x5A7BC0}};
constexpr sh::CallSite kCalls510C40[] = {{0x5, 0x589590}, {0x15, 0x5891F0}};
constexpr sh::CallSite kCalls510C80[] = {{0x0, 0x510C90}, {0x5, 0x510EB0}};

#define E6B_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E6B_CALLS(a) a, E6B_N(a)
#define E6B_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub33_Run", 0x50E400, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub33_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub33_Place", 0x50E420, 0x11A, E6B_CALLS(kCalls50E420), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub33_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub33_WaitNear", 0x50E540, 0xDF, E6B_CALLS(kCalls50E540), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub33_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub33_Open", 0x50E620, 0x1F, E6B_CALLS(kCalls50E620), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub33_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub33_WaitFar", 0x50E640, 0xC2, E6B_CALLS(kCalls50E640), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub33_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub33_Close", 0x50E710, 0x3B, E6B_CALLS(kCalls50E710), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub33_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub34_Run", 0x50E750, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub34_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub34_Place", 0x50E770, 0x11A, E6B_CALLS(kCalls50E770), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub34_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub34_WaitNear", 0x50E890, 0xDF, E6B_CALLS(kCalls50E890), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub34_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub34_Open", 0x50E970, 0x1F, E6B_CALLS(kCalls50E970), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub34_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub34_WaitFar", 0x50E990, 0xC2, E6B_CALLS(kCalls50E990), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub34_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub34_Close", 0x50EA60, 0x3B, E6B_CALLS(kCalls50EA60), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub34_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub37_Run", 0x50EAA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub37_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub37_Place", 0x50EAC0, 0x5A, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub37_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub37_Hold", 0x50EB20, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub37_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub37_Close", 0x50EB40, 0x3A, E6B_CALLS(kCalls50EB40), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub37_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub35_Run", 0x50EB80, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub35_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub35_Place", 0x50EBA0, 0x11A, E6B_CALLS(kCalls50EBA0), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub35_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub35_WaitNear", 0x50ECC0, 0xDF, E6B_CALLS(kCalls50ECC0), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub35_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub35_Open", 0x50EDA0, 0x1F, E6B_CALLS(kCalls50EDA0), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub35_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub35_WaitFar", 0x50EDC0, 0xC2, E6B_CALLS(kCalls50EDC0), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub35_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub35_Close", 0x50EE90, 0x3B, E6B_CALLS(kCalls50EE90), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub35_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub35_Draw", 0x50EED0, 0x23B, E6B_CALLS(kCalls50EED0), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub35_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub38_Run", 0x50F110, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub38_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub38_Place", 0x50F130, 0xBD, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub38_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub38_Close", 0x50F1F0, 0x3A, E6B_CALLS(kCalls50F1F0), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub38_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3B_Run", 0x50F230, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub3B_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub3B_Place", 0x50F250, 0x12D, E6B_CALLS(kCalls50F250), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub3B_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3B_WaitNear", 0x50F380, 0xDF, E6B_CALLS(kCalls50F380), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub3B_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3B_Open", 0x50F460, 0x1F, E6B_CALLS(kCalls50F460), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub3B_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3B_WaitFar", 0x50F480, 0xC2, E6B_CALLS(kCalls50F480), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub3B_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3B_Close", 0x50F550, 0x3B, E6B_CALLS(kCalls50F550), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub3B_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3B_Draw", 0x50F590, 0x1E2, E6B_CALLS(kCalls50F590), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub3B_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub54_Pulse", 0x50F780, 0x9C, E6B_CALLS(kCalls50F780), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub54_Pulse), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub3D_Ripple", 0x50F820, 0x1D5, E6B_CALLS(kCalls50F820), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub3D_Ripple), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub40_Run", 0x50FA00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub40_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub40_WaitCue", 0x50FA20, 0x28, E6B_CALLS(kCalls50FA20), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub40_WaitCue), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub40_WaitOne", 0x50FA50, 0x1A, E6B_CALLS(kCalls50FA50), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub40_WaitOne), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub40_WaitTwo", 0x50FA70, 0x31, E6B_CALLS(kCalls50FA70), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub40_WaitTwo), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub40_Move", 0x50FAB0, 0x2E, E6B_CALLS(kCalls50FAB0), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub40_Move), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub40_Draw", 0x50FAE0, 0x249, E6B_CALLS(kCalls50FAE0), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub40_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub41_Run", 0x50FD30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub41_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub41_Start", 0x50FD50, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub41_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub41_WaitOne", 0x50FD60, 0x27, E6B_CALLS(kCalls50FD60), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub41_WaitOne), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub41_Turn", 0x50FD90, 0x8B, E6B_CALLS(kCalls50FD90), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub41_Turn), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub41_Hold", 0x50FE20, 0xE3, E6B_CALLS(kCalls50FE20), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub41_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub41_Draw", 0x50FF10, 0x19A, E6B_CALLS(kCalls50FF10), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub41_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub44_Run", 0x510C20, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub44_Run), 0, false, kEf, 0, 0, 2, 0x18},
    {"EffectKind18Sub44_Start", 0x510C40, 0x3A, E6B_CALLS(kCalls510C40), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub44_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub44_Step", 0x510C80, 0xA, E6B_CALLS(kCalls510C80), nullptr, 0, nullptr, 0, E6B_FN(EffectKind18Sub44_Step), 0, false, kEf, 0, 0, 0, 0x18},
};
#undef E6B_FN
#undef E6B_CALLS
#undef E6B_N

enum : unsigned {
    k33Run, k33Place, k33WaitNear, k33Open, k33WaitFar, k33Close,
    k34Run, k34Place, k34WaitNear, k34Open, k34WaitFar, k34Close,
    k37Run, k37Place, k37Hold, k37Close,
    k35Run, k35Place, k35WaitNear, k35Open, k35WaitFar, k35Close, k35Draw,
    k38Run, k38Place, k38Close,
    k3BRun, k3BPlace, k3BWaitNear, k3BOpen, k3BWaitFar, k3BClose, k3BDraw,
    k54Pulse, k3DRipple,
    k40Run, k40WaitCue, k40WaitOne, k40WaitTwo, k40Move, k40Draw,
    k41Run, k41Start, k41WaitOne, k41Turn, k41Hold, k41Draw,
    k44Run, k44Start, k44Step, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

unsigned g_k;   // the clone being fuzzed (Seed's k)

// --- the effects ---------------------------------------------------------------------

// The draws without arguments read Sprite_Current's record: its address and
// the words they read (+0x30 the slide, +0x34..+0x3B the point and cell,
// +0x3C..+0x3F the lift) and +8 logged, so a draw on the wrong record, or
// before a state's write it should follow, shows. E6A's 0x50E1C0 (read to its
// last instruction for this: EffectKind18Sub35_Draw's shape, reading +8,
// +0x30, +0x36, +0x3A) alike.
U FxCurrent(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    if (sh::InRegions(s, 0x80))
        sh::Note(Key(s) ^ s[8], static_cast<U>(Long(s + 0x30)), static_cast<U>(Long(s + 0x34)),
                 static_cast<U>(Long(s + 0x38)) ^ static_cast<U>(Long(s + 0x3C)));
    return answer;
}

#define E6B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define E6B_RAW(address) #address, address, address
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own draws called (or tail-jumped to) directly, by name: no
    // argument (logging the record's words), or whole words (the step is
    // multiplied and shifted; the variant indexes, the angle is negated and
    // multiplied)
    {E6B_OURS(EffectKind18Sub35_Draw), 0, {}, kG, 0, 0, {}, &FxCurrent},
    {E6B_OURS(EffectKind18Sub3B_Draw), 0, {}, kG, 0, 0, {}, &FxCurrent},
    {E6B_OURS(EffectKind18Sub40_Draw), 1, {kW}, kG, 0, 0},
    {E6B_OURS(EffectKind18Sub41_Draw), 2, {kW, kW}, kG, 0, 0},
    // group E6A's (wave six), raw: void, on Sprite_Current
    {"0x50E1C0", at::kE6ADraw, at::kE6ADraw, 0, {}, kG, 0, 0, {}, &FxCurrent},
    // group E6C's (wave six), raw: void
    {"0x510C90", at::kE6CStep, at::kE6CStep, 0, {}, kPh, 0, 0},
    {"0x510EB0", at::kE6CTail, at::kE6CTail, 0, {}, kPh, 0, 0},
};
#undef E6B_RAW
#undef E6B_OURS

// The sub-state tables the dispatchers jump through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {Key(EffectKind18Sub33_States), EffectKind18Sub33_States_count},
    {Key(EffectKind18Sub34_States), EffectKind18Sub34_States_count},
    {Key(EffectKind18Sub37_States), EffectKind18Sub37_States_count},
    {Key(EffectKind18Sub35_States), EffectKind18Sub35_States_count},
    {Key(EffectKind18Sub38_States), EffectKind18Sub38_States_count},
    {Key(EffectKind18Sub3B_States), EffectKind18Sub3B_States_count},
    {Key(EffectKind18Sub40_States), EffectKind18Sub40_States_count},
    {Key(EffectKind18Sub41_States), EffectKind18Sub41_States_count},
    {Key(EffectKind18Sub44_States), EffectKind18Sub44_States_count},
};
const std::uint8_t kKinds[] = {0x18};

// Beyond effect mode's standard regions: none. The leader (ObjTrio), the
// chapter bytes, Cond_ByteFE, Field_Kind2Z / X and MapView_Redraw, the area
// block, the story flags and the records are standard.

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// A 16.16 point about a cell: a whole cell -3..+4 from it and a fraction at the
// tests' boundaries (half a cell, a cell, one either side).
U Around(std::int32_t cell) {
    const std::int32_t d = static_cast<std::int32_t>(sh::Next() % 8) - 3;
    const U frac = PickOf(0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x8001, sh::Next() & 0xFFFF);
    return (static_cast<U>(cell + d) << 16) + frac;
}
// A slide +0x30 one step from each end and one either side: the ends 0 and
// 0x100 / -0x100 / 0xC0, the steps 0x10 and 0x20.
U Slide() {
    return PickOf(0, 1, 0xFFFF, 0xF, 0x10, 0x11, 0x1F, 0x20, 0x21, 0xFFF0, 0xFFEF, 0xFFF1, 0xFFE0, 0xFFDF, 0xFFE1,
                  0xAF, 0xB0, 0xB1, 0xC0, 0xDF, 0xE0, 0xE1, 0xEF, 0xF0, 0xF1, 0x100, 0xFF, 0x101,
                  0xFF00, 0xFEFF, 0xFF01, 0xFF10, 0xFF0F, 0xFF11, 0xFF20, 0xFF1F, 0xFF21, sh::Next());
}
// A cell word: small (0..0x7F) or random.
U Cell() { return sh::Often() ? sh::Next() % 0x80 : sh::Next() & 0xFFFF; }

// The variant table and its room a place state reads (0 for the others).
struct Placing { U cells; unsigned room; };
Placing PlaceOf(unsigned k) {
    switch (k) {
    case k33Place: return {at::kSub33Cells, at::kRoom2};
    case k34Place: return {at::kSub34Cells, at::kRoom2};
    case k37Place: return {at::kSub37Cells, at::kRoom2};
    case k35Place: return {at::kSub35Cells, at::kRoom4};
    case k38Place: return {at::kSub38Cells, at::kRoom4};
    case k3BPlace: return {at::kSub3BCells, at::kRoom2};
    default: return {0, 0};
    }
}

void Seed(unsigned k) {
    g_k = k;
    // every record the disturbance may move Sprite_Current to: the slide at
    // its ends, +8 the axis (0 half the time), the counts +9 / +0xA inside
    // sub-kind 0x41's curve (the states read it by index before any call)
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        SetWord(e + 0x30, Slide());
        if (sh::Half()) e[8] = 0;
        e[9] = static_cast<unsigned char>(sh::Next() % 0x18);
    }
    unsigned char* const s = Sprite_Current;
    SetWord(s + 0x36, Cell());
    SetWord(s + 0x3A, Cell());
    std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    const Placing place = PlaceOf(k);
    if (place.room != 0) {
        // the variant inside its room, the z cell 0 half the time (+8); the
        // cell the leader is tested against is the table's (after the write)
        const U v = sh::Next() % place.room;
        SetWord(s + 0x36, v);
        if (sh::Half()) SetWord(s + 0x3A, 0);
        cx = Mem(place.cells + 2 * v)[0];
        cz = Mem(place.cells + 2 * v + 1)[0];
    }
    // the leader about the cell seven times in eight
    if (sh::Next() % 8 != 0) {
        SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(Around(cx)));
        SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(Around(cz)));
    }
    if (sh::Half()) Field_Request = 0;
    if (sh::Half()) Frame_Counter &= ~7u;
    // the cue bytes: Cond_ByteFE 0..3 mostly, the chapter's run 0xC and step 2
    if (sh::Often()) Cond_ByteFE = static_cast<unsigned char>(sh::Next() % 4);
    if (sh::Half()) Mem(sh::at::kRun)[0] = 0xC;
    if (sh::Half()) Mem(sh::at::kStep)[0] = 2;
    switch (k) {
    case k37Hold: s[9] = static_cast<unsigned char>(PickOf(0, 6, 7, 8, 9, 0xFF, sh::Next())); break;
    case k41Turn: s[9] = static_cast<unsigned char>(PickOf(0, 0xA, 0xB, 0xC, 0xE, 0xF, 0x10, sh::Next() % 0x1C)); break;
    case k41Hold:
        s[9] = static_cast<unsigned char>(PickOf(0, 1, 0x15, 0x16, 0x17, 0x18, sh::Next() % 0x1C));
        s[0xA] = static_cast<unsigned char>(PickOf(0, 0x17, 0x18, 0x19, 0x1A, 0x2F, 0x30, 0x31, sh::Next()));
        break;
    case k3DRipple: {
        // a map of 1..31 cells each way (the harness keeps the area block's
        // first 8 KiB compared) and the leader's cell about it, so the 35 x
        // 35 rows and columns about the leader cross its edges
        unsigned char* const header = Mem(sh::at::kAreaBlock);
        header[0] = static_cast<unsigned char>(1 + sh::Next() % 0x1F);
        header[1] = static_cast<unsigned char>(1 + sh::Next() % 0x1F);
        SetWord(Mem(at::kLeaderCellX), PickOf(0, 0x14, 0x20, 0x30, 0x40, 0xFFF0, sh::Next() % 0x40));
        SetWord(Mem(at::kLeaderCellZ), PickOf(0, 0x14, 0x20, 0x30, 0x40, 0xFFF0, sh::Next() % 0x40));
        if (sh::Often()) Cond_ByteFE = static_cast<unsigned char>(1 + sh::Next() % 0xFF);
        break;
    }
    default: break;
    }
}

// The draws' arguments: sub-kind 0x40's step a byte (its callers push 0 or +9)
// or any; sub-kind 0x41's variant 0 or 1 (its callers push 0, 1 or +9 > 0xB),
// the angle a curve value (<< 4) or any.
void Args(unsigned k, U* a) {
    switch (k) {
    case k40Draw: a[0] = sh::Often() ? sh::Next() & 0xFF : sh::Next(); break;
    case k41Draw:
        a[0] = sh::Next() % at::kSub41Variants;
        a[1] = sh::Half() ? (sh::Next() % 0x42) << 4 : sh::Next();
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the slide +0x30, +8, the
// leader's point, the cell words, the counts +9 (inside sub-kind 0x41's curve)
// and +0xA, Cond_ByteFE.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 8) {
    case 0: SetWord(s + 0x30, (v & 1) ? 0x100u - ((v >> 1) & 0x30) : v >> 1); break;
    case 1: s[8] = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 2: SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(v)); break;
    case 3: SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(v)); break;
    case 4: SetWord(s + ((v & 1) ? 0x36 : 0x3A), v >> 1); break;
    case 5: s[9] = static_cast<unsigned char>(v % 0x18); break;
    case 6: s[0xA] = static_cast<unsigned char>(v); break;
    case 7: Cond_ByteFE = static_cast<unsigned char>(v % 4); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E6B_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E6B_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_6b: BOF3X_E6B_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_6b", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], nullptr, 0,
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_6b
