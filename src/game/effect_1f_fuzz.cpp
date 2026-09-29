// BOF3X_SHADOW=effect_1f: group E1F's fifteen through the scenario harness
// (scenario_harness.h, used unchanged) in effect mode, once at start-up.
// docs/effect_1f.md section 3. BOF3X_E1F_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E1F --clones --harness scenario
// (2026-09-29), each extent read again to its last instruction (capstone). The
// state handlers run as kEffect (Sprite_Current an Effect_Objects record), the
// two dispatchers with their tables swapped for recorders on both sides; the
// cdecl helpers as kCall; game mode 8's steps as kState. Effect record 6
// (0x7E14E0), the menu's state, is seeded by hand every round: the harness's
// effect shape gives every record a kind and state bytes, and this group's
// menu reads record 6 whatever Sprite_Current is.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1f.h"
#include "game/effect_1f_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_1f {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;

// tools/band_rows.py --group E1F --clones --harness scenario, 2026-09-29.
constexpr sh::CallSite kCalls52A6F0[] = {{0x28, 0x5905D0}, {0x5C, 0x516B30}, {0x83, 0x516B30}};
constexpr sh::CallSite kCalls52A780[] = {{0xC, 0x461EB0}, {0x29, 0x587740}, {0x6F, 0x587740}, {0x9C, 0x587740}, {0x102, 0x516B30}, {0x166, 0x5905D0}};
constexpr sh::CallSite kCalls52A8F0[] = {{0xE, 0x461EB0}, {0x2D, 0x587740}, {0x7C, 0x516B30}, {0xBB, 0x591B60}, {0xD3, 0x590BB0}, {0x101, 0x587740}, {0x12E, 0x587740}, {0x1AC, 0x516B30}, {0x20C, 0x591B60}, {0x224, 0x590BB0}, {0x2B6, 0x587740}, {0x2DD, 0x587740}, {0x322, 0x587740}, {0x355, 0x587740}, {0x3A3, 0x587740}, {0x409, 0x587740}, {0x422, 0x587740}, {0x439, 0x587740}, {0x499, 0x5905D0}};
constexpr sh::CallSite kCalls52CD50[] = {{0x10, 0x588F20}};
constexpr sh::CallSite kCalls52CDF0[] = {{0x11, 0x52CD50}};
constexpr sh::CallSite kCalls52CE20[] = {{0x25, 0x589870}};
constexpr sh::CallSite kCalls52CED0[] = {{0x16, 0x52CE60}};
constexpr sh::CallSite kCalls52CF00[] = {{0x0, 0x517350}, {0x5, 0x52B6C0}, {0xA, 0x56E6C0}, {0xF, 0x531B60}, {0x14, 0x52CDF0}, {0x19, 0x494030}, {0x1E, 0x592F00}};
constexpr sh::CallSite kCalls52CF30[] = {{0x0, 0x593950}};
constexpr sh::CallSite kCalls52CF40[] = {{0x0, 0x56E6C0}, {0x5, 0x531B60}, {0xA, 0x52CDF0}, {0xF, 0x494030}, {0x14, 0x592F00}};
constexpr sh::CallSite kCalls52CF60[] = {{0x63, 0x5A77C0}, {0x6F, 0x461E50}};
constexpr sh::CallSite kCalls52CFE0[] = {{0x8, 0x5A7710}, {0x90, 0x461E50}};

