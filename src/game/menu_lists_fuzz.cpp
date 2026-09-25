// BOF3X_SHADOW=menu_lists: a differential fuzz of the field menu and the list
// draws, once at start-up. docs/menu_lists.md section 4.
//
// Twenty byte-copies, every call out re-aimed at a recording stand-in
// (bof3::CloneCall with `expected`); the two .data dispatch blocks the
// originals jump and call through - the menu's states and top-bar steps
// 0x6672B4..0x6672E3 and handler 6's kinds and five state tables
// 0x66AF94..0x66B043 - swapped entry by entry for recorders. One round: one
// function, random bytes in every region any of them touches, the pointers
// and indices put back inside what the tables hold, each branch's boundaries
// seeded; theirs, then from the same state ours; the regions, the primitive
// pool, the text buffer, the packet cursor and the stand-ins' log compared.
// Everything is put back afterwards.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/menu_lists_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace menu_lists {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void SetPtr(std::uint32_t address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }
unsigned char& B(std::uint32_t address) { return At(address)[0]; }
unsigned char* Record_(unsigned i) { return At(at::kRecords + i * at::kRecordSize); }
unsigned char* Cur() { return At(static_cast<std::uint32_t>(Long(At(at::kCurrent)))); }

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 200;
struct Entry { std::uint32_t what, a, b, c, d, e, f; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

constexpr unsigned kPool = 0x200;
unsigned char g_prim[kPool];
constexpr unsigned kText = 0x40;
unsigned char g_text[kText];   // what the Msg_SystemPtr stand-in hands back

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0, std::uint32_t f = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e, f};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p);
    const std::uint32_t pool = Address(g_prim), text = Address(g_text);
    if (at >= pool && at < pool + sizeof g_prim) return 0x10000u + (at - pool);
    if (at >= text && at < text + sizeof g_text) return 0x20000u + (at - text);
    return at;
}
std::uint32_t Current() { return static_cast<std::uint32_t>(Long(At(at::kCurrent))); }

// Coordinates near the slide bounds and the screen's edges, and anything.
std::uint16_t Coordinate(std::uint32_t h) {
    static const std::int16_t kEdges[] = {-253, -201, -200, -199, -168, -21, -20, -19, -4, 0,  0x10, 0x11, 0x12,
                                          0x26, 0x27, 0x31, 0x62, 0x68, 0x73, 0xC8, 0x13F, 0x140, 0x141, 0x175, 0x7FFF,
                                          -0x8000};
    if (h % 3 == 0) return static_cast<std::uint16_t>(h >> 16);
    return static_cast<std::uint16_t>(kEdges[(h >> 4) % (sizeof kEdges / sizeof kEdges[0])]);
}

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the records, indices mostly inside the
// tables.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 24) {
    case 0: SetPtr(at::kCurrent, Record_(v % 22)); break;
    case 1: Cur()[3] = static_cast<unsigned char>(v % 3); break;
    case 2: SetWord(Cur() + 4, Coordinate(h * 0x9E3779B1u)); break;
    case 3: SetWord(Cur() + 6, Coordinate(h * 0x85EBCA6Bu)); break;
    case 4: Cur()[0xA] = static_cast<unsigned char>(v % 3 == 0 ? v : v % 8); break;
    case 5: Cur()[0xB] = static_cast<unsigned char>(v % 8); break;
    case 6: Cur()[0xC] = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 7: SetWord(Cur() + 0x10, static_cast<std::uint16_t>(static_cast<int>(v % 7) - 2)); break;
    case 8: B(at::kMenu) = static_cast<unsigned char>(v % 9); break;
    case 9: B(at::kStep) = static_cast<unsigned char>(v % 3); break;
    case 10: B(at::kCursor) = static_cast<unsigned char>(v % 3 == 0 ? v : v % 8); break;
    case 11: B(at::kCampFlag) = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 12: SetWord(At(at::kPressed), h >> 16); break;
    case 13: Sprite_Current = Record_(v % 22); break;
    case 14: At(at::kParty)[w % 6] = static_cast<unsigned char>(v % 3 == 0 ? 0xFF : v % 4); break;
    case 15: B(at::kMemberCount) = static_cast<unsigned char>(v % 5); break;
    case 16: At(at::kSavedParty)[w % 6] = static_cast<unsigned char>(v % 3 == 0 ? 0xFF : v % 4); break;
    case 17: At(at::kReserveData)[w % 9] = static_cast<unsigned char>(v); break;
    case 18: B(at::kColour) = static_cast<unsigned char>(v); break;
    case 19: B(at::kBackground) = static_cast<unsigned char>(v); break;
    case 20: Gfx_PacketNext = g_prim + v; break;
    case 21: B(at::kIconWindow + 0xC) = static_cast<unsigned char>(v % 2 ? 0 : v); break;
    case 22: At(at::kCharRecords + 0x10 + (w % 8) * at::kCharRecordSize)[v % 0x20] = static_cast<unsigned char>(h); break;
    default: SetLong(At(at::kMoney), static_cast<std::int32_t>(h * 0x9E3779B1u)); break;
    }
}
// Half the time, the current record repointed: what every handler-6 function
// re-reads after a call.
void Repoint() {
    if (Hash() % 2) SetPtr(at::kCurrent, Record_((Hash() >> 8) % 22));
}

