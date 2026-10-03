// BOF3X_SHADOW=effect_5g: group E5G's 24 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_5g.md section 4. BOF3X_E5G_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E5G --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; every extent is the tool's. Shapes: the four sub-state
// dispatchers, the states and sub-kind 0x2C's draw kEffect (Sprite_Current one
// of the 20 Effect_Objects records, +5 0x18, a dispatcher's +2 below its
// table's length); the two draws with arguments kCall. No function answers.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5g.h"
#include "game/effect_5g_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_5g {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E5G --clones, 2026-10-03.
constexpr sh::CallSite kCalls50AFB0[] = {{0x36, 0x5720C0}, {0x100, 0x50B220}};
constexpr sh::CallSite kCalls50B0C0[] = {{0x64, 0x587740}, {0x77, 0x50B220}};
constexpr sh::CallSite kCalls50B150[] = {{0x1C, 0x50B220}};
constexpr sh::CallSite kCalls50B1E0[] = {{0x24, 0x587740}, {0x38, 0x50B220}};
constexpr sh::CallSite kCalls50B220[] = {{0x43, 0x5A77C0},  {0x4C, 0x461E50},  {0x68, 0x5A75D0},  {0x70, 0x5A77A0},
                                         {0xFD, 0x5720C0},  {0x123, 0x5720C0}, {0x164, 0x5720C0}, {0x18A, 0x5720C0},
                                         {0x203, 0x5A85F0}, {0x209, 0x5A9290}, {0x222, 0x572A00}, {0x22B, 0x461E50}};
constexpr sh::CallSite kCalls50B480[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x27, 0x5A75B0}, {0x2F, 0x5A7780}, {0x78, 0x461E50}};
constexpr sh::CallSite kCalls50B540[] = {{0x47, 0x5720C0}, {0x150, 0x50B8B0}};
constexpr sh::CallSite kCalls50B6A0[] = {{0xC7, 0x587740}, {0xD8, 0x50B8B0}};
constexpr sh::CallSite kCalls50B780[] = {{0x1A, 0x50B8B0}};
constexpr sh::CallSite kCalls50B7A0[] = {{0x64, 0x50B8B0}, {0xBB, 0x50B8B0}};
constexpr sh::CallSite kCalls50B870[] = {{0x24, 0x587740}, {0x36, 0x50B8B0}};
constexpr sh::CallSite kCalls50B8B0[] = {{0x99, 0x5A77C0},  {0xAF, 0x572FA0},  {0xBB, 0x5A75D0},  {0xC3, 0x5A77A0},
                                         {0x1A7, 0x5720C0}, {0x1CD, 0x5720C0}, {0x20E, 0x5720C0}, {0x234, 0x5720C0},
                                         {0x281, 0x5A85F0}, {0x28A, 0x5A9290}, {0x2A7, 0x572A00}, {0x2BD, 0x572FA0}};
constexpr sh::CallSite kCalls50BBB0[] = {{0x36, 0x50BDC0}, {0x3F, 0x50BDC0}};
constexpr sh::CallSite kCalls50BC00[] = {{0x7F, 0x587740}, {0x94, 0x50BDC0}, {0x9D, 0x50BDC0}};
constexpr sh::CallSite kCalls50BCB0[] = {{0x1E, 0x50BDC0}, {0x27, 0x50BDC0}};
constexpr sh::CallSite kCalls50BCE0[] = {{0x78, 0x50BDC0}, {0x81, 0x50BDC0}};
constexpr sh::CallSite kCalls50BD70[] = {{0x24, 0x587740}, {0x3A, 0x50BDC0}, {0x43, 0x50BDC0}};
constexpr sh::CallSite kCalls50BDC0[] = {{0x98, 0x5A77C0},  {0xB1, 0x572FA0},  {0xD8, 0x5720C0},  {0xFE, 0x5720C0},
                                         {0x13F, 0x5720C0}, {0x165, 0x5720C0}, {0x18E, 0x5A75D0}, {0x196, 0x5A77A0},
                                         {0x1C9, 0x5A85F0}, {0x1CF, 0x5A9290}, {0x1E2, 0x572A00}, {0x1FA, 0x572FA0}};