#define E1F_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E1F_CALLS(a) a, E1F_N(a)
#define E1F_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect, kCa = sh::Shape::kCall, kSt = sh::Shape::kState;
constexpr U kAll = 0xFFFFFFFFu;
// Answers: Sprite_UpdateScreenScaled's eax is defined on every path (the
// scale, or Sprite_Current at 0) and compared whole though its callers are
// state handlers that drop it; FieldPanel_KindPoints' eax whole (its high word
// the count's on one path); FieldPanel_KindTotal's ax (above it the last
// callee's); UiSprite_Draw's primitive whole. The rest are void.
const sh::Clone kAll15[] = {
    {"ChoiceMenu_Run", 0x52A6C0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, E1F_FN(ChoiceMenu_Run), 0, false, kEf},
    {"ExtraSlots_Step", 0x52A6D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1F_FN(ExtraSlots_Step), 0, false, kEf},
    {"ExtraSlots_Enter", 0x52A6F0, 0x8C, E1F_CALLS(kCalls52A6F0), nullptr, 0, nullptr, 0, E1F_FN(ExtraSlots_Enter), 0, false, kEf},
    {"ExtraSlots_PickSlot", 0x52A780, 0x16F, E1F_CALLS(kCalls52A780), nullptr, 0, nullptr, 0, E1F_FN(ExtraSlots_PickSlot), 0,
     false, kEf},
    {"ExtraSlots_PickItem", 0x52A8F0, 0x4A4, E1F_CALLS(kCalls52A8F0), nullptr, 0, nullptr, 0, E1F_FN(ExtraSlots_PickItem), 0,
     false, kEf},
    {"Sprite_UpdateScreenScaled", 0x52CD50, 0x9A, E1F_CALLS(kCalls52CD50), nullptr, 0, nullptr, 0,
     E1F_FN(Sprite_UpdateScreenScaled), kAll, false, kEf},
    {"Sprite_UpdateAllScaled", 0x52CDF0, 0x26, E1F_CALLS(kCalls52CDF0), nullptr, 0, nullptr, 0, E1F_FN(Sprite_UpdateAllScaled),
     0, false, kSt},
    {"Effect_ResetFirstSeven", 0x52CE20, 0x3C, E1F_CALLS(kCalls52CE20), nullptr, 0, nullptr, 0, E1F_FN(Effect_ResetFirstSeven),
     0, false, kEf},
    {"FieldPanel_KindPoints", 0x52CE60, 0x63, nullptr, 0, nullptr, 0, nullptr, 0, E1F_FN(FieldPanel_KindPoints), kAll, false, kCa},
    {"FieldPanel_KindTotal", 0x52CED0, 0x2C, E1F_CALLS(kCalls52CED0), nullptr, 0, nullptr, 0, E1F_FN(FieldPanel_KindTotal), 0xFFFF,
     false, kCa},
    {"GameMode8_Frame", 0x52CF00, 0x23, E1F_CALLS(kCalls52CF00), nullptr, 0, nullptr, 0, E1F_FN(GameMode8_Frame), 0, false, kSt},
    {"GameMode8_TradeStep", 0x52CF30, 0x5, E1F_CALLS(kCalls52CF30), nullptr, 0, nullptr, 0, E1F_FN(GameMode8_TradeStep), 0, false,
     kSt},
    {"GameMode8_WaitFrame", 0x52CF40, 0x19, E1F_CALLS(kCalls52CF40), nullptr, 0, nullptr, 0, E1F_FN(GameMode8_WaitFrame), 0, false,
     kSt},
    {"UiSprite_SetMode", 0x52CF60, 0x79, E1F_CALLS(kCalls52CF60), nullptr, 0, nullptr, 0, E1F_FN(UiSprite_SetMode), 0, false, kCa},
    {"UiSprite_Draw", 0x52CFE0, 0x9C, E1F_CALLS(kCalls52CFE0), nullptr, 0, nullptr, 0, E1F_FN(UiSprite_Draw), kAll, false, kCa},
};
#undef E1F_FN
#undef E1F_CALLS
#undef E1F_N

