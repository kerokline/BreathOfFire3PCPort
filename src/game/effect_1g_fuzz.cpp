// BOF3X_SHADOW=effect_1g: group E1G's fourteen functions through the scenario
// harness's field mode (scenario_harness.h, docs/scenario_harness.md sections
// 7 and 8, not edited), once at start-up. docs/effect_1g.md section 4.
//
// The clone table is tools/band_rows.py --group E1G --clones --harness scenario
// (2026-09-29, every extent read again to its last instruction; the tool's),
// names given. None of the fourteen reads Sprite_Current or an effect record:
// they are the item-trade screen's (docs/effect_1g.md section 0), so the group
// runs field mode, not effect mode - the shapes kState for the four states and
// kCall for the helpers. Beyond the standard and field-standard stand-ins the
// group lists:
//   - its own functions another of its own calls, each logging the trade
//     screen's bytes when it runs (the order of a state's writes against its
//     draws);
//   - standard callees re-listed with the masks of what the callee reads,
//     where the originals push a register with stale upper bytes
//     (Text_DrawAt's colour and count, Item_NamePtr, Item_IconKind,
//     Menu_DrawIcon8, Inventory_Count);
//   - louder stand-ins where a caller branches on the answer: Input_AutoRepeat
//     from the moves, Inventory_Count near the counts the records need,
//     Item_IconKind a nibble (as the real one answers), Sound_PlayEffect and
//     Transition_Start logging the trade bytes too.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1g.h"
#include "game/effect_1g_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_1g {
namespace {

namespace sh = scenario_harness;
using sh::Shape;
using U = std::uint32_t;

unsigned char& B(U a) { return *sh::Mem(a); }
void SetW(U a, unsigned v) { move_script::SetWord(sh::Mem(a), v); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the clone table (tools/band_rows.py --group E1G --clones, 2026-09-29) ----------------

constexpr sh::CallSite kCalls594060[] = {{0xD, 0x469750}, {0x37, 0x587740}, {0x54, 0x594410}, {0x78, 0x516B30}, {0x7F, 0x594410}, {0x87, 0x594AD0}};
constexpr sh::CallSite kCalls594100[] = {{0xC, 0x461EB0},  {0x2D, 0x587740},  {0x56, 0x587740},  {0x94, 0x495040},
                                         {0xAF, 0x587740}, {0xDF, 0x469750},  {0xFD, 0x516B30},  {0x119, 0x5905D0},
                                         {0x120, 0x594410}, {0x128, 0x5947D0}, {0x12D, 0x5942C0}};
constexpr sh::CallSite kCalls594240[] = {{0x21, 0x469750}, {0x3F, 0x516B30}, {0x5B, 0x5905D0}, {0x62, 0x594410}, {0x6A, 0x5947D0}, {0x6F, 0x5942C0}};
constexpr sh::CallSite kCalls5942C0[] = {{0x4C, 0x5A77C0}, {0x55, 0x461E50}, {0x6F, 0x5A75D0}, {0xDE, 0x461E50}, {0x12A, 0x5A77C0}, {0x133, 0x461E50}};
constexpr sh::CallSite kCalls594410[] = {{0xC, 0x594610},   {0x2F, 0x516B30},  {0x6C, 0x591680},  {0x76, 0x594700},  {0x94, 0x57D800},
                                         {0xA5, 0x516B30},  {0xE2, 0x57D800},  {0xF3, 0x516B30},  {0x126, 0x594D50}, {0x136, 0x57D800},
                                         {0x14C, 0x516B30}, {0x187, 0x57D800}, {0x19D, 0x516B30}, {0x1D3, 0x594D50}};
constexpr sh::CallSite kCalls594610[] = {{0x29, 0x57CF60}, {0x32, 0x52CF60}, {0x3D, 0x52CFE0}, {0x51, 0x52CFE0}, {0x68, 0x52CFE0}, {0x79, 0x52CFE0},
                                         {0x8A, 0x468950}, {0x98, 0x468950}, {0xAC, 0x52CFE0}, {0xC0, 0x52CFE0}, {0xD4, 0x52CFE0}};
constexpr sh::CallSite kCalls594700[] = {{0x57, 0x5919B0}};
constexpr sh::CallSite kCalls5947D0[] = {{0x17, 0x5949F0},  {0x3D, 0x516B30},  {0x4C, 0x5949F0},  {0x72, 0x516B30},  {0xBA, 0x591680},
                                         {0xCC, 0x57D800},  {0xE0, 0x516B30},  {0x116, 0x5B9380}, {0x12B, 0x517090}, {0x152, 0x5919B0},
                                         {0x17D, 0x57D800}, {0x194, 0x516B30}, {0x1AE, 0x5B9380}, {0x1BC, 0x57D800}, {0x1D3, 0x516B30},
                                         {0x1EC, 0x5B9380}, {0x204, 0x517090}};
constexpr sh::CallSite kCalls5949F0[] = {{0x23, 0x57CF60}, {0x2C, 0x52CF60}, {0x37, 0x52CFE0}, {0x4B, 0x52CFE0}, {0x62, 0x52CFE0}, {0x73, 0x52CFE0},
                                         {0x81, 0x468950}, {0x8C, 0x468950}, {0x9D, 0x52CFE0}, {0xB1, 0x52CFE0}, {0xC5, 0x52CFE0}};
constexpr sh::CallSite kCalls594AD0[] = {{0x11, 0x594C90},  {0x44, 0x591680},  {0x4E, 0x57D800},  {0x5D, 0x516B30},  {0x74, 0x5B9380},
                                         {0x87, 0x517090},  {0xB6, 0x594D50},  {0xD7, 0x516B30},  {0x102, 0x5919B0}, {0x117, 0x5B9380},
                                         {0x12D, 0x517090}, {0x153, 0x516B30}, {0x181, 0x5919B0}, {0x196, 0x5B9380}, {0x1AC, 0x517090}};
constexpr sh::CallSite kCalls594C90[] = {{0x20, 0x57CF60}, {0x29, 0x52CF60}, {0x34, 0x52CFE0}, {0x48, 0x52CFE0}, {0x62, 0x52CFE0},
                                         {0x70, 0x468950}, {0x7B, 0x468950}, {0x89, 0x52CFE0}, {0x9D, 0x52CFE0}, {0xB1, 0x52CFE0}};
constexpr sh::CallSite kCalls594D50[] = {{0x13, 0x591720}, {0x31, 0x57D360}};

#define E1G_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E1G_FN(name) reinterpret_cast<const void*>(&::name)
#define E1G_C(name, base, size, calls) #name, base, size, calls, E1G_N(calls), nullptr, 0, nullptr, 0, E1G_FN(name)
#define E1G_L(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, E1G_FN(name)
constexpr Shape kSt = Shape::kState, kCa = Shape::kCall;
const sh::Clone kClones[] = {
    {E1G_C(ItemTrade_FullMessage, 0x594060, 0x8C, kCalls594060), 0, false, kSt},
    {E1G_L(ItemTrade_Leave, 0x5940F0, 0xE), 0, false, kSt},
    {E1G_C(ItemTrade_LeaveAsk, 0x594100, 0x132, kCalls594100), 0, false, kSt},
    {E1G_C(ItemTrade_LeaveWait, 0x594240, 0x74, kCalls594240), 0, false, kSt},
    {E1G_C(ItemTrade_DrawBackground, 0x5942C0, 0x141, kCalls5942C0), 0, false, kCa},
    {E1G_C(ItemTrade_DrawList, 0x594410, 0x1F1, kCalls594410), 0, false, kCa},
    {E1G_C(ItemTrade_DrawListFrame, 0x594610, 0xE1, kCalls594610), 0, false, kCa},
    {E1G_C(ItemTrade_Lacks, 0x594700, 0x90, kCalls594700), 0xFF, false, kCa},
    {E1G_L(ItemTrade_RowCount, 0x594790, 0x36), 0xFF, false, kCa},
    {E1G_C(ItemTrade_DrawNeeds, 0x5947D0, 0x21C, kCalls5947D0), 0, false, kCa},
    {E1G_C(ItemTrade_DrawNeedsFrame, 0x5949F0, 0xD2, kCalls5949F0), 0, false, kCa},
    {E1G_C(ItemTrade_DrawCount, 0x594AD0, 0x1B7, kCalls594AD0), 0, false, kCa},
    {E1G_C(ItemTrade_DrawCountFrame, 0x594C90, 0xBE, kCalls594C90), 0, false, kCa},
    {E1G_C(Item_DrawIcon, 0x594D50, 0x3A, kCalls594D50), 0, false, kCa},
};
#undef E1G_L
#undef E1G_C
#undef E1G_FN

enum : unsigned {
    kFull, kLeave, kLeaveAsk, kLeaveWait, kBackground, kList, kListFrame, kLacks, kRowCount, kNeeds, kNeedsFrame,
    kCountWindow, kCountFrame, kIcon, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the stand-ins' effects ---------------------------------------------------------------------

// The trade screen's bytes as a callee sees them: a state's writes before a
// draw or a sound show against the original's order.
U FxNoteTrade(const U*, U answer) {
    sh::Note(static_cast<U>(B(at::kTradePick)) | static_cast<U>(B(at::kTradeStep)) << 8 | static_cast<U>(B(at::kTradeState)) << 16 |
                 static_cast<U>(B(at::kTradeQuantity)) << 24,
             B(at::kTradeAnswer));
    return answer;
}
U FxText(const U*, U answer) { return Key(sh::Text() + (answer & 0xF0)); }
U FxRepeat(const U*, U answer) {
    static const U kMoves[] = {0, 0x1000, 0x2000, 0x4000, 0x8000, 0xA000, 0x5000, 0xF000};
    const U n = sh::Noise();
    return n % 4 == 0 ? answer : (answer & 0xFFFF0000u) | kMoves[(n >> 4) % 8];
}
U FxHeld(const U*, U answer) {
    // a count near what the records need (their counts are small) half the
    // time, else a stack's 0..99; the upper word garbage (the callers mask it)
    const U n = sh::Noise();
    return (answer & 0xFFFF0000u) | (n & 1 ? (n >> 1) % 8 : (n >> 1) % 100);
}

// --- the callees -------------------------------------------------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr sh::Answer kG = sh::Answer::kGarbage;
#define E1G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
const sh::Callee kCallees[] = {
    // the group's own, called by another of its own (by name in ours, by address in the clones)
    {E1G_OURS(ItemTrade_DrawBackground), 0, {}, kG, 0, 0, {}, &FxNoteTrade},
    // the flag: its byte (0x594431 mov al, bl; 0x59448D mov al, [esp + 0x28]; 0x594610 mov cl)
    {E1G_OURS(ItemTrade_DrawList), 1, {kU8}, kG, 0, 0, {}, &FxNoteTrade},
    {E1G_OURS(ItemTrade_DrawListFrame), 3, {kAll, kAll, kU8}, kG, 0, 0},
    // 0x594711 and ebp, 0xFF; 0x594767 and ecx, 0xFF
    {E1G_OURS(ItemTrade_Lacks), 2, {kU8, kU8}, sh::Answer::kFlag, 0, 0},
    {E1G_OURS(ItemTrade_DrawNeeds), 0, {}, kG, 0, 0, {}, &FxNoteTrade},
    {E1G_OURS(ItemTrade_DrawNeedsFrame), 2, {kAll, kAll}, kG, 0, 0},
    {E1G_OURS(ItemTrade_DrawCount), 0, {}, kG, 0, 0, {}, &FxNoteTrade},
    {E1G_OURS(ItemTrade_DrawCountFrame), 2, {kAll, kAll}, kG, 0, 0},
    // the item (0x594D54 test al, al) and category bytes (Item_IconKind's and
    // eax, 0xFF); dim's byte (Menu_DrawIcon8's 0x57D390 test al, al) - the
    // list pushes a stack dword whose low byte it wrote
    {E1G_OURS(Item_DrawIcon), 5, {kAll, kAll, kU8, kU8, kU8}, kG, 0, 0},
    // re-listed: the callee reads a byte or a word of what the original pushes whole
    // msgbox.cpp: x, y shorts; the colour and the count bytes (Text_DrawString)
    {E1G_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, kAll}, kG, 0, 0, {0, 0, 0, 0, sh::kDerefString}, nullptr, nullptr, true},
    // char_stats.cpp: the category's and the item's low bytes; the answer into the text buffer
    {E1G_OURS(Item_NamePtr), 2, {kU8, kU8}, kG, 0, 0, {}, &FxText, nullptr, true},
    // 0x591724 and eax, 0xFF; 0x591736 and eax, 0xFF - and a nibble answered (0x591748 and eax, 0xF)
    {E1G_OURS(Item_IconKind), 2, {kU8, kU8}, sh::Answer::kByte, 0, 15},
    // 0x57D3A0 and ecx, 0xFFFF; 0x57D3BB and edx, 0xFFFF; 0x57D3D1 and eax, 0xFF; 0x57D390 test al, al
    {E1G_OURS(Menu_DrawIcon8), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},
    // char_stats.cpp: the three low bytes; a count near the records' needs
    {E1G_OURS(Inventory_Count), 3, {kU8, kU8, kU8}, kG, 0, 0, {}, &FxHeld},
    // louder: the pad's moves; the trade bytes at each sound and fade
    {E1G_OURS(Input_AutoRepeat), 1, {kAll}, kG, 0, 0, {}, &FxRepeat},
    {E1G_OURS(Sound_PlayEffect), 1, {kU16}, kG, 0, 0, {}, &FxNoteTrade},
    {E1G_OURS(Transition_Start), 1, {kU8}, kG, 0, 0, {}, &FxNoteTrade},
};
#undef E1G_OURS

// ItemTrade_Leave's table, swapped for recorders on both sides while the fuzz runs.
const sh::DataTable kTables[] = {{at::kLeaveSteps, 2}};

// --- the state ------------------------------------------------------------------------------------

// Beyond the harness's standard and field regions (which hold Input_Pressed,
// MoveScript_WaitWordDA, the packet cursor and buffer, Frame_Counter, the
// style byte 0x903A5A, the row byte 0x905B88 in the camera-turn cells, the
// text scratch 0x904BA0).
const sh::Region kRegions[] = {
    {0x6BE080, 0x20},                        // the trade screen's bytes 0x6BE08C..0x6BE08F
    {0x939850, 0x20},                        // its states 0x93985C / 0x93985E
    {0x903580, 0x14},                        // the button words 0x90358E / 0x903590
    {0x66C7E8, 4},                           // Game_Mode, Game_Step
    {0x803580, 0xE8},                        // MessagePools: the prompts' words 0x80360A..0x80361B
};

// --- the moves -----------------------------------------------------------------------------------
//
// What the functions read again after a call: the pick, the quantity, the
// answer, the step and state, the row byte, the row count. Drawn only from the
// hash the harness hands over.
void Disturb(U h) {
    const unsigned v = (h >> 8) & 0xFF, w = (h >> 16) & 0xFF;
    switch (h % 7) {
    case 0: B(at::kTradePick) = static_cast<unsigned char>(w & 0xC0 ? w : w % 11); break;
    case 1: B(at::kTradeQuantity) = static_cast<unsigned char>(v); break;
    case 2: B(at::kTradeAnswer) = static_cast<unsigned char>(v & 1); break;
    case 3: B(at::kTradeStep) = static_cast<unsigned char>(v % 2); break;
    case 4: B(at::kTradeState) = static_cast<unsigned char>(w); break;
    case 5: B(at::kTradeRow) = static_cast<unsigned char>(v % 10); break;
    case 6: B(at::kTradeRowCount) = static_cast<unsigned char>(v % 11); break;
    default: break;
    }
}

// --- the seed ---------------------------------------------------------------------------------------

// Every function: the row one of the rows with entries (1, 2, 8) most often,
// the row count 0..10, the pick inside it with bit 7 / bit 6 at times, the
// quantity at its boundaries (and negative as an s8), the answer 0 / 1, the
// step below the leave table's two, the buttons as FE2's seeds draw them.
void Seed(unsigned) {
    B(at::kTradeRow) = static_cast<unsigned char>(PickOf(1, 2, 8, sh::Next() % 10));
    const unsigned rows = sh::Next() % 11;
    B(at::kTradeRowCount) = static_cast<unsigned char>(rows);
    B(at::kTradePick) = static_cast<unsigned char>((sh::Often() ? 0 : PickOf(0x40, 0x80, 0xC0)) | (sh::Next() % (rows ? rows : 1)));
    B(at::kTradeQuantity) = static_cast<unsigned char>(PickOf(0, 1, 2, 0x62, 0x63, 0x80, 0xFF, sh::Next() % 100));
    B(at::kTradeAnswer) = static_cast<unsigned char>(sh::Next() % 2);
    B(at::kTradeStep) = static_cast<unsigned char>(sh::Next() % 2);
    const U pressed = sh::Next() & 0xFFFF;
    SetW(0x7E1BEC, pressed);   // Input_Pressed
    SetW(at::kConfirm, sh::Half() ? pressed & (1u << (sh::Next() % 16)) : 0);
    SetW(at::kCancel, sh::Half() ? pressed & (1u << (sh::Next() % 16)) : 0);
    SetW(at::kWaitWord, sh::Half() ? 0 : sh::Next());
}

// The arguments each kCall function reads (garbage above the bytes it masks).
void Args(unsigned k, U* a) {
    const U hi = a[9] & 0xFFFFFF00u;
    switch (k) {
    case kList: a[0] = hi | PickOf(0, 1, sh::Next() & 0xFF); break;
    case kListFrame: a[2] = hi | PickOf(0, 1, sh::Next() & 0xFF); break;
    case kLacks:
        a[0] = hi | (sh::Often() ? sh::Next() % 10 : sh::Next() & 0xFF);
        a[1] = (a[8] & 0xFFFFFF00u) | PickOf(0, 1, 2, 0x63, sh::Next() % 100, sh::Next() & 0xFF);
        break;
    case kIcon:
        if (sh::Next() % 3 == 0) a[2] &= 0xFFFFFF00u;   // no item
        break;
    default: break;
    }
}

// A subset for a control or a hunt (BOF3X_E1G_ONLY=first,count; the enum's
// numbering) and the rounds (BOF3X_E1G_ROUNDS); the committed run is every
// function at 6,000.
unsigned g_first;
void SeedFrom(unsigned k) { Seed(k + g_first); }
void ArgsFrom(unsigned k, U* a) { Args(k + g_first, a); }

}  // namespace

void SelfTest() {
    unsigned first = 0, count = kCount, rounds = 6000;
    if (const char* only = std::getenv("BOF3X_E1G_ONLY")) {
        char* end = nullptr;
        first = static_cast<unsigned>(std::strtoul(only, &end, 10));
        if (end && *end == ',') count = static_cast<unsigned>(std::strtoul(end + 1, nullptr, 10));
        if (first >= kCount) first = kCount - 1;
        if (count == 0 || first + count > kCount) count = kCount - first;
    }
    if (const char* r = std::getenv("BOF3X_E1G_ROUNDS")) rounds = static_cast<unsigned>(std::strtoul(r, nullptr, 10));
    g_first = first;
    sh::Group group = {"effect_1g", kClones + first, count, kCallees, sizeof kCallees / sizeof kCallees[0],
                       kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                       &SeedFrom, &Disturb, rounds};
    group.args = &ArgsFrom;
    group.field = true;
    sh::Run(group);
}

}  // namespace effect_1g