// --- the stand-ins ---------------------------------------------------------

// The dispatch tables' entries.
template <unsigned N> void __cdecl StubHandler() {
    Record(N, Current(), B(at::kMenu), B(at::kStep), Id(Sprite_Current));
    Repoint();
    Disturb();
}
void __cdecl StubPlace() {
    Record(40, B(at::kMenu), B(at::kStep));
    if (Hash() % 2) B(at::kMenu) = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
}
void __cdecl StubReconcile() {
    Record(41, B(at::kStep), Word(At(at::kGameStep)));
    if (Hash() % 2) B(at::kStep) = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
}
void __cdecl StubDrawMember(unsigned char* rec) {
    Record(42, Id(rec), Current());
    Disturb();
}
void __cdecl StubBackdrop(unsigned kind) {
    Record(43, kind & 0xFF, Id(Sprite_Current));
    if (Hash() % 2) B(at::kCursor) = static_cast<unsigned char>(Hash() % 3 ? (Hash() >> 8) % 7 : Hash() >> 8);
    Disturb();
}
void __cdecl StubRecalc(unsigned char* rec) {
    Record(44, Id(rec), B(at::kMemberCount));
    Disturb();
}
// The count's low byte is what every caller reads; the bits above it are
// anything. Mostly 0..3, now and then up to 8.
int __cdecl StubPartyCount(unsigned slot) {
    Record(45, slot);
    Disturb();
    const std::uint32_t h = Hash();
    const unsigned n = h % 8 == 0 ? (h >> 3) % 9 : (h >> 3) % 4;
    return static_cast<int>((h & 0xFFFFFF00u) | n);
}
unsigned char __cdecl StubGateway() {
    Record(46, Id(Sprite_Current), B(at::kCampFlag));
    if (Hash() % 2) B(at::kCampFlag) = static_cast<unsigned char>(B(at::kCampFlag) ? 0 : 1 + (Hash() >> 8) % 0xFF);
    Disturb();
    return static_cast<unsigned char>(Hash() % 2 ? 0 : Hash() >> 8);
}
unsigned char __cdecl StubCampCell() {
    Record(47, Id(Sprite_Current));
    Disturb();
    return static_cast<unsigned char>(Hash() % 2 ? 0 : Hash() >> 8);
}
unsigned __cdecl StubAutoRepeat(unsigned pressed) {
    Record(48, pressed, Id(Sprite_Current));
    if (Hash() % 2) SetWord(At(at::kPressed), Hash() >> 12);
    Disturb();
    const std::uint32_t h = Hash();
    return (h & ~0xA000u) | (h % 3 == 0 ? 0x8000u : 0) | ((h >> 2) % 3 == 0 ? 0x2000u : 0);
}
void __cdecl StubSound(unsigned short id) {
    Record(49, id, B(at::kCursor));
    if (Hash() % 2) B(at::kCursor) = static_cast<unsigned char>(Hash() % 3 ? (Hash() >> 8) % 8 : Hash() >> 8);
    Disturb();
}
void __cdecl StubWindowReset() {
    Record(50);
    Disturb();
}
void __cdecl StubMemberBody(int x, int y, unsigned record, unsigned flag, int zero) {
    Record(51, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF, record & 0xFF, flag & 0xFF,
           static_cast<std::uint32_t>(zero), Id(Gfx_PacketNext));
    if (Hash() % 2) Gfx_PacketNext = g_prim + (Hash() >> 8) % 0x100;
    Disturb();
}
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(52, Id(prim), static_cast<std::uint32_t>(dfe), static_cast<std::uint32_t>(dtd), tpage, static_cast<std::uint32_t>(tw));
    Disturb();
}
// The commit moves the packet cursor as the real one does - wrapping inside
// our pool - so that a cursor read too early or too late shows.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(53, slot, size, Id(Gfx_PacketNext));
    Gfx_PacketNext = g_prim + (static_cast<unsigned>(Gfx_PacketNext - g_prim) + (size & 0xFF)) % 0x100u;
    Disturb();
}
// Moves the record's fields half the time: MenuList_DrawMemberPanel reads
// them again after it.
unsigned __cdecl StubGetClut(int x, int y) {
    Record(54, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    Disturb();
    return Hash();
}
void __cdecl StubMemberFace(int x, int y, unsigned u, unsigned v, unsigned clut, unsigned shade) {
    Record(55, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF, u & 0xFF, v & 0xFF, clut & 0xFFFF,
           shade & 0xFF);
    Disturb();
}
void __cdecl StubMoneyBox(int x, int y, int unused, unsigned value) {
    Record(56, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF, static_cast<std::uint32_t>(unused),
           value);
    Disturb();
}
void __cdecl StubTimeBox(int x, int y) {
    Record(57, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF);
    Disturb();
}
void __cdecl StubIcon(unsigned icon, unsigned x, unsigned y, unsigned w, unsigned h, unsigned shade) {
    Record(58, icon & 0xFF, x & 0xFFFF, y & 0xFFFF, w & 0xFF, h & 0xFF, shade & 0xFF);
    Repoint();
    Disturb();
}
void __cdecl StubTitleBox(int x, int y, int w, int h, int colour) {
    Record(59, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF, static_cast<std::uint32_t>(w),
           static_cast<std::uint32_t>(h), static_cast<std::uint32_t>(colour) & 0xFF);
    Repoint();
    Disturb();
}
const unsigned char* __cdecl StubMsg(unsigned id) {
    Record(60, id & 0xFFFF);
    Repoint();
    Disturb();
    const std::uint32_t h = Hash();
    for (unsigned i = 0; i < kText; ++i) g_text[i] = static_cast<unsigned char>((h >> (i % 24)) * (i + 1));
    return g_text + h % 8;
}
unsigned char __cdecl StubCharCount(const unsigned char* text) {
    Record(61, Id(text));
    Repoint();
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 4 == 0 ? h >> 8 : (h >> 8) % 12);
}
const unsigned char* __cdecl StubText(int x, int y, int colour, int count, const unsigned char* text) {
    Record(62, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF, static_cast<std::uint32_t>(colour),
           static_cast<std::uint32_t>(count) & 0xFF, Id(text));
    Disturb();
    return text;
}

