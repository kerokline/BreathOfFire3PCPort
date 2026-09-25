// BOF3X_SHADOW=menu_draw_helpers: a differential fuzz of group DI's
// twenty-five functions against byte-copies of Capcom's, once at start-up
// (docs/menu_draw_helpers.md section 3).
//
// Every call out of a copy is re-aimed at a recording stand-in, and ours is put
// on the same stand-ins through menu_draw_helpers::g. The eleven .data dispatch
// tables the functions jump or call through (the two kind tables and the nine
// step tables) have their entries swapped for recorders for the run - both
// sides read the same .data - and put back after. MenuWin_SlideOutLeft's bound
// is the imm32 in its copy (+6), which ours is aimed at and each round seeds.
//
// Per round: one function; random bytes over the window records, the current
// record pointer, the window colour, the zenny and party list, and the member
// map 0x66972C; the current record put back on one of the 22; the indices put
// inside their tables and each branch's boundaries seeded; theirs, then ours
// from the same state; every region and the stand-ins' log compared. The
// stand-ins record only what their callee reads (the low word of a coordinate,
// the low byte of a byte argument) and between calls change something a caller
// reads again after it - once in 23 the current record pointer itself.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/menu_draw_helpers_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace menu_draw_helpers {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U Address(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U XY(int x, int y) { return static_cast<std::uint16_t>(x) | static_cast<U>(static_cast<std::uint16_t>(y)) << 16; }

// --- the stand-ins' log ------------------------------------------------------

