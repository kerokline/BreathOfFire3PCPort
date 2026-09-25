// BOF3X_SHADOW=shop_states2: a differential fuzz of the shop's buy and sell
// states, the shop's close and task 0's title loop, once at start-up.
// docs/shop_states2.md section 4.
//
// Twenty-four byte-copies, every call out re-aimed at a recording stand-in
// (bof3::CloneCall with `expected`); the dispatch tables 0x664118..0x66418C
// (thirty dwords, the five step tables and the two after them) and task 0's
// mode table 0x667294 pointed at recorders; the shop's list pointer
// 0x903844 and the inventory's list pointers 0x656B00 / 0x656B14 at buffers
// of our own. One round: one function, random bytes in every region any of
// them touches, the pointers and indices put back inside what the buffers
// hold, each branch's boundaries seeded; theirs, then from the same state
// ours; the regions, the buffers and the stand-ins' log compared. The title
// loop never returns: the Task_Sleep recorder long-jumps out of it, from
// both sides, after one to four sleeps. Everything is put back afterwards.
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/shop_states2_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace shop_states2 {
namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Address(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutWord(U address, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
void PutByte(U address, U v) { At(address)[0] = static_cast<unsigned char>(v); }

// --- the long jump out of the title loop ------------------------------------
// Our own setjmp - ebx, esi, edi, ebp, esp and the return address - as
// mode_flow_fuzz.cpp has it.
std::uint32_t g_jump[6];
extern "C" __attribute__((naked, returns_twice)) int __cdecl ShopStates2JumpSave(std::uint32_t* buffer) {
    asm("movl 4(%esp), %eax\n\t"
        "movl %ebx, 0(%eax)\n\t"
        "movl %esi, 4(%eax)\n\t"
        "movl %edi, 8(%eax)\n\t"
        "movl %ebp, 12(%eax)\n\t"
        "leal 4(%esp), %ecx\n\t"
        "movl %ecx, 16(%eax)\n\t"
        "movl (%esp), %ecx\n\t"
        "movl %ecx, 20(%eax)\n\t"
        "xorl %eax, %eax\n\t"
        "ret");
}
extern "C" __attribute__((naked, noreturn)) void __cdecl ShopStates2JumpBack(std::uint32_t* buffer) {
    asm("movl 4(%esp), %eax\n\t"
        "movl 0(%eax), %ebx\n\t"
        "movl 4(%eax), %esi\n\t"
        "movl 8(%eax), %edi\n\t"
        "movl 12(%eax), %ebp\n\t"
        "movl 16(%eax), %esp\n\t"
        "movl 20(%eax), %ecx\n\t"
        "movl $1, %eax\n\t"
        "jmp *%ecx");
}

// --- the buffers the pointers are aimed at ----------------------------------

// The shop's list: 0x903844 points 0x100 in, so that a row of -128..127
// (read as a signed byte, times 2) stays inside.
constexpr unsigned kListAt = 0x100;
unsigned char g_list[0x300];
// The inventory's five id and five count lists: 0x656B00 / 0x656B14 point
// 0x80 in - rows -128..255.
constexpr unsigned kInvAt = 0x80;
unsigned char g_ids[5][0x180];
unsigned char g_counts[5][0x180];
// What the Item_NamePtr recorder answers.
unsigned char g_names[0x40];

// --- the stand-ins' log -----------------------------------------------------

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d, memory; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;
unsigned g_sleeps, g_sleep_limit, g_loads, g_load_after;
bool g_touched;   // a recorder moved the state or the step byte

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
// The bytes the functions store, hashed at every call: a store moved across
// a call shows even where the callee does not read it.
std::uint32_t Watched() {
    static const struct { std::uint32_t at, size; } kWatched[] = {
        {0x929F00, 0x20}, {0x6BC880, 0x40}, {0x803160, 0x320}, {at::kMoney, 0x10}, {at::kGameMode, 4}, {at::kClutDirty, 1}};
    std::uint32_t h = 0x811C9DC5u;
    for (const auto& r : kWatched)
        for (std::uint32_t i = 0; i < r.size; ++i) h = (h ^ At(r.at)[i]) * 0x01000193u;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, Watched()};
    ++g_log_n;
}

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Categories stay 0..4 and tabs 0..3 (they index the list
// pointers), the list pointer on one of two places in the buffer, the mode
// 0 or 1.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 26) {
    case 0: PutByte(at::kSub, v); g_touched = true; break;
    case 1: PutByte(at::kState, v); g_touched = true; break;
    case 2: PutByte(at::kCounter, v); break;
    case 3: PutByte(at::kMember, v % 5 == 0 ? v : v % 3); break;
    case 4: PutByte(at::kAnswer, v % 3 == 0 ? v : v % 2); break;
    case 5: PutByte(at::kKind, v % 5); break;
    case 6: PutByte(at::kItem, v); break;
    case 7: PutByte(at::kRow, v % 7 == 0 ? v : v % 6); break;
    case 8: PutByte(at::kCount, v % 4 == 0 ? v : v % 12); break;
    case 9: PutLong(at::kMaxCount, v % 2 ? v % 40 : h); break;
    case 10: PutLong(at::kUnitPrice, v % 2 ? v * 3 : h >> 8); break;
    case 11: PutLong(at::kMoney, v % 2 ? w : h); break;
    case 12: PutLong(at::kPressed, h * 0x2C1B3C6Du); break;
    case 13: PutLong(at::kShopList, Address(g_list + kListAt + (v % 2) * 2)); break;
    case 14: g_list[kListAt + w % 0x40] = static_cast<unsigned char>(v); break;
    case 15: PutByte(0x803266, v % 4); break;
    case 16: PutByte(0x803267 + (w & 1), v % 3 == 0 ? v : v % 0x80); break;
    case 17: PutWord(0x80326E, v % 3 == 0 ? v : 0); break;
    case 18: PutByte(0x803283, v % 2); break;
    case 19: PutByte(at::kSellsEquipment, v % 3 == 0 ? v : v % 2); break;
    case 20: PutWord(at::kGameMode, v % 2); break;
    case 21: PutByte(at::kClutDirty, v); break;
    case 22: PutByte(at::kSlotCursor, v % 3 == 0 ? v : v % 2); break;
    case 23: g_counts[w % 5][kInvAt + (v % 0x90) - 0x10] = static_cast<unsigned char>(h >> 8); break;
    case 24: PutWord(at::kRate, h); break;
    default: PutByte(0x803264, v); break;
    }
}

// --- the stand-ins ------------------------------------------------------------

// The thirty dwords of the dispatch tables and the two of the mode table.
template <unsigned N> void __cdecl StubHandler() { Record(200 + N, At(at::kSub)[0], At(at::kState)[0]); Disturb(); }

void __cdecl StubInitWindows() { Record(1); Disturb(); }
void __cdecl StubPriceRate(unsigned short* rate) {
    Record(2, Address(rate));
    *rate = static_cast<unsigned short>(Hash());
    Disturb();
}
// Never 0 (as Shop_ScalePrice, which answers 1 for 0); mostly a price a few
// zenny wide so that the money's quotient lands near 99 and near 0.
unsigned __cdecl StubScalePrice(unsigned price, unsigned rate) {
    Record(3, price, rate & 0xFFFF);
    Disturb();
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return (h >> 4) | 1;
    return 1 + (h >> 8) % 300;
}
unsigned __cdecl StubSellPrice(unsigned kind, unsigned item, unsigned flag) {
    Record(4, kind & 0xFF, item & 0xFF, flag & 0xFF);
    Disturb();
    return Hash() * 0x61C88647u;
}
void __cdecl StubEquip(unsigned record, unsigned slot, unsigned item) {
    Record(5, record & 0xFF, slot & 0xFF, item & 0xFF);
    Disturb();
}
// The icon kind: 5 (two slots), 0xA and 0xB (not equipment) often, the
// upper bytes anything.
unsigned __cdecl StubIconKind(unsigned kind, unsigned item) {
    Record(6, kind & 0xFF, item & 0xFF);
    Disturb();
    static const unsigned char kKinds[] = {0, 1, 2, 3, 4, 5, 5, 0xA, 0xA, 0xB, 0xB, 6, 0xF};
    const std::uint32_t h = Hash();
    return (h & ~0xFFu) | kKinds[(h >> 3) % sizeof kKinds];
}
unsigned __cdecl StubHelpId(unsigned kind, unsigned item) {
    Record(7, kind & 0xFF, item & 0xFF);
    Disturb();
    return Hash();
}
unsigned __cdecl StubBasePrice(unsigned kind, unsigned item) {
    Record(8, kind & 0xFF, item & 0xFF);
    Disturb();
    return Hash() * 0x9E3779B1u;
}
// Counts around the 99 edge (two of them summed) half the time.
unsigned short __cdecl StubInvCount(unsigned kind, unsigned item, unsigned equipped) {
    Record(9, kind & 0xFF, item & 0xFF, equipped & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    if (h % 2) return static_cast<unsigned short>((h >> 16 & 0xFF00) | (45 + (h >> 4) % 12));
    if (h % 3 == 0) return static_cast<unsigned short>((h >> 16) & 0xFF00);
    return static_cast<unsigned short>(h >> 9);
}
unsigned char __cdecl StubInvAdd(unsigned kind, unsigned item, unsigned count) {
    Record(10, kind & 0xFF, item & 0xFF, count & 0xFF);
    Disturb();
    return static_cast<unsigned char>(Hash());
}
unsigned char* __cdecl StubNamePtr(unsigned kind, unsigned item) {
    Record(11, kind & 0xFF, item & 0xFF);
    Disturb();
    return g_names + Hash() % 0x20;
}
void __cdecl StubTextSet(unsigned slot, unsigned length, const unsigned char* text) {
    Record(12, slot & 0xFF, length & 0xFF, Address(text));
    Disturb();
}
unsigned __cdecl StubEquipMask(unsigned kind, unsigned item) {
    Record(13, kind & 0xFF, item & 0xFF);
    Disturb();
    return Hash();
}
// Item_CanUse reads the member's low five bits (mode 4).
unsigned char __cdecl StubCanUse(unsigned mode, unsigned member, unsigned kind, unsigned item) {
    Record(14, mode & 0xFF, member & 0x1F, kind & 0xFF, item & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : h >> 8);
}
void __cdecl StubRecalc(unsigned char* record) { Record(15, Address(record)); Disturb(); }
// Party_Count's low byte 0..3 (and 6 now and then), anything above it.
int __cdecl StubPartyCount(unsigned slot) {
    Record(16, slot & 0xFF);
    Disturb();
    static const unsigned char kCounts[] = {0, 1, 1, 2, 2, 3, 3, 3, 6};
    const std::uint32_t h = Hash();
    return static_cast<int>((h & ~0xFFu) | kCounts[(h >> 5) % sizeof kCounts]);
}
// The repeat: one of the bits the states test half the time, anything
// otherwise.
unsigned __cdecl StubAutoRepeat(unsigned pressed) {
    Record(17, pressed & 0xFFFF);
    Disturb();
    static const unsigned kBits[] = {0x8000, 0x2000, 0x4000, 0x1000, 4, 8, 0x5000, 0xA000, 0, 0x9000, 0x6000};
    const std::uint32_t h = Hash();
    if (h % 2) return (h & 0xFFFF0000u) | kBits[(h >> 3) % (sizeof kBits / sizeof kBits[0])];
    return h * 0x2C1B3C6Du;
}
void __cdecl StubSound(unsigned short id) { Record(18, id); Disturb(); }
void __cdecl StubWindowReset() { Record(19); Disturb(); }
void __cdecl StubTaskClear() { Record(20); Disturb(); }
void __cdecl StubTaskSleep(int frames) {
    Record(21, static_cast<std::uint32_t>(frames));
    if (g_sleep_limit != 0 && ++g_sleeps >= g_sleep_limit) ShopStates2JumpBack(g_jump);
    Disturb();
}
void __cdecl StubRunTaskRecords() { Record(22); Disturb(); }
void __cdecl StubLoadDat(int file) { Record(23, static_cast<std::uint32_t>(file)); Disturb(); }
int __cdecl StubLoadDone() {
    Record(24);
    Disturb();
    return ++g_loads > g_load_after ? static_cast<int>(Hash() | 1) : 0;
}
void __cdecl StubClutRestore() { Record(25); Disturb(); }

const Callees kStubs = {
    StubInitWindows, StubPriceRate, StubScalePrice, StubSellPrice, StubEquip,
    StubIconKind, StubHelpId, StubBasePrice, StubInvCount, StubInvAdd, StubNamePtr, StubTextSet,
    StubEquipMask, StubCanUse, StubRecalc, StubPartyCount,
    StubAutoRepeat, StubSound,
    StubWindowReset, StubTaskClear, StubTaskSleep, StubRunTaskRecords, StubLoadDat, StubLoadDone, StubClutRestore,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x583140: return f(&StubInitWindows);
    case 0x583020: return f(&StubPriceRate);
    case 0x5830D0: return f(&StubScalePrice);
    case 0x583100: return f(&StubSellPrice);
    case 0x583210: return f(&StubEquip);
    case 0x591720: return f(&StubIconKind);
    case 0x591C20: return f(&StubHelpId);
    case 0x5749F0: return f(&StubBasePrice);
    case 0x5919B0: return f(&StubInvCount);
    case 0x590BB0: return f(&StubInvAdd);
    case 0x591680: return f(&StubNamePtr);
    case 0x591940: return f(&StubTextSet);
    case 0x5917A0: return f(&StubEquipMask);
    case 0x57D9A0: return f(&StubCanUse);
    case 0x590660: return f(&StubRecalc);
    case 0x531BB0: return f(&StubPartyCount);
    case 0x461EB0: return f(&StubAutoRepeat);
    case 0x587740: return f(&StubSound);
    case 0x59E330: return f(&StubWindowReset);
    case 0x5A99F4: return f(&StubTaskClear);
    case 0x5A9949: return f(&StubTaskSleep);
    case 0x59E230: return f(&StubRunTaskRecords);
    case 0x454590: return f(&StubLoadDat);
    case 0x454810: return f(&StubLoadDone);
    case 0x4549B0: return f(&StubClutRestore);
    default: bof3::Fatal("shop_states2: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// The dispatch tables' thirty dwords (0x664118..0x66418F) and the mode
// table's two.
constexpr unsigned kTableDwords = (at::kTablesEnd - at::kStates) / 4;
static_assert(kTableDwords == 30, "the shop's dispatch tables are thirty dwords");
using Handler = void (__cdecl*)();
template <unsigned... N> struct Handlers { static constexpr Handler kAll[] = {&StubHandler<N>...}; };
const Handler* const kTableStubs =
    Handlers<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29>::kAll;
const Handler kModeStubs[2] = {&StubHandler<40>, &StubHandler<41>};

// --- the copies ---------------------------------------------------------------

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
};

// Every E8 out of each extent (capstone, 2026-09-25); every other transfer
// stays inside, but for the dispatches' absolute `jmp [table]` and the title
// loop's `call [table]`, whose tables are swapped instead.
constexpr Call kOpenCalls[] = {{0x3, 0x583140}, {0x4A, 0x591720}, {0x79, 0x583020}, {0x91, 0x583020}};
constexpr Call kChoiceCalls[] = {{0x4F, 0x461EB0}, {0x61, 0x587740}, {0x89, 0x587740}, {0x93, 0x587740}, {0xE1, 0x587740}};
constexpr Call kBuySetupCalls[] = {{0x13, 0x531BB0}, {0x6D, 0x531BB0}};
constexpr Call kBuyListCalls[] = {{0x32, 0x531BB0},  {0x5E, 0x591720},  {0x77, 0x531BB0},  {0x90, 0x591C20},
                                  {0xFD, 0x461EB0},  {0x151, 0x587740}, {0x1A5, 0x5749F0}, {0x1B3, 0x5830D0},
                                  {0x1DD, 0x5919B0}, {0x1F4, 0x5919B0}, {0x25C, 0x587740}, {0x279, 0x587740},
                                  {0x293, 0x587740}, {0x2A0, 0x531BB0}, {0x2C8, 0x531BB0}};
constexpr Call kBuyCountCalls[] = {{0xA1, 0x461EB0},  {0xF2, 0x587740},  {0x141, 0x587740},
                                   {0x167, 0x587740}, {0x188, 0x587740}, {0x19F, 0x587740}};
constexpr Call kBuyConfirmCalls[] = {{0xD, 0x591680},  {0x17, 0x591940}, {0x70, 0x587740}, {0xA8, 0x587740},
                                     {0xDF, 0x590BB0}, {0xF1, 0x591720}, {0x137, 0x587740}};
constexpr Call kBuyAskEquipCalls[] = {{0xD, 0x591680}, {0x17, 0x591940}, {0x70, 0x587740}, {0xA0, 0x587740}, {0xCA, 0x587740}};
constexpr Call kBuyMemberCalls[] = {{0xE, 0x591680},  {0x18, 0x591940},  {0x5E, 0x531BB0},  {0x73, 0x461EB0},
                                    {0xC3, 0x587740}, {0xEC, 0x5917A0},  {0x11C, 0x531BB0}, {0x14F, 0x531BB0},
                                    {0x1A0, 0x587740}, {0x1D0, 0x587740}, {0x1E8, 0x587740}};
constexpr Call kBuySlotCalls[] = {{0xF, 0x591720},   {0x63, 0x461EB0},  {0x9D, 0x587740},  {0xB4, 0x591680},
                                  {0xBE, 0x591940},  {0xE3, 0x587740},  {0x10F, 0x583210}, {0x139, 0x590660},
                                  {0x15E, 0x587740}, {0x16B, 0x531BB0}, {0x1AB, 0x531BB0}};
constexpr Call kBuyEquippedCalls[] = {{0xE, 0x591680}, {0x18, 0x591940}, {0x47, 0x531BB0}, {0x87, 0x531BB0}, {0xA9, 0x5919B0}};
constexpr Call kSellListCalls[] = {{0x2C, 0x591C20},  {0xA0, 0x461EB0},  {0xB4, 0x587740},  {0xE6, 0x587740},
                                   {0x1F5, 0x587740}, {0x28F, 0x587740}, {0x2AA, 0x57D9A0}, {0x2BC, 0x587740},
                                   {0x307, 0x583100}, {0x328, 0x587740}, {0x341, 0x587740}};
constexpr Call kSellCountCalls[] = {{0x97, 0x461EB0}, {0xE8, 0x587740}, {0x137, 0x587740}, {0x153, 0x587740}, {0x17D, 0x587740}};
constexpr Call kSellConfirmCalls[] = {{0xD, 0x591680}, {0x17, 0x591940}, {0x70, 0x587740}, {0xA8, 0x587740}, {0x167, 0x587740}};
constexpr Call kCloseCalls[] = {{0xE, 0x59E330}};
constexpr Call kRunCalls[] = {{0xE, 0x5A99F4}, {0x13, 0x59E330}, {0x1A, 0x5A9949}, {0x33, 0x59E230}};
constexpr Call kLoadCalls[] = {{0x5, 0x454590}, {0xD, 0x454810}, {0x18, 0x5A9949}, {0x20, 0x454810}, {0x29, 0x4549B0}};

enum : unsigned {
    kStep, kOpenStep, kOpen, kOpenWait, kChoiceStep, kChoice, kBuyStep, kBuySetup, kBuyList, kBuyCount, kBuyConfirm,
    kBuyAskEquip, kBuyMember, kBuySlot, kBuyEquipped, kSellStep, kSellSetup, kSellList, kSellCount, kSellConfirm,
    kSellClose, kClose, kRun, kLoad, kCount
};

#define SS_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0])}
#define SS_P(name, base, size) {name, base, size, nullptr, 0}
const Clone kClones[kCount] = {
    SS_P("ShopTrade_Step", 0x5818B0, 0xE),
    SS_P("ShopTrade_OpenStep", 0x5818C0, 0xE),
    SS_C("ShopTrade_Open", 0x5818D0, 0x9D, kOpenCalls),
    SS_P("ShopTrade_OpenWait", 0x581970, 0x3D),
    SS_P("ShopTrade_ChoiceStep", 0x5819B0, 0xE),
    SS_C("ShopTrade_Choice", 0x5819C0, 0x11C, kChoiceCalls),
    SS_P("ShopTrade_BuyStep", 0x581AE0, 0xE),
    SS_C("ShopTrade_BuySetup", 0x581AF0, 0xE8, kBuySetupCalls),
    SS_C("ShopTrade_BuyList", 0x581BE0, 0x2ED, kBuyListCalls),
    SS_C("ShopTrade_BuyCount", 0x581ED0, 0x1B4, kBuyCountCalls),
    SS_C("ShopTrade_BuyConfirm", 0x582090, 0x147, kBuyConfirmCalls),
    SS_C("ShopTrade_BuyAskEquip", 0x5821E0, 0xDF, kBuyAskEquipCalls),
    SS_C("ShopTrade_BuyMember", 0x5822C0, 0x205, kBuyMemberCalls),
    SS_C("ShopTrade_BuySlot", 0x5824D0, 0x1CD, kBuySlotCalls),
    SS_C("ShopTrade_BuyEquipped", 0x5826A0, 0xCE, kBuyEquippedCalls),
    SS_P("ShopTrade_SellStep", 0x582770, 0xE),
    SS_P("ShopTrade_SellSetup", 0x582780, 0x6B),
    SS_C("ShopTrade_SellList", 0x5827F0, 0x366, kSellListCalls),
    SS_C("ShopTrade_SellCount", 0x582B60, 0x192, kSellCountCalls),
    SS_C("ShopTrade_SellConfirm", 0x582D00, 0x177, kSellConfirmCalls),
    SS_P("ShopTrade_SellClose", 0x582E80, 0x24),
    SS_C("ShopTrade_Close", 0x584F70, 0x1B, kCloseCalls),
    SS_C("TitleTask_Run", 0x588E70, 0x3A, kRunCalls),
    SS_C("TitleMode_Load", 0x588EB0, 0x44, kLoadCalls),
};
#undef SS_C
#undef SS_P

// --- the state both passes start from ---------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x929F00, 0x20},            // the menus' state block
    {0x6BC880, 0x40},            // the shop's bytes
    {0x803160, 0x320},           // the window records, to 0x803480
    {at::kShopList, 4},
    {at::kMoney, 0x10},          // the money, the party list
    {0x7E1BE8, 8},               // Input_Held, Input_Pressed
    {0x903580, 0x14},            // the pad map: confirm, cancel
    {at::kGameMode, 4},          // Game_Mode, Game_Step
    {at::kClutDirty, 4},
    {at::kRecordOf, 0x18},       // constant data from here on - random here, put back after
    {at::kIdLists, 0x28},        // the inventory's list pointers
};
constexpr unsigned kRegionBytes = 0x20 + 0x40 + 0x320 + 4 + 0x10 + 8 + 0x14 + 4 + 4 + 0x18 + 0x28;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char list[sizeof g_list];
    unsigned char ids[sizeof g_ids];
    unsigned char counts[sizeof g_counts];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.list, g_list, sizeof g_list);
    std::memcpy(s.ids, g_ids, sizeof g_ids);
    std::memcpy(s.counts, g_counts, sizeof g_counts);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_list, s.list, sizeof g_list);
    std::memcpy(g_ids, s.ids, sizeof g_ids);
    std::memcpy(g_counts, s.counts, sizeof g_counts);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}