const Callees kStubs = {
    StubPlace, StubReconcile, StubDrawMember,
    StubBackdrop, StubRecalc, StubPartyCount, StubGateway, StubCampCell, StubAutoRepeat, StubSound, StubWindowReset,
    StubMemberBody, StubDrawMode, StubCommit, StubGetClut, StubMemberFace, StubMoneyBox, StubTimeBox, StubIcon,
    StubTitleBox, StubMsg, StubCharCount, StubText,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x589E60: return f(kStubs.place_windows);
    case 0x589FE0: return f(kStubs.reconcile);
    case 0x599B90: return f(kStubs.draw_member);
    case 0x575690: return f(kStubs.backdrop);
    case 0x590660: return f(kStubs.recalc_stats);
    case 0x531BB0: return f(kStubs.party_count);
    case 0x531820: return f(kStubs.exit_gateway);
    case 0x589FB0: return f(kStubs.camp_cell);
    case 0x461EB0: return f(kStubs.auto_repeat);
    case 0x587740: return f(kStubs.sound);
    case 0x59E330: return f(kStubs.window_reset);
    case 0x573560: return f(kStubs.member_body);
    case 0x5A77C0: return f(kStubs.draw_mode);
    case 0x461E50: return f(kStubs.commit);
    case 0x5A79E0: return f(kStubs.get_clut);
    case 0x5744B0: return f(kStubs.member_face);
    case 0x574610: return f(kStubs.money_box);
    case 0x5746C0: return f(kStubs.time_box);
    case 0x5903F0: return f(kStubs.icon);
    case 0x574AB0: return f(kStubs.title_box);
    case 0x497740: return f(kStubs.msg);
    case 0x57D800: return f(kStubs.char_count);
    case 0x516B30: return f(kStubs.text);
    default: bof3::Fatal("menu_lists: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// The two dispatch blocks: every entry a recorder of its own.
constexpr std::uint32_t kMenuBlock = at::kStates;   // 0x6672B4: 9 states, then 3 top-bar steps
constexpr unsigned kMenuEntries = 12;
constexpr std::uint32_t kListBlock = at::kKinds;    // 0x66AF94: 21 kinds, then 11 + 3 + 3 + 3 + 3 states
constexpr unsigned kListEntries = 44;
using Handler = void (__cdecl*)();
template <unsigned Base, unsigned... I> constexpr auto Stubs(std::integer_sequence<unsigned, I...>) {
    struct T { Handler h[sizeof...(I)]; };
    return T{{&StubHandler<Base + I>...}};
}
constexpr auto kMenuStubs = Stubs<100>(std::make_integer_sequence<unsigned, kMenuEntries>{});
constexpr auto kListStubs = Stubs<120>(std::make_integer_sequence<unsigned, kListEntries>{});

// --- the copies ------------------------------------------------------------

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
};

constexpr Call kOpenCalls[] = {{0x71, 0x590660}, {0x8E, 0x531BB0}, {0xBC, 0x531BB0}, {0x1AA, 0x589E60}};
constexpr Call kInputCalls[] = {{0x08, 0x575690}, {0x5A, 0x531820}, {0x6C, 0x589FB0}, {0x96, 0x461EB0},
                                {0xAD, 0x587740}, {0xDE, 0x587740}, {0x113, 0x589FE0}, {0x150, 0x589FE0},
                                {0x176, 0x587740}, {0x186, 0x587740}, {0x1B9, 0x531BB0}, {0x1DD, 0x531BB0},
                                {0x205, 0x531BB0}, {0x227, 0x531BB0}, {0x24B, 0x531BB0}, {0x26D, 0x531BB0}};
constexpr Call kBackdropCalls[] = {{0x06, 0x575690}};
constexpr Call kPlaceCalls[] = {{0x02, 0x59E330}, {0x0F, 0x531BB0}, {0x69, 0x531BB0}};
constexpr Call kPanelCalls[] = {{0x19, 0x599B90}};
constexpr Call kMemberCalls[] = {{0x32, 0x573560}, {0x46, 0x5A77C0}, {0x4F, 0x461E50}, {0x63, 0x5A79E0}, {0x87, 0x5744B0}};
constexpr Call kMoneyCalls[] = {{0x2A, 0x574610}};
constexpr Call kTimeCalls[] = {{0x21, 0x5746C0}};
constexpr Call kIconCalls[] = {{0x9B, 0x5903F0}, {0xFF, 0x5903F0}};
constexpr Call kTitleCalls[] = {{0x2D, 0x574AB0}, {0x3C, 0x497740}, {0x48, 0x57D800}, {0x63, 0x57D800}, {0x85, 0x516B30}};

enum : unsigned {
    kRun, kOpen, kTopBar, kInput, kBackdrop, kPlace, kReconcile, kListRun, kPanel, kMember, kMoney, kTime, kIcons,
    kTitle, kUpOff, kDown16, kLeftOff, kRight17, kRightOff, kDown38, kCount
};

#define ML_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0])}
#define ML_P(name, base, size) {name, base, size, nullptr, 0}
const Clone kClones[kCount] = {
    ML_P("FieldMenu_Run", 0x589970, 0x11),
    ML_C("FieldMenu_Open", 0x589990, 0x1C3, kOpenCalls),
    ML_P("FieldMenu_TopBar", 0x589B60, 0xE),
    ML_C("FieldMenu_TopBarInput", 0x589B70, 0x287, kInputCalls),
    ML_C("FieldMenu_BackdropStep", 0x589E50, 0xD, kBackdropCalls),
    ML_C("FieldMenu_PlaceWindows", 0x589E60, 0x150, kPlaceCalls),
    ML_P("FieldMenu_ReconcileParty", 0x589FE0, 0xF1),
    ML_P("MenuList_Run", 0x599B50, 0x12),
    ML_C("MenuList_MemberPanel", 0x599B70, 0x20, kPanelCalls),
    ML_C("MenuList_DrawMemberPanel", 0x599B90, 0x91, kMemberCalls),
    ML_C("MenuList_MoneyBox", 0x599D50, 0x33, kMoneyCalls),
    ML_C("MenuList_TimeBox", 0x599DC0, 0x2A, kTimeCalls),
    ML_C("MenuList_TopBarIcons", 0x599E50, 0x117, kIconCalls),
    ML_C("MenuList_TitleBox", 0x599FA0, 0x8F, kTitleCalls),
    ML_P("MenuSlide_UpOff", 0x59A3A0, 0x28),
    ML_P("MenuSlide_DownTo16", 0x59A3D0, 0x27),
    ML_P("MenuSlide_LeftOff", 0x59A580, 0x28),
    ML_P("MenuSlide_RightTo17", 0x59A5B0, 0x28),
    ML_P("MenuSlide_RightOff", 0x59A5E0, 0x28),
    ML_P("MenuSlide_DownTo38", 0x59A680, 0x28),
};
#undef ML_C
#undef ML_P

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kIconIds, 8},           // constant data - random here, put back after: the icon ids
    {at::kTitleIds, 8},          // the title ids
    {at::kMemberRecord, 0x20},   // the character-to-record bytes
    {0x66C7E8, 4},               // Game_Mode, Game_Step
    {at::kSavedParty, 0x10},     // the party as the menu opened on it (a Party_Count of 8 copies past the six)
    {0x7E0670, 8},               // Gfx_PacketNext, 0x7E0674..0x7E0677
    {at::kPressed, 4},
    {at::kRecords, 0x318},       // the 22 window records
    {0x90358C, 8},               // the confirm and cancel words
    {0x903A50, 0x560},           // Config's colour and background, the eight character records
    {0x904040, 0x5D0},           // money, the party lists, 0x904152, the reserve's bytes
    {at::kArea, 4},
    {0x905B60, 0x48},            // 0x905B60, 0x905B84, 0x905BA1, Field_InputFlags
    {0x905D90, 4},
    {0x929EC0, 0x60},            // Field_MemberCount, the menu's state block
    {0x937F88, 8},               // Sprite_Current, 0x937F8E / 0x937F8F
    {0x939880, 0x50},            // the menu's globals FieldMenu_Open zeroes
};
constexpr unsigned kRegionBytes = 8 + 8 + 0x20 + 4 + 0x10 + 8 + 4 + 0x318 + 8 + 0x560 + 0x5D0 + 4 + 0x48 + 4 + 0x60 + 8 + 0x50;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char prim[kPool];
    unsigned char text[kText];
    std::uint32_t packet;   // Gfx_PacketNext, as an offset into the pool
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.prim, g_prim, sizeof g_prim);
    std::memcpy(s.text, g_text, sizeof g_text);
    s.packet = static_cast<std::uint32_t>(Gfx_PacketNext - g_prim);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_prim, s.prim, sizeof g_prim);
    std::memcpy(g_text, s.text, sizeof g_text);
    Gfx_PacketNext = g_prim + s.packet;
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

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