constexpr sh::CallSite kCalls50BFF0[] = {{0x47, 0x5720C0}, {0xAB, 0x579F00}, {0xC4, 0x579F00}, {0xD4, 0x50B8B0}};

#define E5G_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E5G_CALLS(a) a, E5G_N(a)
#define E5G_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub2B_Run", 0x50AF90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2B_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub2B_Place", 0x50AFB0, 0x10A, E5G_CALLS(kCalls50AFB0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2B_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2B_WaitNear", 0x50B0C0, 0x81, E5G_CALLS(kCalls50B0C0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2B_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2B_Open", 0x50B150, 0x23, E5G_CALLS(kCalls50B150), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2B_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2B_WaitFar", 0x50B180, 0x5B, nullptr, 0, nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2B_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2B_Close", 0x50B1E0, 0x3F, E5G_CALLS(kCalls50B1E0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2B_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2B_Draw", 0x50B220, 0x251, E5G_CALLS(kCalls50B220), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2B_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub36_Pulse", 0x50B480, 0x9C, E5G_CALLS(kCalls50B480), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub36_Pulse), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub2C_Run", 0x50B520, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2C_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub2C_Place", 0x50B540, 0x157, E5G_CALLS(kCalls50B540), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2C_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2C_WaitNear", 0x50B6A0, 0xDF, E5G_CALLS(kCalls50B6A0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2C_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2C_Open", 0x50B780, 0x1F, E5G_CALLS(kCalls50B780), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2C_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2C_WaitFar", 0x50B7A0, 0xC2, E5G_CALLS(kCalls50B7A0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2C_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2C_Close", 0x50B870, 0x3B, E5G_CALLS(kCalls50B870), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2C_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2C_Draw", 0x50B8B0, 0x2D7, E5G_CALLS(kCalls50B8B0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub2C_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3A_Run", 0x50BB90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub3A_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub3A_Start", 0x50BBB0, 0x48, E5G_CALLS(kCalls50BBB0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub3A_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3A_WaitParty", 0x50BC00, 0xA9, E5G_CALLS(kCalls50BC00), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub3A_WaitParty), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3A_Open", 0x50BCB0, 0x30, E5G_CALLS(kCalls50BCB0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub3A_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3A_WaitClear", 0x50BCE0, 0x8D, E5G_CALLS(kCalls50BCE0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub3A_WaitClear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3A_Close", 0x50BD70, 0x4C, E5G_CALLS(kCalls50BD70), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub3A_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3A_DrawPanel", 0x50BDC0, 0x208, E5G_CALLS(kCalls50BDC0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub3A_DrawPanel), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub4A_Run", 0x50BFD0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub4A_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub4A_Place", 0x50BFF0, 0xDB, E5G_CALLS(kCalls50BFF0), nullptr, 0, nullptr, 0, E5G_FN(EffectKind18Sub4A_Place), 0, false, kEf, 0, 0, 0, 0x18},
};
#undef E5G_FN
#undef E5G_CALLS
#undef E5G_N

enum : unsigned {
    k2BRun, k2BPlace, k2BWaitNear, k2BOpen, k2BWaitFar, k2BClose, k2BDraw,
    k36Pulse,
    k2CRun, k2CPlace, k2CWaitNear, k2COpen, k2CWaitFar, k2CClose, k2CDraw,
    k3ARun, k3AStart, k3AWaitParty, k3AOpen, k3AWaitClear, k3AClose, k3ADraw,
    k4ARun, k4APlace, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the effects ---------------------------------------------------------------------

// The two draws with arguments read Sprite_Current's record: its address and
// the words they read (+0x30 the slide, +0x36 / +0x3A the cell) logged, so a
// draw on the wrong record, or before a state's write it should follow, shows.
U FxCurrent(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    sh::Note(Key(s), sh::InRegions(s + 0x30, 0x10) ? static_cast<U>(Long(s + 0x30)) : 0u,
             sh::InRegions(s + 0x30, 0x10) ? static_cast<U>(Long(s + 0x34)) : 0u,
             sh::InRegions(s + 0x30, 0x10) ? static_cast<U>(Long(s + 0x38)) : 0u);
    return answer;
}

#define E5G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name: the callers push immediates,
    // read whole (test ecx, ecx; the side indexes a table and is added to the
    // texture word; dy goes to MapView_LinkPrimAt whole)
    {E5G_OURS(EffectKind18Sub2B_Draw), 1, {kW}, kG, 0, 0, {}, &FxCurrent},
    {E5G_OURS(EffectKind18Sub2C_Draw), 0, {}, kPh, 0, 0},
    {E5G_OURS(EffectKind18Sub3A_DrawPanel), 2, {kW, kW}, kG, 0, 0, {}, &FxCurrent},
};
#undef E5G_OURS