constexpr unsigned kLog = 16;
struct Entry { U what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n;
U g_seed;

U Hash() {
    U h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(U what, U a = 0, U b = 0, U c = 0, U d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}

// A byte some caller reads again after a call, or (1 in 23) the current
// record pointer: from a hash, so both sides see the same change.
void Disturb() {
    const U h = Hash();
    if (h % 3 == 0) return;
    if (h % 23 == 0) {
        SetLong(At(at::kCurrent), static_cast<std::int32_t>(at::kRecords + ((h >> 8) % 22u) * 0x24u));
        return;
    }
    static const unsigned kField[] = {3, 4, 5, 6, 7, 0xA, 0xB, 0xC, 0x10, 0x11};
    unsigned char* const w = At(static_cast<U>(Long(At(at::kCurrent))));
    const auto v = static_cast<unsigned char>(h >> 12);
    switch ((h >> 4) % 8) {
    case 0: At(at::kColour)[0] = v; break;
    case 1: At(at::kZenny + ((h >> 20) & 3))[0] = v; break;
    case 2: At(at::kPartyList + ((h >> 20) % 3))[0] = v; break;
    case 3: At(at::kMemberRecord + ((h >> 20) & 0xFF))[0] = v; break;
    default: w[kField[(h >> 20) % 10]] = v; break;
    }
}

// The fake strings Msg_SystemPtr answers.
unsigned char g_text[0x100];

void __cdecl StubTitleBox(int x, int y, int w, int h, int colour) {
    Record(1, XY(x, y), static_cast<std::uint16_t>(w), static_cast<std::uint16_t>(h), static_cast<U>(colour) & 0xFF);
    Disturb();
}
const unsigned char* __cdecl StubMsg(unsigned id) {
    Record(2, id & 0xFFFF);
    const unsigned char* const r = g_text + (Hash() & 0x7F);
    Disturb();
    return r;
}
const unsigned char* __cdecl StubText(int x, int y, int colour, int count, const unsigned char* text) {
    Record(3, XY(x, y), static_cast<U>(colour), static_cast<U>(count), Address(text));
    Disturb();
    return text;
}
void __cdecl StubButtons(int x, int y, int set, int sel, int) {
    Record(4, XY(x, y), static_cast<U>(set) & 0xFF, static_cast<U>(sel) & 0xFF);
    Disturb();
}
void __cdecl StubMoney(int x, int y, int, unsigned value) {
    Record(5, XY(x, y), value);
    Disturb();
}
void __cdecl StubCursor(int x, int y, int w, int h, int blink, int flags) {
    Record(6, XY(x, y), static_cast<std::uint16_t>(w), static_cast<std::uint16_t>(h),
           (static_cast<U>(blink) & 0xFF) | (static_cast<U>(flags) & 0xFF) << 8);
    Disturb();
}
void __cdecl StubMember(int x, int y, unsigned member, unsigned slot, unsigned item) {
    Record(7, XY(x, y), member & 0xFF, slot & 0xFF, item & 0xFF);
    Disturb();
}
void __cdecl StubEquip(int x, int y, unsigned member) {
    Record(8, XY(x, y), member & 0xFF);
    Disturb();
}
template <U Id_> void __cdecl StubWindow(unsigned char* window) {
    Record(Id_, Address(window));
    Disturb();
}
void __cdecl StubHand(int x, int y, int) {
    Record(13, XY(x, y));
    Disturb();
}
template <U Id_> void __cdecl StubHandler() {
    Record(Id_);
    Disturb();
}

const void* StubFor(U target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case bof3::addr::Menu_DrawTitleBox: return f(&StubTitleBox);
    case bof3::addr::Msg_SystemPtr: return f(&StubMsg);
    case bof3::addr::Text_DrawAt: return f(&StubText);
    case bof3::addr::Menu_DrawButtonRow: return f(&StubButtons);
    case bof3::addr::Menu_DrawMoneyBox: return f(&StubMoney);
    case bof3::addr::Menu_DrawCursorBox: return f(&StubCursor);
    case bof3::addr::Shop_DrawMemberStats: return f(&StubMember);
    case bof3::addr::Menu_DrawEquipPanel: return f(&StubEquip);
    case bof3::addr::Shop_DrawBuyList: return f(&StubWindow<9>);
    case bof3::addr::Shop_DrawBuyDetail: return f(&StubWindow<10>);
    case bof3::addr::Menu_DrawItemList: return f(&StubWindow<11>);
    case bof3::addr::Shop_DrawSellDetail: return f(&StubWindow<12>);
    case bof3::addr::Menu_DrawHand: return f(&StubHand);
    case bof3::addr::BattleMenu_DrawItemList: return f(&StubWindow<14>);
    case bof3::addr::BattleMenu_DrawSkillList: return f(&StubWindow<15>);
    default: bof3::Fatal("menu_draw_helpers: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

Callees Stubs() {
    return {StubTitleBox, StubMsg,       StubText,       StubButtons,    StubMoney,
            StubCursor,   StubMember,    StubEquip,      StubWindow<9>,  StubWindow<10>,
            StubWindow<11>, StubWindow<12>, StubHand,    StubWindow<14>, StubWindow<15>,
            0};   // slide_left_bound: set to the copy's immediate
}

// --- the dispatch tables ------------------------------------------------------
// What each holds (read 2026-09-25), refused if it holds anything else; each
// entry becomes a recorder numbered 100 * (table + 1) + entry.

constexpr U kRet = 0x437CC0;
struct Table { U at; unsigned n; U had[19]; };
const Table kTables[] = {
    {table::kHandler7Kinds, 19, {0x59B240, 0x59B350, 0x59B310, 0x59CB20, 0x59B3C0, 0x59B3F0, 0x59B4F0, 0x59B560, 0x59B810,
                                 0x59BB60, 0x59BBB0, 0x59BE50, 0x59BEA0, 0x59BEC0, 0x59BF00, 0x59C110, 0x59C190, 0x59C1E0,
                                 0x59C260}},
    {table::kTitleSteps, 3, {kRet, 0x59A3A0, 0x59A3D0}},
    {table::kButtonsSteps, 3, {kRet, 0x59A3A0, 0x59A680}},
    {table::kMoneySteps, 3, {kRet, 0x59B390, 0x59A680}},
    {table::kMemberSteps, 5, {kRet, 0x59B440, 0x59A5B0, 0x59B470, 0x59B4A0}},
    {table::kEquipSteps, 3, {kRet, 0x59B440, 0x59B530}},
    {table::kBuyListSteps, 4, {kRet, 0x59A5E0, 0x59B7B0, 0x59B7E0}},
    {table::kItemListSteps, 3, {kRet, 0x59A5E0, 0x59BB80}},
    {table::kHandler8Kinds, 6, {0x59CB20, 0x59CB40, 0x59CBC0, 0x59CC10, 0x59CC90, 0x59CCE0}},
    {table::kBattleItemSteps, 5, {kRet, 0x59A580, 0x59CB60, 0x59CB90, 0x59A5E0}},
    {table::kBattleSkillSteps, 3, {kRet, 0x59A5E0, 0x59CBE0}},
};
constexpr unsigned kTableCount = sizeof kTables / sizeof kTables[0];

using Handler = void (__cdecl*)();
// 19 recorders a table is enough for the longest.
template <unsigned T_> constexpr Handler kRow[19] = {
    &StubHandler<100 * (T_ + 1) + 0>,  &StubHandler<100 * (T_ + 1) + 1>,  &StubHandler<100 * (T_ + 1) + 2>,
    &StubHandler<100 * (T_ + 1) + 3>,  &StubHandler<100 * (T_ + 1) + 4>,  &StubHandler<100 * (T_ + 1) + 5>,
    &StubHandler<100 * (T_ + 1) + 6>,  &StubHandler<100 * (T_ + 1) + 7>,  &StubHandler<100 * (T_ + 1) + 8>,
    &StubHandler<100 * (T_ + 1) + 9>,  &StubHandler<100 * (T_ + 1) + 10>, &StubHandler<100 * (T_ + 1) + 11>,
    &StubHandler<100 * (T_ + 1) + 12>, &StubHandler<100 * (T_ + 1) + 13>, &StubHandler<100 * (T_ + 1) + 14>,
    &StubHandler<100 * (T_ + 1) + 15>, &StubHandler<100 * (T_ + 1) + 16>, &StubHandler<100 * (T_ + 1) + 17>,
    &StubHandler<100 * (T_ + 1) + 18>,
};
const Handler* const kRows[kTableCount] = {kRow<0>, kRow<1>, kRow<2>, kRow<3>, kRow<4>, kRow<5>,
                                           kRow<6>, kRow<7>, kRow<8>, kRow<9>, kRow<10>};

void SwapTables(U (*saved)[19]) {
    for (unsigned t = 0; t < kTableCount; ++t) {
        const Table& tb = kTables[t];
        for (unsigned i = 0; i < tb.n; ++i) {
            const U had = static_cast<U>(Long(At(tb.at + 4 * i)));
            if (had != tb.had[i])
                bof3::Fatal("menu_draw_helpers: table 0x%X entry %u holds 0x%X, not 0x%X", static_cast<unsigned>(tb.at), i,
                            static_cast<unsigned>(had), static_cast<unsigned>(tb.had[i]));
            saved[t][i] = had;
            SetLong(At(tb.at + 4 * i), static_cast<std::int32_t>(Address(reinterpret_cast<const void*>(kRows[t][i]))));
        }
    }
}
void RestoreTables(const U (*saved)[19]) {
    for (unsigned t = 0; t < kTableCount; ++t)
        for (unsigned i = 0; i < kTables[t].n; ++i) SetLong(At(kTables[t].at + 4 * i), static_cast<std::int32_t>(saved[t][i]));
}

// --- the copies ------------------------------------------------------------------

struct Call { U offset, target; };
struct Clone { const char* name; U base, size; const Call* calls; int n_calls; const void* ours; };

constexpr Call kTitleCalls[] = {{0x2F, bof3::addr::Menu_DrawTitleBox}, {0x46, bof3::addr::Msg_SystemPtr},
                                {0x6A, bof3::addr::Text_DrawAt},       {0x95, bof3::addr::Msg_SystemPtr},
                                {0xB9, bof3::addr::Text_DrawAt}};
constexpr Call kButtonsCalls[] = {{0x2B, bof3::addr::Menu_DrawButtonRow}};
constexpr Call kMoneyCalls[] = {{0x2A, bof3::addr::Menu_DrawMoneyBox}};
constexpr Call kCursorCalls[] = {{0x25, bof3::addr::Menu_DrawCursorBox}};
constexpr Call kMemberCalls[] = {{0x3D, bof3::addr::Shop_DrawMemberStats}};
constexpr Call kEquipCalls[] = {{0x35, bof3::addr::Menu_DrawEquipPanel}};
constexpr Call kBuyListCalls[] = {{0x19, bof3::addr::Shop_DrawBuyList}};
constexpr Call kBuyDetailCalls[] = {{0x6, bof3::addr::Shop_DrawBuyDetail}};
constexpr Call kItemListCalls[] = {{0x19, bof3::addr::Menu_DrawItemList}};
constexpr Call kSellCalls[] = {{0x6, bof3::addr::Shop_DrawSellDetail}};
constexpr Call kHandCalls[] = {{0x11, bof3::addr::Menu_DrawHand}};
constexpr Call kBattleItemCalls[] = {{0x19, bof3::addr::BattleMenu_DrawItemList}};
constexpr Call kBattleSkillCalls[] = {{0x19, bof3::addr::BattleMenu_DrawSkillList}};

const void* V(void (__cdecl* f)()) { return reinterpret_cast<const void*>(f); }

enum : unsigned {
    kH7, kTitle, kButtons, kMoney, kMoneyUp, kCursor, kMember, kLeft, kMemberUp, kMemberDown, kEquip, kEquipIn,
    kBuyList, kBuy84, kBuy46, kBuyDetail, kItemList, kItemIn, kSell, kH8, kHand, kBattleItems, kBattleRight,
    kBattleSkills, kBattleSkillIn, kCount
};

#define DI_C(name, base, size, calls, ours) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), V(&ours)}
#define DI_P(name, base, size, ours) {name, base, size, nullptr, 0, V(&ours)}
// The extents are the disassembly's, to the last instruction (capstone,
// 2026-09-25).
const Clone kClones[kCount] = {
    DI_P("Window_Handler7Kinds", 0x59B220, 0x12, Window_Handler7Kinds),
    DI_C("ShopWin_TitleRun", 0x59B240, 0xC2, kTitleCalls, ShopWin_TitleRun),
    DI_C("ShopWin_ButtonsRun", 0x59B310, 0x34, kButtonsCalls, ShopWin_ButtonsRun),
    DI_C("ShopWin_MoneyRun", 0x59B350, 0x33, kMoneyCalls, ShopWin_MoneyRun),
    DI_P("ShopWin_MoneySlideUp", 0x59B390, 0x28, ShopWin_MoneySlideUp),
    DI_C("ShopWin_CursorBoxDraw", 0x59B3C0, 0x2E, kCursorCalls, ShopWin_CursorBoxDraw),
    DI_C("ShopWin_MemberStatsRun", 0x59B3F0, 0x46, kMemberCalls, ShopWin_MemberStatsRun),
    DI_P("MenuWin_SlideOutLeft", 0x59B440, 0x28, MenuWin_SlideOutLeft),
    DI_P("ShopWin_MemberSlideUp", 0x59B470, 0x28, ShopWin_MemberSlideUp),
    DI_P("ShopWin_MemberSlideDown", 0x59B4A0, 0x4C, ShopWin_MemberSlideDown),
    DI_C("ShopWin_EquipRun", 0x59B4F0, 0x3E, kEquipCalls, ShopWin_EquipRun),
    DI_P("ShopWin_EquipSlideIn", 0x59B530, 0x28, ShopWin_EquipSlideIn),
    DI_C("ShopWin_BuyListRun", 0x59B560, 0x20, kBuyListCalls, ShopWin_BuyListRun),
    DI_P("ShopWin_BuyListSlideTo84", 0x59B7B0, 0x28, ShopWin_BuyListSlideTo84),
    DI_P("ShopWin_BuyListSlideTo46", 0x59B7E0, 0x28, ShopWin_BuyListSlideTo46),
    DI_C("ShopWin_BuyDetailRun", 0x59B810, 0xD, kBuyDetailCalls, ShopWin_BuyDetailRun),
    DI_C("ShopWin_ItemListRun", 0x59BB60, 0x20, kItemListCalls, ShopWin_ItemListRun),
    DI_P("ShopWin_ItemListSlideIn", 0x59BB80, 0x28, ShopWin_ItemListSlideIn),
    DI_C("ShopWin_SellDetailRun", 0x59BBB0, 0xD, kSellCalls, ShopWin_SellDetailRun),
    DI_P("Window_Handler8Kinds", 0x59CB00, 0x12, Window_Handler8Kinds),
    DI_C("MenuWin_Hand", 0x59CB20, 0x1A, kHandCalls, MenuWin_Hand),
    DI_C("BattleMenuWin_ItemListRun", 0x59CB40, 0x20, kBattleItemCalls, BattleMenuWin_ItemListRun),
    DI_P("BattleMenuWin_ItemListSlideRight", 0x59CB60, 0x26, BattleMenuWin_ItemListSlideRight),
    DI_C("BattleMenuWin_SkillListRun", 0x59CBC0, 0x20, kBattleSkillCalls, BattleMenuWin_SkillListRun),
    DI_P("BattleMenuWin_SkillListSlideIn", 0x59CBE0, 0x28, BattleMenuWin_SkillListSlideIn),
};
#undef DI_C
#undef DI_P

// --- the state both passes start from ----------------------------------------------

struct Region { U at, size; };
const Region kRegions[] = {
    {at::kRecords, 0x318},       // the 22 window records
    {at::kCurrent, 4},           // the record running (Fix puts it on one of the 22)
    {at::kColour, 1},
    {at::kZenny, 0x10A},         // the zenny, and the party list with every byte +0xA can reach
    {at::kMemberRecord, 0x100},  // the member map, every byte a party-list byte can reach
};
constexpr unsigned kRegionBytes = 0x318 + 4 + 1 + 0x10A + 0x100;

struct State {
    unsigned char memory[kRegionBytes];
    std::uint32_t bound;
    Entry log[kLog];
    unsigned log_n;
};
unsigned char* g_bound;   // the copy of MenuWin_SlideOutLeft's imm32
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(s.memory + at, At(r.at), r.size);
        at += r.size;
    }
    std::memcpy(&s.bound, g_bound, 4);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(At(r.at), s.memory + at, r.size);
        at += r.size;
    }
    std::memcpy(g_bound, &s.bound, 4);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

U g_rng = 0x2F6B79D1u;
U Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool Often() { return Next() % 3 != 0; }

unsigned char* Current() { return At(static_cast<U>(Long(At(at::kCurrent)))); }

// A word at or around a slide's arrival: `bound - by` lands exactly on it.
std::uint16_t Around(int bound, int by) {
    static const int kD[] = {0, 1, -1, 2, -2, 0x10, -0x10, 0x20, -0x20, 0x40};
    if (!Often()) return static_cast<std::uint16_t>(Next());
    return static_cast<std::uint16_t>(bound - by + kD[Next() % 10]);
}

void Seed(unsigned k) {
    unsigned char* const r = Current();
    switch (k) {
    case kH7: r[2] = static_cast<unsigned char>(Next() % 19); break;
    case kH8: r[2] = static_cast<unsigned char>(Next() % 6); break;
    case kTitle: {
        r[3] = static_cast<unsigned char>(Next() % 3);
        static const std::uint16_t kId[] = {0, 0, 0x36, 0x49, 0x4A, 0x52, 0x35, 0x37, 0x48, 0x4B, 0x51, 0x53, 0xF,
                                            0x136, 0x8036};
        SetWord(r + 0x10, Often() ? kId[Next() % 15] : static_cast<std::uint16_t>(Next()));
        break;
    }
    case kButtons:
    case kMoney:
    case kEquip:
    case kItemList:
    case kBattleSkills: r[3] = static_cast<unsigned char>(Next() % 3); break;
    case kMember:
    case kBattleItems: r[3] = static_cast<unsigned char>(Next() % 5); break;
    case kBuyList: r[3] = static_cast<unsigned char>(Next() % 4); break;
    case kCursor: r[0xA] = static_cast<unsigned char>(Often() ? Next() % 4 : Next()); break;
    case kMoneyUp: {
        static const int kY[] = {-4, -3, -5, -20, -19, -21, -36, 0, 0x10, 0x7FF0};
        SetWord(r + 6, Often() ? static_cast<std::uint16_t>(kY[Next() % 10]) : static_cast<std::uint16_t>(Next()));
        break;
    }
    case kLeft: {
        static const int kBound[] = {-150, -150, -203, -1, 0, 0x7FFF, -0x8000};
        const int bound = Often() ? kBound[Next() % 7] : static_cast<short>(Next());
        const U imm = (Next() % 2 ? 0xFFFF0000u : Next() & 0xFFFF0000u) | static_cast<std::uint16_t>(bound);
        std::memcpy(g_bound, &imm, 4);
        SetWord(r + 4, Around(bound, -0x20));
        break;
    }
    case kMemberUp: SetWord(r + 6, Around(0x3E, -0x10)); break;
    case kMemberDown: {
        r[0xA] = static_cast<unsigned char>(Often() ? Next() % 4 : Next());
        SetWord(r + 6, Around(r[0xA] * 0x36 + 0x3E, 0x10));
        break;
    }
    case kEquipIn: SetWord(r + 4, Around(0xF, 0x20)); break;
    case kBuy84: SetWord(r + 4, Around(0x84, -0x20)); break;
    case kBuy46: SetWord(r + 4, Around(0x46, -0x20)); break;
    case kItemIn: SetWord(r + 4, Around(0x4B, -0x20)); break;
    case kBattleRight: SetWord(r + 4, Around(0x53, 0x20)); break;
    case kBattleSkillIn: SetWord(r + 4, Around(0x53, -0x20)); break;
    default: break;
    }
}

// What the rounds reached, from the original's side.
struct Coverage { unsigned title_none, title_one, title_two, stopped, moving, repointed; } g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    unsigned texts = 0;
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what == 3) ++texts;
    if (k == kTitle) {
        if (texts == 0) ++g_cover.title_none;
        else if (texts == 1) ++g_cover.title_one;
        else ++g_cover.title_two;
    }
    if (std::memcmp(in.memory + 0x318, out.memory + 0x318, 4) != 0) ++g_cover.repointed;
    const U record = static_cast<U>(Long(in.memory + 0x318));
    const unsigned slot = (record - at::kRecords) / 0x24u;
    if (slot < 22 && out.log_n == 0 && k != kH7 && k != kH8) {   // a slide: no call out
        if (in.memory[slot * 0x24 + 3] != 0 && out.memory[slot * 0x24 + 3] == 0) ++g_cover.stopped;
        else ++g_cover.moving;
    }
}

}  // namespace