enum : unsigned {
    kChoice, kStep, kEnter, kPickSlot, kPickItem, kScaled, kAllScaled, kReset, kPoints, kTotal, kFrame, kTrade, kWait,
    kSetMode, kDraw, kCount
};
static_assert(kCount == sizeof kAll15 / sizeof kAll15[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* M(U a) { return sh::Mem(a); }
void Put16(U a, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(M(a), &w, 2);
}
void Put32(U a, U v) { std::memcpy(M(a), &v, 4); }
U Get32(U a) {
    U v;
    std::memcpy(&v, M(a), 4);
    return v;
}
unsigned char* R6() { return M(at::kRecord6); }

// --- the stand-ins' effects: deterministic in the log so far and the state ------

// Gfx_CommitPrim: the primitive at the cursor (size bytes) noted as it stands
// when committed - so a field written after the commit shows - then the cursor
// moved on while the buffer has room (as the standard row does).
U FxCommit(const U* a, U answer) {
    unsigned char* const next = sh::Pointer(0x7E0670);
    const unsigned size = a[1] & 0xFF;
    if (next >= sh::Packets() && next + size + 0x40 <= sh::Packets() + 0x800) {
        sh::NoteBytes(next, size);
        sh::SetPointer(0x7E0670, next + size);
    }
    return answer;
}
// Inventory_Remove / Inventory_Add: the real ones rewrite the category's list;
// ExtraSlots_PickItem counts that list after them. Half the time a byte of the
// category-3 list moves.
U FxInventory(const U*, U answer) {
    const U n = sh::Noise();
    if (n & 1) M(at::kAccessoryIds)[(n >> 1) % 0x80] = static_cast<unsigned char>(n >> 9);
    return answer;
}
// Sprite_UpdateScreen: Sprite_Current and its +0x3C (which the caller holds at
// 0 across the call) noted as the callee sees them.
U FxUpdateScreen(const U*, U answer) {
    const unsigned char* const s = Sprite_Current;
    sh::Note(Key(s), sh::InRegions(s + 0x3C, 4) ? Get32(Key(s + 0x3C)) : 0xDEAD);
    return answer;
}

#define E1F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kP = sh::Answer::kPhase;
constexpr U kU8 = 0xFF, kU16 = 0xFFFF;
const sh::Callee kCallees[] = {
    // the group's own, called directly: their recorders log Sprite_Current
    {E1F_OURS(Sprite_UpdateScreenScaled), 0, {}, kP, 0, 0, {}, nullptr, nullptr, true},
    {E1F_OURS(Sprite_UpdateAllScaled), 0, {}, kP, 0, 0, {}, nullptr, nullptr, true},
    // FieldPanel_KindTotal pushes the count from `movzx ax, al` (eax's high word
    // whatever it was) and the index whole; the callee reads the kind's byte
    // (and eax, 0xFF) and the count's word (cmp ax / and 0xFFFF)
    {E1F_OURS(FieldPanel_KindPoints), 2, {kU8, kU16}, kG, 0, 0, {}, nullptr, nullptr, true},
    // the item pushed as ebx / eax over a byte load (the high bytes the caller's);
    // Inventory_Add / _Remove read the low bytes only (scena_sx.cpp, char_stats.cpp)
    {E1F_OURS(Inventory_Remove), 3, {kU8, kU8, kU8}, kF, 0, 0, {}, &FxInventory, nullptr, true},
    {E1F_OURS(Inventory_Add), 3, {kU8, kU8, kU8}, kF, 0, 0, {}, &FxInventory, nullptr, true},
    {E1F_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, kG, 0, 0, {}, &FxCommit, nullptr, true},
    {E1F_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &FxUpdateScreen, nullptr, true},
};
#undef E1F_OURS

// The dispatchers' tables, swapped for recorders on both sides.
const sh::DataTable kTables[] = {
    {0x6602F4, 3},   // ChoiceMenu_Choices
    {0x660300, 3},   // ExtraSlots_Steps
};

// Beyond effect mode's standard regions (which hold Effect_Objects, the
// sprites, the input cells, the buttons, the pools, the save block's
// 0x904098..0x904160 with 0x9040EC and the extra slots): category 3's id list.
const sh::Region kRegions[] = {
    {at::kAccessoryIds, 0x80},
};

// --- the seeds ----------------------------------------------------------------

// The ids whose NameTable_Accessories record has kind 0xA / 0xB (read from the
// image once; the lists are seeded from them so the menus' filters pass).
unsigned char g_kindA[256], g_kindB[256];
unsigned g_nA, g_nB;
void Kinds() {
    if (g_nA || g_nB) return;
    for (unsigned id = 0; id < 256; ++id) {
        const unsigned char k = *reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(at::kAccessoryKind + 24 * id));
        if (k == 0xA) g_kindA[g_nA++] = static_cast<unsigned char>(id);
        if (k == 0xB) g_kindB[g_nB++] = static_cast<unsigned char>(id);
    }
}
unsigned char IdOf(unsigned which) {
    switch (which % 3) {
    case 0: return g_nA ? g_kindA[sh::Next() % g_nA] : static_cast<unsigned char>(sh::Next());
    case 1: return g_nB ? g_kindB[sh::Next() % g_nB] : static_cast<unsigned char>(sh::Next());
    default: return static_cast<unsigned char>(sh::Next());
    }
}
// An s16 index into the id list: mostly inside the 128, sometimes just past
// either end (the original reads there unchecked; readable memory either way).
U ListIndex() { return PickOf(sh::Next() % 0x80, sh::Next() % 0x80, 0, 0x7F, 0x80, 0xFFFF, 0xFFFFu - sh::Next() % 0x100, 0x80 + sh::Next() % 0x100); }
U Counter() { return PickOf(0, 1, 2, 7, 8, 9, 10, 16, 17, sh::Next() % 0x40, 0xFFFFFFFFu, 0x80000000u, sh::Next()); }

// The buttons: the cancel and confirm masks one bit or random, Input_Pressed on
// either, both, the repeat bits, the page bits, or random.
void Buttons() {
    const U cancel = PickOf(0x40, 0x20, 0x80, 1u << (sh::Next() % 16), sh::Next() & 0xFFFF);
    const U confirm = PickOf(0x20, 0x40, 0x10, 1u << (sh::Next() % 16), sh::Next() & 0xFFFF);
    Put16(0x903590, cancel);    // Field_CancelButtons
    Put16(0x90358E, confirm);   // Field_ConfirmButtons
    Put16(0x7E1BEC, PickOf(cancel, confirm, cancel | confirm, 0x1000, 0x4000, 0x5000, 4, 8, 0xC, 0, sh::Next() & 0xFFFF,
                           (sh::Next() & 0xFFFF) & ~(cancel | confirm)));
}

void Menu() {
    Kinds();
    unsigned char* const r = R6();
    r[1] = static_cast<unsigned char>(PickOf(3, 4, 3, 4, 2, 5, 8, 0xE, sh::Next()));
    r[7] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0x80, 0x81, 0x7F, 0xFF, 0xFE, sh::Next()));
    r[8] = static_cast<unsigned char>(sh::Next());
    r[0xA] = static_cast<unsigned char>(sh::Often() ? 0 : sh::Next() % 4);
    Put32(at::kRecord6 + 0xC, Counter());
    Put32(at::kRecord6 + 0x10, Counter());
    Put32(at::kRecord6 + 0x18, Counter());
    Put32(at::kRecord6 + 0x1C, PickOf(Counter(), 0x40 + sh::Next() % 0x40, 9, 17, 18));
    Put32(at::kRecord6 + 0x38, (sh::Next() & 0xFFFF0000u) | PickOf(0, 1, 8, 9, 10, 0x7F, 0x8000, 0xFFFF, 0xFFF8, sh::Next() % 0x80));
    Put16(at::kRecord6 + 0x36, ListIndex());
    Put16(at::kRecord6 + 0x3A, ListIndex());
    for (unsigned i = 0; i < 0x80; ++i) M(at::kAccessoryIds)[i] = IdOf(sh::Next());
    // the slots: empty, random, or the id the cursor is on
    const auto at36 = static_cast<short>(Get32(at::kRecord6 + 0x36) & 0xFFFF);
    const auto at3A = static_cast<short>(Get32(at::kRecord6 + 0x3A) & 0xFFFF);
    M(at::kExtraAccessory)[0] = static_cast<unsigned char>(PickOf(0, sh::Next(), IdOf(1), 0));
    const unsigned char under = *reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(at::kAccessoryIds + static_cast<U>(static_cast<int>(at3A))));
    M(at::kExtraItem)[0] = static_cast<unsigned char>(PickOf(0, sh::Next(), under, under, IdOf(0)));
    (void)at36;
    Buttons();
}