unsigned Byte(const State& s, std::uint32_t address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + (address - r.at)];
        at += r.size;
    }
    return 0x100;
}
std::uint32_t LongOf(const State& s, std::uint32_t address) {
    return Byte(s, address) | Byte(s, address + 1) << 8 | Byte(s, address + 2) << 16 | Byte(s, address + 3) << 24;
}

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

// Random bytes put back inside what the buffers hold: the list pointer and
// the ten inventory pointers on our buffers, the list's count inside it,
// categories 0..4, the tab 0..3, the member mostly a member, the mode 0 / 1.
void Fix() {
    PutLong(at::kShopList, Address(g_list + kListAt));
    for (unsigned k = 0; k < 5; ++k) {
        PutLong(at::kIdLists + k * 4, Address(g_ids[k] + kInvAt));
        PutLong(at::kCountLists + k * 4, Address(g_counts[k] + kInvAt));
    }
    g_list[kListAt] = static_cast<unsigned char>(Often() ? Next() % 9 : Next() % 0x80);
    for (unsigned i = 0; i < 0x80; ++i)
        if (Half()) g_list[kListAt + 1 + i * 2] = static_cast<unsigned char>(Next() % 5);   // categories, some 0
    PutByte(at::kKind, Next() % 5);
    PutByte(0x803266, Next() % 4);
    PutByte(at::kMember, Often() ? Next() % 3 : Next());
    PutByte(at::kRow, Often() ? Next() % 8 : Next());
    PutWord(at::kGameMode, Next() % 2);
    if (Half()) PutWord(0x80326E, 0);
}