// The sub-state tables the dispatchers jump through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65E9EC, 5}, {0x65EA48, 5}, {0x65EA60, 5}, {0x65EA78, 4},
};
const std::uint8_t kKinds[] = {0x18};

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
// A slide +0x30 at its ends: 0xC0 and 0x80 open, 0 shut, one step off.
U Slide() { return PickOf(0, 0x10, 0x11, 0xFFF0, 0x70, 0x7F, 0x80, 0xB0, 0xBF, 0xC0, 0xD0, sh::Next()); }
// A cell word: small (0..0x7F) or random.
U Cell() { return sh::Often() ? sh::Next() % 0x80 : sh::Next() & 0xFFFF; }

void Seed(unsigned k) {
    // every record the disturbance may move Sprite_Current to: the slide at
    // its ends, +8 the axis (0 half the time)
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        SetWord(e + 0x30, Slide());
        if (sh::Half()) e[8] = 0;
    }
    unsigned char* const s = Sprite_Current;
    SetWord(s + 0x36, Cell());
    SetWord(s + 0x3A, Cell());
    // the cell the leader is tested against: the table's for a place state
    // (after the write), the record's own otherwise
    std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    switch (k) {
    case k2BPlace: {
        const U v = sh::Next() % at::kSub2BVariants;
        SetWord(s + 0x36, v);
        cx = Mem(at::kSub2BCells + 2 * v)[0];
        cz = Mem(at::kSub2BCells + 2 * v + 1)[0];
        break;
    }
    case k2CPlace:
    case k4APlace: {
        const U v = sh::Next() % at::kSub2CVariants;
        SetWord(s + 0x36, v);
        if (sh::Half()) SetWord(s + 0x3A, 0);   // +8 from the spawn's z cell
        cx = Mem(at::kSub2CCells + 2 * v)[0];
        cz = Mem(at::kSub2CCells + 2 * v + 1)[0];
        break;
    }
    default: break;
    }
    // the leader about the cell seven times in eight
    if (sh::Next() % 8 != 0) {
        SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(Around(cx)));
        SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(Around(cz)));
    }
    // the party: each record in use or not, about the cell
    for (unsigned m = 0; m < at::kTrioCount; ++m) {
        unsigned char* const r = Mem(sh::at::kObjTrio + at::kTrioStride * m);
        if (sh::Half()) r[0] = 0;
        if (sh::Next() % 8 != 0) {
            SetLong(r + 0x34, static_cast<std::int32_t>(Around(cx)));
            SetLong(r + 0x38, static_cast<std::int32_t>(Around(cz)));
        }
    }
    if (sh::Half()) Field_Request = 0;
    if (sh::Half()) Frame_Counter &= ~7u;
}

// The draws' arguments: Sub2B_Draw's flag 0 half the time (the callers push 0
// or 1); the panel's side inside its table's room, dy the callers' 0..3 or
// any.
void Args(unsigned k, U* a) {
    switch (k) {
    case k2BDraw: a[0] = sh::Half() ? 0u : PickOf(1, sh::Next()); break;
    case k3ADraw:
        a[0] = sh::Next() % at::kSidesRoom;
        a[1] = PickOf(0, 1, 2, 3, 0xFFFFFFFFu, sh::Next());
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the slide +0x30, +8, the
// leader's point, the cell words.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 5) {
    case 0: SetWord(s + 0x30, (v & 1) ? 0xC0u - ((v >> 1) & 0x10) : v >> 1); break;
    case 1: s[8] = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 2: SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(v)); break;
    case 3: SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(v)); break;
    case 4: SetWord(s + ((v & 1) ? 0x36 : 0x3A), v >> 1); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E5G_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E5G_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_5g: BOF3X_E5G_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_5g", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], nullptr, 0,
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_5g