void SelfTest() {
    constexpr unsigned kPer = 2000;
    constexpr unsigned kRounds = kPer * kCount;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("menu_draw_helpers: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[5];
        if (c.n_calls > 5) bof3::Fatal("menu_draw_helpers: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    auto* const left = static_cast<unsigned char*>(clones[kLeft]);
    if (left[5] != 0xB9) bof3::Fatal("menu_draw_helpers: MenuWin_SlideOutLeft's copy +5 is 0x%02X, not mov ecx", left[5]);
    g_bound = left + 6;
    for (unsigned i = 0; i < sizeof g_text; ++i) g_text[i] = static_cast<unsigned char>(i);

    static U saved_tables[kTableCount][19];
    static State saved, input, their_out, our_out;
    unsigned char bound_had[4];
    std::memcpy(bound_had, g_bound, 4);
    Capture(saved);
    SwapTables(saved_tables);
    g = Stubs();
    g.slide_left_bound = Address(g_bound);

    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        for (unsigned i = 0; i < kRegionBytes; ++i) input.memory[i] = static_cast<unsigned char>(Next());
        std::memcpy(&input.bound, bound_had, 4);
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        SetLong(At(at::kCurrent), static_cast<std::int32_t>(at::kRecords + (Next() % 22u) * 0x24u));
        Seed(k);
        g_seed = Next();
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const void* const fn = pass ? kClones[k].ours : clones[k];
            reinterpret_cast<void (__cdecl*)()>(const_cast<void*>(fn))();
            Capture(pass ? our_out : their_out);
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      menu_draw_helpers self-test MISMATCH: round %u, %s, log %u / %u", round,
                          kClones[k].name, their_out.log_n, our_out.log_n);
        }
    }
    g = kOriginals;
    RestoreTables(saved_tables);
    Apply(saved);
    std::memcpy(g_bound, bound_had, 4);

    bof3::Log("shadow      menu_draw_helpers self-test: %u rounds over %u functions (%u each), %u calls to the "
              "stand-ins, %u MISMATCHES; the 22 window records, the current record, the window colour, the zenny and "
              "party list, the member map, the slide bound and the stand-ins' log compared",
              kRounds, static_cast<unsigned>(kCount), kPer, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      menu_draw_helpers: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      menu_draw_helpers coverage: title with no text %u, one %u, two %u; slides stopped %u, moving "
              "%u; current record repointed %u",
              c.title_none, c.title_one, c.title_two, c.stopped, c.moving, c.repointed);
    if (bad) bof3::Fatal("the menu draw helpers differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace menu_draw_helpers