// The buttons: the confirm and cancel words, and a pressed word that hits
// confirm, cancel, both or neither.
void Buttons() {
    const U confirm = 1u << (Next() % 16), cancel = Half() ? 1u << (Next() % 16) : (Next() & 0xFFFF);
    PutWord(at::kConfirm, confirm);
    PutWord(at::kCancel, cancel);
    U pressed = Next();
    switch (Next() % 4) {
    case 0: pressed |= confirm; break;
    case 1: pressed = (pressed & ~confirm) | cancel; break;
    case 2: pressed &= ~(confirm | cancel); break;
    default: break;
    }
    if (Next() % 8 == 0) pressed &= 0xFFFF0000u;
    PutLong(at::kPressed, pressed);
}

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    Buttons();
    g_sleep_limit = 0;
    g_sleeps = 0;
    g_loads = 0;
    g_load_after = 0;
    // indexes of the dispatches: inside the table mostly, into the tables
    // after it now and then (inside the thirty dwords swapped)
    const auto index = [](unsigned real, unsigned into) { return Often() ? Next() % real : Next() % into; };
    switch (k) {
    case kStep: PutByte(at::kState, index(5, 30)); break;
    case kOpenStep: PutByte(at::kSub, index(2, 25)); break;
    case kChoiceStep: PutByte(at::kSub, index(1, 23)); break;
    case kBuyStep: PutByte(at::kSub, index(8, 22)); break;
    case kSellStep: PutByte(at::kSub, index(5, 14)); break;
    case kOpenWait:
    case kSellClose:
    case kClose: {
        static const unsigned char kCounters[] = {0, 1, 1, 2, 5, 0xFF};
        PutByte(at::kCounter, kCounters[Next() % sizeof kCounters]);
        break;
    }
    case kChoice: PutByte(at::kChoice, Often() ? Next() % 2 : Next()); break;
    case kBuySetup: PutByte(at::kSellsEquipment, Often() ? Next() % 2 : Next()); break;
    case kBuyList: {
        const unsigned n = g_list[kListAt];
        PutByte(at::kRow, Often() && n ? Next() % n : Next() % 3);
        PutByte(0x803283, Often() ? 0 : Next());
        if (Half()) PutLong(at::kMoney, Next() % 30000);
        break;
    }
    case kBuyCount:
    case kSellCount: {
        static const unsigned char kCounts[] = {0, 1, 1, 2, 9, 10, 11, 0x7F, 0x80, 0xFF};
        PutByte(at::kCount, kCounts[Next() % sizeof kCounts]);
        static const std::uint32_t kMost[] = {0, 1, 2, 10, 11, 20, 99, 0x80, 0xFF, 0x100};
        PutLong(at::kMaxCount, Often() ? kMost[Next() % 10] : Next());
        break;
    }
    case kBuyConfirm:
    case kBuyAskEquip:
    case kSellConfirm:
        PutByte(at::kAnswer, Often() ? Next() % 2 : Next());
        if (k == kSellConfirm && Half()) PutLong(at::kMoney, at::kMoneyMax - Next() % 3000);
        if (k == kSellConfirm && Half()) {
            const unsigned kind = Next() % 5;
            PutByte(at::kKind, kind);
            g_counts[kind][kInvAt + static_cast<signed char>(At(at::kRow)[0])] = static_cast<unsigned char>(At(at::kCount)[0]);
        }
        if (k == kBuyConfirm && Half()) PutByte(at::kSellsEquipment, 1);
        break;
    case kBuyMember:
    case kBuySlot:
    case kBuyEquipped:
        if (Half()) PutByte(at::kSlotCursor, Next() % 2);
        if (k == kBuyEquipped && Half()) PutLong(at::kPressed, Next() & 0xFFFF0000u);
        for (unsigned i = 0; i < 3; ++i) PutByte(at::kPartyList + i, Next() % 24);
        break;
    case kSellList: {
        static const unsigned char kTops[] = {0, 0, 1, 5, 8, 9, 10, 0x6E, 0x6F, 0x70, 0x76, 0x77, 0x78};
        static const unsigned char kRows[] = {0, 0, 1, 8, 9, 0x7E, 0x7F, 0x80, 0xFF};
        const unsigned top = kTops[Next() % sizeof kTops];
        PutByte(0x803267, top);
        PutByte(0x803268, Half() ? top + Next() % 10 : kRows[Next() % sizeof kRows]);
        PutByte(0x80325F, Often() ? 0 : Next());
        break;
    }
    case kRun: g_sleep_limit = 1 + Next() % 4; break;
    case kLoad: g_load_after = Next() % 4; break;
    default: break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Pair { int state, sub; };
struct Coverage {
    Pair exits[kCount][10];
    unsigned n_exits[kCount];
    std::uint32_t sounds[kCount][32];
    unsigned n_sounds[kCount];
    unsigned calls[kCount];
    unsigned held_full, room_cut, afforded, double_sound, clamped_one, clamped_most, money_capped, emptied;
    unsigned page_up, page_down, scrolled, tabs, can_use_no, sleeps_cut, waited;
} g_cover;
unsigned Logged(const State& s, std::uint32_t what, std::uint32_t a = 0xFFFFFFFFu) {
    unsigned n = 0;
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i)
        if (s.log[i].what == what && (a == 0xFFFFFFFFu || s.log[i].a == a)) ++n;
    return n;
}
void Cover(unsigned k, const State& in, const State& out, bool touched) {
    g_cover.calls[k] += out.log_n;
    // the state and step moves as signed bytes; a store of an absolute value
    // (state 1 step 0) shows as whatever the difference was
    const Pair p = {static_cast<signed char>(Byte(out, at::kState) - Byte(in, at::kState)),
                    static_cast<signed char>(Byte(out, at::kSub) - Byte(in, at::kSub))};
    // the sounds played, in order: each branch plays its own
    std::uint32_t sounds = 0x811C9DC5u;
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what == 18) sounds = (sounds ^ out.log[i].a) * 0x01000193u;
    bool known = false;
    for (unsigned i = 0; i < g_cover.n_sounds[k]; ++i) known = known || g_cover.sounds[k][i] == sounds;
    if (!known && g_cover.n_sounds[k] < 32) g_cover.sounds[k][g_cover.n_sounds[k]++] = sounds;
    // only rounds where no recorder moved the state or the step say which exit was taken
    if (!touched) {
        bool seen = false;
        for (unsigned i = 0; i < g_cover.n_exits[k]; ++i)
            if (g_cover.exits[k][i].state == p.state && g_cover.exits[k][i].sub == p.sub) seen = true;
        if (!seen && g_cover.n_exits[k] < 10) g_cover.exits[k][g_cover.n_exits[k]++] = p;
    }
    switch (k) {
    case kBuyList:
        if (Logged(out, 9) == 2) {
            if (LongOf(out, at::kMaxCount) == 0 && Byte(out, at::kCount) == 0) ++g_cover.held_full;
            else if (LongOf(out, at::kMaxCount) < 99) ++g_cover.room_cut;
            if (Logged(out, 18, 0x103)) ++g_cover.afforded;
        }
        break;
    case kBuyCount:
    case kSellCount: {
        const unsigned id = k == kBuyCount ? 0x100 : 0x101;
        if (Logged(out, 18, id) == 2) ++g_cover.double_sound;
        if (Byte(out, at::kCount) == 1 && Byte(in, at::kCount) != 1) ++g_cover.clamped_one;
        if (Byte(out, at::kCount) == Byte(in, at::kMaxCount) && Byte(in, at::kCount) != Byte(in, at::kMaxCount)) ++g_cover.clamped_most;
        break;
    }
    case kSellConfirm:
        if (LongOf(out, at::kMoney) == at::kMoneyMax) ++g_cover.money_capped;
        if (Logged(out, 18, 0x104)) {
            const unsigned kind = Byte(in, at::kKind);
            const int row = static_cast<signed char>(Byte(in, at::kRow));
            if (kind < 5 && out.counts[kind * 0x180 + kInvAt + row] == 0) ++g_cover.emptied;
        }
        break;
    case kSellList:
        if (Byte(out, 0x803267) + 9 == Byte(in, 0x803267) || (Byte(in, 0x803267) != 0 && Byte(out, 0x803267) == 0)) ++g_cover.page_up;
        if (Byte(out, 0x803267) == Byte(in, 0x803267) + 9 || (Byte(in, 0x803267) != 0x77 && Byte(out, 0x803267) == 0x77)) ++g_cover.page_down;
        if (Byte(out, 0x80326E) != 0 && Byte(in, 0x80326E) == 0 && Byte(in, 0x80326F) == 0) ++g_cover.scrolled;
        if (Logged(out, 18, 0x101)) ++g_cover.tabs;
        if (Logged(out, 14) && Logged(out, 18, 0x107)) ++g_cover.can_use_no;
        break;
    case kRun:
        if (Logged(out, 21)) ++g_cover.sleeps_cut;
        break;
    case kLoad:
        if (Logged(out, 24) > 1) ++g_cover.waited;
        break;
    default:
        break;
    }
}