// Sprite_UpdateScreenScaled's divisor away from 0 on the record current: the
// original faults there, ours aborts.
void KeepDivisor() {
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    const U far = Get32(Key(s + 0x60));
    std::uint16_t lift16;
    std::memcpy(&lift16, s + 0x3E, 2);
    const U lift = static_cast<U>(static_cast<int>(static_cast<short>(lift16))) << 1;
    if (static_cast<std::int32_t>(far) < 0 && far - lift == 0) Put32(Key(s + 0x60), far - 1);
}

void Scaled() {
    if (sh::Half()) Sprite_Current = sh::SpriteRecord(sh::Next());
    unsigned char* const s = Sprite_Current;
    const U lift = PickOf(0, 1, 0xFFFF, 0x8000, 0x7FFF, sh::Next() % 0x100, sh::Next());
    Put16(Key(s + 0x3E), lift);
    const U doubled = static_cast<U>(static_cast<int>(static_cast<short>(lift))) << 1;
    Put32(Key(s + 0x60), PickOf(0, 1, 0xFFFFFFFFu, 2, 0xFFFFFFFEu, 0x4650000, 0x4650001, 0x80000000u, 0x7FFFFFFFu,
                                doubled - 1, doubled + 1, 0u - (sh::Next() % 0x1000), sh::Next() % 0x1000, sh::Next()));
    Put32(Key(s + 0x3C), sh::Next());
    KeepDivisor();
}