// Random bytes put back inside what the tables and buffers hold: the current
// record and Sprite_Current inside the records, the packet cursor inside the
// pool, the member count small (128 or more never ends FieldMenu_Open's loop,
// in both), the menu's state and step inside their tables, every record's
// kind and state inside handler 6's.
void Fix() {
    SetPtr(at::kCurrent, Record_(Next() % 22));
    Sprite_Current = Record_(Next() % 22);
    Gfx_PacketNext = g_prim + Next() % 0x100;
    B(at::kMemberCount) = static_cast<unsigned char>(Next() % 5);
    B(at::kMenu) = static_cast<unsigned char>(Next() % 9);
    B(at::kStep) = static_cast<unsigned char>(Next() % 3);
    for (unsigned i = 0; i < 22; ++i) {
        unsigned char* const r = Record_(i);
        r[2] = static_cast<unsigned char>(Next() % 5);
        r[3] = static_cast<unsigned char>(Next() % (r[2] == 0 ? 11 : 3));
    }
}

struct Args { std::uint32_t a[4]; };

// Each branch's boundaries, on top of the random bytes.
Args Seed(unsigned k) {
    Args args;
    for (std::uint32_t& v : args.a) v = Next();
    switch (k) {
    case kRun:
        B(at::kMenu) = static_cast<unsigned char>(Next() % 9);
        break;
    case kOpen:
        B(at::kMemberCount) = static_cast<unsigned char>(Next() % 5);
        B(at::kOpenedAt) = static_cast<unsigned char>(Half() ? 1 : Half() ? 0 : Next());
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const r = At(at::kCharRecords + 0x10 + i * at::kCharRecordSize);
            if (Often()) SetWord(r + 8, (Word(r + 0x10) >> 2) + Next() % 3 - 1);
        }
        break;
    case kTopBar:
        B(at::kStep) = static_cast<unsigned char>(Next() % 3);
        break;
    case kInput: {
        static const unsigned char kCursors[] = {0, 1, 2, 3, 4, 5, 6, 6, 6, 7, 0x7F, 0x80, 0x81, 0xFF};
        B(at::kCursor) = kCursors[Next() % sizeof kCursors];
        B(at::kCampFlag) = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
        B(at::kInputFlags) = static_cast<unsigned char>(Half() ? Next() & ~1u : Next() | 1);
        SetWord(At(at::kArea), Half() ? 0xBD : Half() ? 0xBC : Next());
        const unsigned cancel = 1u << (Next() % 16), confirm = 1u << (Next() % 16);
        SetWord(At(at::kCancel), cancel | (Half() ? Next() & 0xFFFF : 0));
        SetWord(At(at::kConfirm), confirm | (Half() ? Next() & 0xFFFF : 0));
        const unsigned c = Word(At(at::kCancel)), f = Word(At(at::kConfirm));
        unsigned pressed = Next() & 0xFFFF;
        switch (Next() % 3) {
        case 0: pressed |= c; break;
        case 1: pressed = (pressed & ~c) | (f & ~c); break;
        default: pressed &= ~(c | f); break;
        }
        SetWord(At(at::kPressed), pressed);
        break;
    }
    case kReconcile:
        for (unsigned i = 0; i < 6; ++i) {
            At(at::kSavedParty)[i] = static_cast<unsigned char>(Next() % 4 == 0 ? 0xFF : Next() % 5);
            At(at::kParty)[i] = static_cast<unsigned char>(Next() % 4 == 0 ? 0xFF : Next() % 5);
        }
        break;
    case kListRun:
        Cur()[2] = static_cast<unsigned char>(Next() % 21);
        break;
    case kMember: {
        unsigned char* const r = Record_(Next() % 22);
        args.a[0] = Address(r);
        break;
    }
    case kIcons: {
        unsigned char* const r = Cur();
        r[0xA] = static_cast<unsigned char>(Often() ? Next() % 7 : Next());
        r[0xB] = static_cast<unsigned char>(Half() ? r[0xA] : Next() % 8);
        static const std::int16_t kCounts[] = {-2, -1, 0, 1, 2, 3, 4, 0x7F, 0xF8, 0x7FFF, -0x8000};
        SetWord(r + 0x10, static_cast<std::uint16_t>(kCounts[Next() % (sizeof kCounts / sizeof kCounts[0])]));
        r[0xC] = static_cast<unsigned char>(Half() ? 0 : Next());
        break;
    }
    case kUpOff:
    case kDown16:
    case kDown38: {
        static const short kBound[] = {-20, 0x10, 0x26};
        const short b = kBound[k == kUpOff ? 0 : k == kDown16 ? 1 : 2];
        const int step = k == kUpOff ? -0x10 : 0x10;
        if (Often()) SetWord(Cur() + 6, static_cast<std::uint16_t>(b - step + static_cast<int>(Next() % 7) - 3));
        break;
    }
    case kLeftOff:
    case kRight17:
    case kRightOff: {
        const short b = k == kLeftOff ? static_cast<short>(Long(At(at::kLeftOffBound)))
                        : k == kRight17 ? short{0x11}
                                        : static_cast<short>(Long(At(at::kRightOffBound)));
        const int step = k == kLeftOff ? -0x20 : 0x20;
        if (Often()) SetWord(Cur() + 4, static_cast<std::uint16_t>(b - step + static_cast<int>(Next() % 7) - 3));
        break;
    }
    default:
        break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[170];
    unsigned opened_at_5, stepped, cancelled, camped, camp_refused, chosen, clamped, grown;
} g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 170) ++g_cover.logged[out.log[i].what];
    switch (k) {
    case kOpen:
        if (Byte(out, at::kStep) == 5 && Byte(out, at::kMenu) == 5) ++g_cover.opened_at_5;
        else ++g_cover.stepped;
        break;
    case kInput: {
        bool reconciled = false, refused = false, chose = false;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
            if (out.log[i].what == 41) reconciled = true;
            if (out.log[i].what == 49 && out.log[i].a == 0x107) refused = true;
            if (out.log[i].what == 49 && out.log[i].a == 0x104) chose = true;
        }
        if (reconciled && Byte(out, at::kCampChosen) == 1 && Byte(in, at::kCampChosen) != 1) ++g_cover.camped;
        else if (reconciled) ++g_cover.cancelled;
        if (refused) ++g_cover.camp_refused;
        if (chose) ++g_cover.chosen;
        break;
    }
    case kIcons:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 58 && out.log[i].d > 0x10) { ++g_cover.grown; break; }
        break;
    case kUpOff:
    case kDown16:
    case kLeftOff:
    case kRight17:
    case kRightOff:
    case kDown38: {
        bool any = false;
        for (unsigned i = 0; i < 22; ++i)
            if (Byte(out, at::kRecords + i * at::kRecordSize + 3) == 0 && Byte(in, at::kRecords + i * at::kRecordSize + 3) != 0)
                any = true;
        if (any) ++g_cover.clamped;
        break;
    }
    default:
        break;
    }
}