using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

// One call, cut short by the Task_Sleep recorder's long jump when it is the
// title loop.
__attribute__((noinline)) void Run(const void* fn) {
    if (ShopStates2JumpSave(g_jump) != 0) return;
    reinterpret_cast<Fn4>(const_cast<void*>(fn))(0, 0, 0, 0);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("shop_states2: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        if (c.n_calls > 16) bof3::Fatal("shop_states2: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&ShopTrade_Step),        reinterpret_cast<const void*>(&ShopTrade_OpenStep),
        reinterpret_cast<const void*>(&ShopTrade_Open),        reinterpret_cast<const void*>(&ShopTrade_OpenWait),
        reinterpret_cast<const void*>(&ShopTrade_ChoiceStep),  reinterpret_cast<const void*>(&ShopTrade_Choice),
        reinterpret_cast<const void*>(&ShopTrade_BuyStep),     reinterpret_cast<const void*>(&ShopTrade_BuySetup),
        reinterpret_cast<const void*>(&ShopTrade_BuyList),     reinterpret_cast<const void*>(&ShopTrade_BuyCount),
        reinterpret_cast<const void*>(&ShopTrade_BuyConfirm),  reinterpret_cast<const void*>(&ShopTrade_BuyAskEquip),
        reinterpret_cast<const void*>(&ShopTrade_BuyMember),   reinterpret_cast<const void*>(&ShopTrade_BuySlot),
        reinterpret_cast<const void*>(&ShopTrade_BuyEquipped), reinterpret_cast<const void*>(&ShopTrade_SellStep),
        reinterpret_cast<const void*>(&ShopTrade_SellSetup),   reinterpret_cast<const void*>(&ShopTrade_SellList),
        reinterpret_cast<const void*>(&ShopTrade_SellCount),   reinterpret_cast<const void*>(&ShopTrade_SellConfirm),
        reinterpret_cast<const void*>(&ShopTrade_SellClose),   reinterpret_cast<const void*>(&ShopTrade_Close),
        reinterpret_cast<const void*>(&TitleTask_Run),         reinterpret_cast<const void*>(&TitleMode_Load)};

    static State saved, input, their_out, our_out;
    std::uint32_t saved_tables[kTableDwords], saved_modes[2];
    std::memcpy(saved_tables, At(at::kStates), sizeof saved_tables);
    std::memcpy(saved_modes, At(at::kModes), sizeof saved_modes);
    Capture(saved);
    g = kStubs;
    for (unsigned i = 0; i < kTableDwords; ++i) PutLong(at::kStates + 4 * i, Address(reinterpret_cast<const void*>(kTableStubs[i])));
    for (unsigned i = 0; i < 2; ++i) PutLong(at::kModes + 4 * i, Address(reinterpret_cast<const void*>(kModeStubs[i])));

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.list) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.ids) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.counts) b = static_cast<unsigned char>(Next() % 4 == 0 ? Next() % 3 : Next());
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        g_seed = Next();
        Seed(k);
        Capture(input);
        const std::uint32_t sleep_limit = g_sleep_limit, load_after = g_load_after;
        bool touched = false;

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            g_sleep_limit = sleep_limit;
            g_sleeps = 0;
            g_load_after = load_after;
            g_loads = 0;
            g_touched = false;
            Run(pass ? ours[k] : clones[k]);
            Capture(pass ? our_out : their_out);
            if (pass == 0) touched = g_touched;
        }
        calls += their_out.log_n;
        Cover(k, input, their_out, touched);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      shop_states2 self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    std::memcpy(At(at::kStates), saved_tables, sizeof saved_tables);
    std::memcpy(At(at::kModes), saved_modes, sizeof saved_modes);
    Apply(saved);

    bof3::Log("shadow      shop_states2 self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the menu block, the shop's bytes, the window records, the money and party list, the "
              "input, the pad map, the mode, the lists and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      shop_states2: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    for (unsigned k = 0; k < kCount; ++k) {
        char exits[160];
        unsigned n = 0;
        exits[0] = 0;
        for (unsigned i = 0; i < c.n_exits[k] && n + 16 < sizeof exits; ++i) {
            const Pair& p = c.exits[k][i];
            const int w = std::snprintf(exits + n, sizeof exits - n, "%s%+d/%+d", i ? " " : "", p.state, p.sub);
            if (w > 0) n += static_cast<unsigned>(w);
        }
        bof3::Log("shadow      shop_states2 coverage: %s - %u stand-in calls, %u sound sequences, moves (state/step) %s",
                  kClones[k].name, c.calls[k], c.n_sounds[k], exits);
    }
    bof3::Log("shadow      shop_states2 coverage: buy list held 99+ %u, room cut %u, afforded %u; counts: double sound %u, "
              "to 1 %u, to the most %u; sell: money capped %u, stack emptied %u, page up %u, page down %u, scroll %u, "
              "tabs %u, not sellable %u; title loop cut %u, load waited %u",
              c.held_full, c.room_cut, c.afforded, c.double_sound, c.clamped_one, c.clamped_most, c.money_capped,
              c.emptied, c.page_up, c.page_down, c.scrolled, c.tabs, c.can_use_no, c.sleeps_cut, c.waited);
    if (bad) bof3::Fatal("the shop's buy and sell states differ from the original in %u self-test rounds", bad);
}

}  // namespace shop_states2