void Seed(unsigned k) {
    switch (k) {
    case kChoice: R6()[6] = static_cast<unsigned char>(sh::Next() % 3); break;
    case kStep: Sprite_Current[4] = static_cast<unsigned char>(sh::Next() % 3); break;
    case kEnter:
    case kPickSlot:
    case kPickItem: Menu(); break;
    case kScaled: Scaled(); break;
    case kAllScaled:
        for (unsigned i = 0; i < 30; ++i) M(0x7DEE80 + 0xA4 * i)[0] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next() | 1);
        break;
    case kTotal:
        for (unsigned i = 0; i < 0x20; ++i) M(at::kKindCounts)[i] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        break;
    default: break;
    }
}

// The argument words, after the seed.
void Args(unsigned k, U* a) {
    switch (k) {
    case kPoints: {
        const U kind = sh::Often() ? sh::Next() % 0x20 : sh::Next() % 0x100;
        a[0] = (sh::Next() & 0xFFFFFF00u) | kind;
        const U threshold = *reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(at::kKindRecords + 36 * kind + 0xF));
        const U low = PickOf(threshold, threshold - 1, threshold + 1, 0, 1, 0xFFFF, threshold * 2, sh::Next() % 0x100, sh::Next());
        a[1] = (sh::Next() & 0xFFFF0000u) | (low & 0xFFFF);
        break;
    }
    case kSetMode: a[0] = (sh::Next() & 0xFFFFFF00u) | (sh::Often() ? sh::Next() % 10 : sh::Next() & 0xFF); break;
    case kDraw: a[0] = (sh::Next() & 0xFFFFFF00u) | (sh::Often() ? sh::Next() % 74 : sh::Next() & 0xFF); break;
    default: break;
    }
}

// What the functions read after a call, moved by the group's case of the
// harness's disturbance (from its hash only).
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const r = R6();
    switch (h % 12) {
    case 0: r[PickOf(1, 7, 8, 0xA, 6)] = static_cast<unsigned char>(v >> 4); break;
    case 1: Put32(at::kRecord6 + 0xC + 4 * (v % 5), v >> 12); break;           // +0xC, +0x10, +0x14, +0x18, +0x1C
    case 2: Put16(at::kRecord6 + ((v & 1) ? 0x36 : 0x3A), (v >> 4) % 0x180); break;
    case 3: Put16(at::kRecord6 + 0x38, v >> 3); break;
    case 4: Put16(0x7E1BEC, v >> 5); break;
    case 5: Put16((v & 1) ? 0x903590 : 0x90358E, 1u << ((v >> 1) % 16)); break;
    case 6: M((v & 1) ? at::kExtraItem : at::kExtraAccessory)[0] = static_cast<unsigned char>(v >> 3); break;
    case 7: M(at::kAccessoryIds)[v % 0x80] = static_cast<unsigned char>(v >> 7); break;
    case 8: M(at::kKindCounts)[v % 0x20] = static_cast<unsigned char>(v >> 5); break;
    case 9: {
        unsigned char* const s = Sprite_Current;
        if (sh::InRegions(s, 0x80)) s[3 + (v & 1)] = static_cast<unsigned char>(v >> 1);
        break;
    }
    case 10: {
        unsigned char* const s = Sprite_Current;
        if (sh::InRegions(s, 0x80)) Put32(Key(s + ((v & 1) ? 0x60 : 0x3C)), v >> 2);
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E1F_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E1F_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll15[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll15[k];
        }
    if (n == 0) bof3::Fatal("effect_1f: BOF3X_E1F_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_1f", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.settle = &KeepDivisor;
    g.effect = true;
    sh::Run(g);
}

}  // namespace effect_1f