using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("menu_lists: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        if (c.n_calls > 16) bof3::Fatal("menu_lists: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&FieldMenu_Run), reinterpret_cast<const void*>(&FieldMenu_Open),
        reinterpret_cast<const void*>(&FieldMenu_TopBar), reinterpret_cast<const void*>(&FieldMenu_TopBarInput),
        reinterpret_cast<const void*>(&FieldMenu_BackdropStep), reinterpret_cast<const void*>(&FieldMenu_PlaceWindows),
        reinterpret_cast<const void*>(&FieldMenu_ReconcileParty), reinterpret_cast<const void*>(&MenuList_Run),
        reinterpret_cast<const void*>(&MenuList_MemberPanel), reinterpret_cast<const void*>(&MenuList_DrawMemberPanel),
        reinterpret_cast<const void*>(&MenuList_MoneyBox), reinterpret_cast<const void*>(&MenuList_TimeBox),
        reinterpret_cast<const void*>(&MenuList_TopBarIcons), reinterpret_cast<const void*>(&MenuList_TitleBox),
        reinterpret_cast<const void*>(&MenuSlide_UpOff), reinterpret_cast<const void*>(&MenuSlide_DownTo16),
        reinterpret_cast<const void*>(&MenuSlide_LeftOff), reinterpret_cast<const void*>(&MenuSlide_RightTo17),
        reinterpret_cast<const void*>(&MenuSlide_RightOff), reinterpret_cast<const void*>(&MenuSlide_DownTo38)};

    static State saved, input, their_out, our_out;
    unsigned char* const saved_packet = Gfx_PacketNext;
    std::uint32_t saved_menu[kMenuEntries], saved_list[kListEntries];
    std::memcpy(saved_menu, At(kMenuBlock), sizeof saved_menu);
    std::memcpy(saved_list, At(kListBlock), sizeof saved_list);
    Gfx_PacketNext = g_prim;
    Capture(saved);
    g = kStubs;
    for (unsigned i = 0; i < kMenuEntries; ++i) SetPtr(kMenuBlock + 4 * i, reinterpret_cast<const void*>(kMenuStubs.h[i]));
    for (unsigned i = 0; i < kListEntries; ++i) SetPtr(kListBlock + 4 * i, reinterpret_cast<const void*>(kListStubs.h[i]));

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.prim) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.text) b = static_cast<unsigned char>(Next());
        input.packet = Next() % 0x100u;
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        g_seed = Next();
        const Args args = Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? ours[k] : clones[k];
            reinterpret_cast<Fn4>(const_cast<void*>(fn))(args.a[0], args.a[1], args.a[2], args.a[3]);
            Capture(out);
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      menu_lists self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    std::memcpy(At(kMenuBlock), saved_menu, sizeof saved_menu);
    std::memcpy(At(kListBlock), saved_list, sizeof saved_list);
    Apply(saved);
    Gfx_PacketNext = saved_packet;

    bof3::Log("shadow      menu_lists self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the menu's state block, the party lists, the window records, the character records, "
              "the tables, the primitive pool, the text, the packet cursor and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      menu_lists: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned states = 0, kinds = 0, list_states = 0;
    for (unsigned i = 100; i < 112; ++i) states += c.logged[i] ? 1u : 0u;
    for (unsigned i = 120; i < 141; ++i) kinds += c.logged[i] ? 1u : 0u;
    for (unsigned i = 141; i < 164; ++i) list_states += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      menu_lists coverage: menu states and steps %u of 12, kinds %u of 21, kind states %u of 23; "
              "opened on 5 %u, stepped %u; cancelled %u, camped %u, camp refused %u, entries chosen %u; recalcs %u, "
              "gateway %u, cell %u, sounds %u, panels %u, faces %u, money %u, time %u, icons %u (grown %u), titles %u, "
              "texts %u; slides clamped %u",
              states, kinds, list_states, c.opened_at_5, c.stepped, c.cancelled, c.camped, c.camp_refused, c.chosen,
              c.logged[44], c.logged[46], c.logged[47], c.logged[49], c.logged[51], c.logged[55], c.logged[56], c.logged[57],
              c.logged[58], c.grown, c.logged[59], c.logged[62], c.clamped);
    if (bad) bof3::Fatal("the field menu and the list draws differ from the original in %u self-test rounds", bad);
}

}  // namespace menu_lists
