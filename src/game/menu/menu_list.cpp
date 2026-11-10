// Moved here by tools/move_funcs.py from battle_e7.cpp, battle_result.cpp, battle_win_states.cpp, battle_windows.cpp, menu_draw_helpers.cpp, menu_lists.cpp, rest_2g.cpp, rest_2h.cpp, window_kinds.cpp (docs/source-layout-round.md).
#include "game/menu/menu_list.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dinput.h>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/battle_e7.h"
#include "game/battle_e7_callees.h"
#include "game/battle_result.h"
#include "game/battle_result_callees.h"
#include "game/battle_win_states.h"
#include "game/battle_win_states_callees.h"
#include "game/battle_windows.h"
#include "game/battle_windows_callees.h"
#include "game/boss_harness.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu/menu_list_callees.h"
#include "game/menu_draw_helpers.h"
#include "game/menu_draw_helpers_callees.h"
#include "game/menu_frame.h"
#include "game/menu_lists.h"
#include "game/menu_lists_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2g_callees.h"
#include "game/rest_2h.h"
#include "game/rest_2h_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/window_kinds_callees.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = menu_list::at;

}  // namespace

using namespace battle_result;

using namespace battle_win_states;

using namespace battle_windows;

namespace {

using namespace menu_draw_helpers;

}  // namespace

using namespace menu_lists;

using namespace window_kinds;

namespace {

using U = std::uint32_t;

using boss_harness::Phase;

// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;

using move_script::SetWord;

unsigned char& BBattleE7(U address) { return move_script::At(address)[0]; }

// The record 0x905B84 names, read afresh wherever the original reads it.
unsigned char* Win() { return move_script::At(BattleWinStates_Current); }

std::int32_t S16(unsigned v) { return static_cast<std::int16_t>(v); }

unsigned W(const unsigned char* p) { return move_script::Word(p); }

// `fild dword` then `fstp dword`: an int made a float (exact below 2^24).
void PutFloat(unsigned char* p, std::int32_t v) {
    const auto f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

const char* Str(U address) { return reinterpret_cast<const char*>(move_script::At(address)); }

char* Buf(U address) { return reinterpret_cast<char*>(move_script::At(address)); }

const unsigned char* Text(U address) { return move_script::At(address); }

// The window colour's first CLUT word (row s8 0x903A5A of the shadow
// 0x80B7A8), one five-bit field shifted into a byte. Read afresh each time,
// as the original reads it.
unsigned ColourField(unsigned shift) {
    const int row = Config_WindowColour;
    const unsigned w = move_script::Word(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x228 + static_cast<U>(row * 64)));
    return (((w >> shift) & 0x1F) << 3) & 0xFF;
}

// The draw mode a window part opens with: page (x, y) under abr.
void OpenPage(unsigned abr, int x, int y) {
    const unsigned tpage = BH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFF;
    BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
}

using StatGainFn = unsigned (__cdecl*)(unsigned, unsigned);

unsigned StatGain(unsigned member, unsigned stat) { return BH_AT(StatGainFn, battle_e7::at::kStatGain.v)(member, stat) & 0xFFFF; }

// A task slot BattleTask_Create answered, as an index into the pool: the
// original writes +9 / +0xA of slot 0xFF (none) past the 48 slots.
unsigned char* TaskSlot(const char* who, unsigned slot) {
    if (slot >= battle_e7::at::kTaskCount)
        bof3::Fatal("%s: BattleTask_Create answered slot %u, past the %u of the pool - the original writes its +9 / "
                    "+0xA past the pool (docs/battle_e7.md section 7)",
                    who, slot, battle_e7::at::kTaskCount);
    return move_script::At(::bof3::Img(BattleTask_Slots_addr) + slot * battle_e7::at::kTaskStride);
}

// call [esp + 4 * record +3] over a stack table of `n`: a Fatal past it (the
// original calls whatever the stack holds beyond the table).
void StackDispatch(const char* who, const U* table, unsigned n) {
    const unsigned s = rv::As<records::WindowRecord>(Win())->state;
    if (s >= n)
        bof3::Fatal("%s: the window record's +3 is %u, past the %u entries of its stack table - the original calls "
                    "through the stack beyond it (docs/battle_e7.md section 7)",
                    who, s, n);
    Phase(table[s])();
}

// A list window's open state (0x598FA0, 0x599390, 0x5994A0): nine tasks of
// `kind` 0 and parameter `param`, the slot's +9 the row 0..2 and +0xA the
// column 0..2; then the record's +0x14 = `rows`.
void OpenTasks(const char* who, unsigned param, unsigned rows) {
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j) {
            unsigned char* const t = TaskSlot(who, BH_CALL(BattleTask_Create)(0, param));
            t[9] = static_cast<unsigned char>(i);
            t[0xA] = static_cast<unsigned char>(j);
        }
    move_script::SetWord(Win() + 0x14, rows);
}

// A row's cost: the cost bytes of its three indices (0xFF none after the
// first), added as bytes.
unsigned RowCost(const unsigned char* row) {
    unsigned cost = BBattleE7(::bof3::Img(battle_e7::at::kCostTable.v) + row[0]);
    if (row[1] != 0xFF) cost = (cost + BBattleE7(::bof3::Img(battle_e7::at::kCostTable.v) + row[1])) & 0xFF;
    if (row[2] != 0xFF) cost = (cost + BBattleE7(::bof3::Img(battle_e7::at::kCostTable.v) + row[2])) & 0xFF;
    return cost;
}

// 7 when the menu actor's AP (+0x9A of the member its +5 names) is below the
// cost, else 0.
unsigned CostColour(unsigned cost) {
    auto* const member = static_cast<const records::PartyMember*>(BattleE4_MenuActor);
    const unsigned ap = W(move_script::At(::bof3::Img(ObjTrio_addr) + 0x9a + member->actor * battle_e7::at::kPartyStride));
    return ap < (cost & 0xFFFF) ? 7u : 0u;
}

}  // namespace

namespace battle_result {

using move_script::At;

}  // namespace battle_result

namespace {

unsigned char& Byte(std::uint32_t address) { return battle_result::At(address)[0]; }

unsigned char* MemberBattleResult(unsigned slot) { return battle_result::At(::bof3::Img(battle_result::at::kMembers.v) + (slot & 0xFF) * battle_result::at::kMemberSize); }

// The record the window task is running, re-read wherever the original reads it.
unsigned char* CurrentWindow() { return battle_result::At(BattleWinStates_Current); }

// MemberBattleResult() is +0x80 into the member record (0x802DC0); the record, and its
// typed view, starts 0x80 before.
char* TextBattleResult(std::uint32_t address) { return reinterpret_cast<char*>(battle_result::At(address)); }

const char* Format(std::uint32_t address) { return reinterpret_cast<const char*>(battle_result::At(address)); }

}  // namespace

namespace battle_win_states {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_win_states

// original 0x597230 (PSX 0x801E9470): state 1 - banner record k = +0xA of the
// pool 0x93B8E0 (12 bytes a record): its text (+4) counted by
// Text_GlyphCount; then, record re-read, a wide banner (+2 set) is the medium
// box at (x - 0x10, y) and the text at x + 0x26 - 6 n, 0x12 characters; a
// narrow one the small box at (x, y) and the text at x + 6 (6 - n), 8
// characters; y + 3, colour +0xA, through Text_DrawAt. The record is re-read
// after the count and after the box.
//
// DIVERGENCE DIV-0061: under a Latin language overlay the text is centred
// on the width the pen covers (DIV-0006's advances) about the same middle -
// x + 0x26 wide, x + 0x24 narrow - instead of by 6 units a glyph: the
// owner's "Green Apple" and "Flare" left of centre, 2026-09-27. 0 until
// BattleWinStates_Inject patches it to 1 under the name BattleBannerCentre,
// so BOF3X_ORIGINAL=BattleBannerCentre and the fuzz keep the original's x.
// The same flag lets the text run to half again the characters (amended for
// issue 127): the room for 8 glyphs of 12 units holds 12 letters of 8, and
// "Lucky Strike" and "Instant Kill", 12 each, drew as "Lucky St".
unsigned char g_banner_centre = 0;

namespace {

std::uint32_t Addr(const void* p) { return Low32(p); }

unsigned char* Current() { return battle_win_states::At(BattleWinStates_Current); }

const unsigned char* Target() { return battle_win_states::At(BattleActorCopies_Command); }

bool Choosing() { return BattleE4_Picking != 0; }

int S8(std::uint32_t address) { return static_cast<signed char>(battle_win_states::At(address)[0]); }

// Enemy slot t's record: 0x93B9E0 + 0x128 * (t - 3), the subtraction at full
// width (0x597320, 0x597400 - `sub eax, 3` on the zero-extended byte).
unsigned char* Enemy(unsigned slot) { return battle_win_states::At(::bof3::Img(Battle_Enemies_addr) + 0x80 + battle_win_states::at::kEnemyStride * (slot - 3u)); }

// A party member's battle object, ObjTrio 0x802D40 + 0x14C * m.
unsigned char* MemberBattleWinStates(unsigned m) { return battle_win_states::At(::bof3::Img(ObjTrio_addr) + battle_win_states::at::kMemberStride * m); }

using Handler = void (__cdecl*)();

void Run(std::uint32_t handler) { reinterpret_cast<Handler>(static_cast<std::uintptr_t>(::bof3::CodePlace(handler)))(); }

// A state or kind byte past its stack table: the original calls through the
// words above the table on its own stack - its return address first - which
// cannot be reproduced; ours aborts (CLAUDE.md rule 4, as
// Battle_PhaseDispatch and Field_RunTaskRecords do). docs/battle_win_states.md
// section 4.
[[noreturn]] void PastTable(const char* who, const char* byte, unsigned value, unsigned entries) {
    bof3::Fatal("%s: the current window's %s byte is %u; the original's stack table holds %u", who, byte, value,
                entries);
}

// The gauge's length: 55 * value / top as the original's `idiv` (both words
// zero-extended, so signed and unsigned agree), its low byte kept. A top of 0
// is the original's divide fault; ours aborts loudly instead.
unsigned char Gauge(unsigned value, unsigned top, const char* who) {
    if (top == 0) bof3::Fatal("%s: a gauge of %u over a top of 0 - the original's idiv faults here", who, value);
    return static_cast<unsigned char>((55u * value) / top);
}

// The low-HP flag every gauge state leaves after its step: bit 13 of the
// status word set (as `or byte [+1], 0x20`) while value < top / 4, cleared
// (as `and word, 0xDFFF`) otherwise.
void LowFlag(unsigned char* status, unsigned value, unsigned top) {
    if (value < (top >> 2))
        status[1] = static_cast<unsigned char>(status[1] | 0x20);
    else
        battle_win_states::SetWord(status, battle_win_states::Word(status) & 0xDFFFu);
}

// The target banner's x offset: 0x64E2BC for side bytes 0 and 3, else 0x64E2BE.
int SideDx(unsigned side) { return (side == 0 || side == 3) ? S8(::bof3::Img(battle_win_states::at::kSideDx.v)) : BattleWinStates_OtherDx; }

// The text's x within the banner: the original's count arithmetic, or under
// DIV-0061 the same middle less half the real width.
unsigned BannerTextX(unsigned x, unsigned n, bool wide, const unsigned char* text) {
    if (!g_banner_centre) return wide ? x - 6 * n + 0x26 : x + 6 * (6 - n);
    return x + (wide ? 0x26u : 0x24u) - TextAdvance_Width(text) / 2;
}

// Characters Text_DrawAt may draw: 0x12 wide, 8 narrow; under DIV-0061 times
// 3 / 2, 12-unit glyphs counted as 8-unit letters (27, 12).
int BannerTextCount(bool wide) {
    const int glyphs = wide ? 0x12 : 8;
    return g_banner_centre ? glyphs * 3 / 2 : glyphs;
}

}  // namespace

namespace battle_windows {

using move_script::At;

}  // namespace battle_windows

namespace {

unsigned char* PtrBattleWindows(std::uint32_t at) { return battle_windows::At(static_cast<std::uint32_t>(move_script::Long(battle_windows::At(at)))); }

unsigned char* CurrentBattleWindows() { return PtrBattleWindows(::bof3::Img(battle_windows::at::kCurrent.v)); }

constexpr auto& AtMenuDrawHelpers = move_script::At;

constexpr auto& LongMenuDrawHelpers = move_script::Long;

unsigned char* RecMenuDrawHelpers() {
    return AtMenuDrawHelpers(*reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr))));
}

// `call / jmp [table + index * 4]`: the .data dword read at the call, the
// index unbounded as in the original (a byte, zero-extended).
void Dispatch(menu_draw_helpers::U table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<menu_draw_helpers::U>(LongMenuDrawHelpers(AtMenuDrawHelpers(table + index * 4)))))();
}

}  // namespace

namespace menu_lists {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

}  // namespace menu_lists

namespace {

using Step = void (__cdecl*)();

// Entry `index` of a code-pointer table in .data, read afresh and unchecked.
Step Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Step>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(menu_lists::Long(menu_lists::At(table + index * 4)))));
}

// The record the window layer is running, re-read from 0x905B84 wherever the
// original re-reads it. Volatile so that ours cannot cache it either: the
// fuzz's stand-ins repoint it between calls.
unsigned char* RecMenuLists() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(
        *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr)))));
}

short SMenuLists(const unsigned char* p) { return static_cast<short>(menu_lists::Word(p)); }

void SlideY(int step, short bound, bool down) {
    unsigned char* const moved = RecMenuLists();
    rv::As<records::WindowRecord>(moved)->y = rv::As<records::WindowRecord>(moved)->y + static_cast<unsigned>(step);
    unsigned char* const r = RecMenuLists();
    if (down ? SMenuLists(r + 6) > bound : SMenuLists(r + 6) < bound) {
        rv::As<records::WindowRecord>(r)->y = static_cast<std::uint16_t>(bound);
        RecMenuLists()[3] = 0;
    }
}

void SlideX(int step, short bound, bool right) {
    unsigned char* const moved = RecMenuLists();
    rv::As<records::WindowRecord>(moved)->x = rv::As<records::WindowRecord>(moved)->x + static_cast<unsigned>(step);
    unsigned char* const r = RecMenuLists();
    if (right ? SMenuLists(r + 4) > bound : SMenuLists(r + 4) < bound) {
        rv::As<records::WindowRecord>(r)->x = static_cast<std::uint16_t>(bound);
        RecMenuLists()[3] = 0;
    }
}

using move_script::At;

using move_script::Word;

U Key(const void* p) { return Low32(p); }

unsigned char BRest2g(U address) { return move_script::At(address)[0]; }

short SRest2g(const unsigned char* p) { return static_cast<short>(move_script::Word(p)); }

std::int32_t Sx(const unsigned char* p) { return static_cast<std::int32_t>(SRest2g(p)); }

// The record the window layer is running, re-read from 0x905B84 wherever the
// original re-reads it. Volatile so that ours cannot cache it either: the
// fuzz's stand-ins repoint it between calls.
unsigned char* RecRest2g() {
    return move_script::At(*reinterpret_cast<volatile U*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr))));
}

// `call [table + 4 * record +3]` over a .data table of `entries` handlers, read
// in place (the fuzz swaps the cells for recorders): a Fatal past them, where
// the original calls through the dword after (the next table's, and past the
// run of code pointers, data).
void State(const char* who, U table, unsigned entries) {
    const unsigned s = RecRest2g()[3];
    if (s >= entries)
        bof3::Fatal("%s: the window record's +3 is %u, past the %u states of 0x%X - the original calls through the dword "
                    "after (docs/rest_2g.md section 7)",
                    who, s, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(move_script::Long(move_script::At(table + 4 * s)))))();
}

// `call [esp + 4 * byte]` over a stack table of `n` immediates: a Fatal past
// them, where the original calls whatever the stack holds beyond the table
// (its own saved registers and return address).
void StackState(const char* who, const U* table, unsigned n, unsigned index, const char* byte) {
    if (index >= n)
        bof3::Fatal("%s: the window record's %s is %u, past the %u entries of its stack table - the original calls "
                    "through the stack beyond it (docs/rest_2g.md section 7)",
                    who, byte, index, n);
    scenario_harness::Phase(table[index])();
}

// The character record (CharacterRecords index) of the party slot `slot`:
// 0x66972C[0x904062[slot]], both bytes read in place.
unsigned PartyRecord(unsigned slot) { return BRest2g(::bof3::Img(rest_2g::at::kMemberRecord.v) + BRest2g(::bof3::Img(rest_2g::at::kParty.v) + slot)); }

// A slide: the record's word at `off` (+4 x, +6 y) moves by `step`, and when
// it is then past `bound` - signed, by `past` - it is held there and +3 goes
// to 0. As the originals have it: the word is added in place, the record
// re-read before the compare and again before +3.
enum class Past : std::uint8_t { kAbove, kAtOrAbove, kBelow, kAtOrBelow };

void Slide(unsigned off, int step, short bound, Past past) {
    unsigned char* const moved = RecRest2g();
    move_script::SetWord(moved + off, move_script::Word(moved + off) + static_cast<unsigned>(step));
    unsigned char* const r = RecRest2g();
    const short v = SRest2g(r + off);
    const bool hold = past == Past::kAbove ? v > bound : past == Past::kAtOrAbove ? v >= bound : past == Past::kBelow ? v < bound : v <= bound;
    if (!hold) return;
    move_script::SetWord(r + off, static_cast<std::uint16_t>(bound));
    RecRest2g()[3] = 0;
}

// A slide bound of DIV-0041's faster slide, as widescreen.cpp holds it (docs/operand-store.md):
// the imm32 of `mov ecx, imm32` (its low word is cx), or the imm16 of `cmp cx,
// imm16`.
short Imm32Bound(U operand) { return Widescreen_SlideLogic(Widescreen_SlideBound(operand)); }

short Imm16Bound(U operand) { return Widescreen_SlideLogic(Widescreen_SlideBound(operand)); }

// DIVERGENCE DIV-0041 (amended 2026-10-13, the faster slide): a slide-off
// state decides by Capcom's bound at every width (Imm32Bound), and in the wide
// view the box is drawn to the bound moved out by the columns
// (Widescreen_SlideHeld, docs/ultrawide-slides.md).
void SlideOff(int step, U operand, Past past) {
    const short bound = Imm32Bound(operand);
    const short x_read = SRest2g(RecRest2g() + 4);
    Slide(4, step, bound, past);
    Widescreen_SlideHeld(RecRest2g(), x_read, bound);
}

constexpr auto& AtRest2h = move_script::At;

constexpr auto& LongRest2h = move_script::Long;

constexpr auto& WordRest2h = move_script::Word;

unsigned char& BRest2h(U address) { return *AtRest2h(address); }

U LRest2h(U address) { return static_cast<U>(LongRest2h(AtRest2h(address))); }

int S8Rest2h(unsigned v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// The current window record, re-read at every use (the originals reload
// 0x905B84 after each call).
unsigned char* RecRest2h() { return AtRest2h(*reinterpret_cast<volatile U*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr)))); }

int XRest2h(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->x; }

int YRest2h(const unsigned char* w) { return rv::As<records::WindowRecord>(w)->y; }

unsigned char Style() { return static_cast<unsigned char>(Config_WindowColour); }

// The step: the current record's +3 through a step table, called through the
// word the table holds (the fuzz swaps the words for recorders); past the
// table's own run of handlers the original jumps through whatever follows,
// ours aborts.
void StepRest2h(const char* who, U table, unsigned entries) {
    const unsigned index = RecRest2h()[3];
    if (index >= entries)
        bof3::Fatal("%s: the window's step +3 is %u, past the %u entries of 0x%X - the original jumps through the "
                    "dword after (docs/rest_2h.md section 7)",
                    who, index, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(LRest2h(table + 4 * index)))();
}

// CharacterRecords' record n (0xA4 each), unchecked; a member id's record
// index, 0x66972C[id] read in place (the byte is unbounded, the table is 24).
unsigned char* Record(U n) { return AtRest2h(::bof3::Img(CharacterRecords_addr) + n * rest_2h::at::kRecordStride); }

U RecordOf(U id) { return BRest2h(::bof3::Img(rest_2h::at::kMemberMap.v) + (id & 0xFF)); }

char* PrintBuf() { return reinterpret_cast<char*>(AtRest2h(::bof3::Img(rest_2h::at::kPrintBuf.v))); }

const char* FormatRest2h(U address) { return reinterpret_cast<const char*>(AtRest2h(address)); }

const unsigned char* TextRest2h(U address) { return AtRest2h(address); }

const unsigned char* Message(unsigned id) { return SH_CALL(Msg_SystemPtr)(id & 0xFFFF); }

void Piece(int x, int y, unsigned id) { SH_CALL(Menu_DrawPiece)(x, y, id, 1); }

void Pieces(int x, int y, U list) { SH_CALL(Menu_DrawPieces)(x, y, TextRest2h(list), 1); }

}  // namespace

namespace window_kinds {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

namespace {

template <typename T> T As(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

// The record the window layer is running, re-read from 0x905B84 wherever the
// original re-reads it (after every call out). Volatile so that ours cannot
// cache it either: the fuzz's stand-ins repoint it between calls.
unsigned char* Rec() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(
        *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr)))));
}

short S(const unsigned char* at) { return static_cast<short>(Word(at)); }

// 55 * v / max, as `lea` x 3 and `idiv` compute it: 32-bit signed, truncated.
int Scale(int v, int max) { return v * 55 / max; }

}  // namespace

}  // namespace window_kinds

// window_kinds's callee seam, moved here from window_kinds.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace window_kinds {

const Callees kOriginals = {
    BattleWin_DrawMessageBox,
    Text_DrawAt,
    BattleWin_DrawMessage,
    BattleMsg_Advance,
    Window_FreeCurrent,
    {As<Handler>(::bof3::Code(bof3::addr::BattleWin_BannerSlideIn)), As<Handler>(::bof3::Code(kNop.v)), As<Handler>(::bof3::Code(bof3::addr::BattleWin_BannerSlideOut))},
    {As<Handler>(::bof3::Code(bof3::addr::BattleWin_MessageWait)), As<Handler>(::bof3::Code(bof3::addr::BattleWin_MessageSlideIn)),
     As<Handler>(::bof3::Code(bof3::addr::BattleWin_MessageShow)), As<Handler>(::bof3::Code(bof3::addr::BattleWin_MessageSlideOut))},
    {As<Handler>(::bof3::Code(kResultKinds[0])), As<Handler>(::bof3::Code(kResultKinds[1])), nullptr, nullptr, As<Handler>(::bof3::Code(kResultKinds[4])),
     As<Handler>(::bof3::Code(kResultKinds[5]))},
};
Callees g = kOriginals;

}  // namespace window_kinds

// battle_win_states's callee seam, moved here from battle_win_states.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace battle_win_states {

const Callees kOriginals = {
    BattleWin_DrawPartyStatus,
    BattleWin_DrawCommandCross,
    BattleWin_DrawCommandLabel,
    BattleWin_DrawSmallBox,
    BattleWin_DrawMediumBox,
    BattleWin_DrawTargetEnemy,
    BattleWin_DrawEnemyStatus,
    BattleWin_DrawTargetMember,
    Window_FlagAndAdvance,
    Window_RestoreAndBack,
    Window_DispatchKind,
    Text_GlyphCount,
    Text_DrawAt,
    Menu_DrawIcon,
    Battle_ActorIsOut,
    Battle_ReturnTrue,
    {at::kKinds[0], at::kKinds[1], at::kKinds[2], at::kKinds[3], at::kKinds[4], at::kKinds[5], at::kKinds[6],
     at::kKinds[7]},
    {at::kPartyStates[0], at::kPartyStates[1], at::kPartyStates[2]},
    {at::kCrossStates[0], at::kCrossStates[1], at::kCrossStates[2]},
    {at::kLabelStates[0], at::kLabelStates[1], at::kLabelStates[2]},
    {at::kBannerStates[0], at::kBannerStates[1], at::kBannerStates[2]},
    {at::kEnemyStates[0], at::kEnemyStates[1], at::kEnemyStates[2], at::kEnemyStates[3]},
    {at::kMemberStates[0], at::kMemberStates[1], at::kMemberStates[2]},
};
Callees g = kOriginals;

}  // namespace battle_win_states

// menu_draw_helpers's callee seam, moved here from menu_draw_helpers.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace menu_draw_helpers {

const Callees kOriginals = {
    Menu_DrawTitleBox,  Msg_SystemPtr,       Text_DrawAt,        Menu_DrawButtonRow,       Menu_DrawMoneyBox,
    Menu_DrawCursorBox, Shop_DrawMemberStats, Menu_DrawEquipPanel, Shop_DrawBuyList,       Shop_DrawBuyDetail,
    Menu_DrawItemList,  Shop_DrawSellDetail, Menu_DrawHand,      BattleMenu_DrawItemList, BattleMenu_DrawSkillList,
    Low32(Widescreen_SlideOperand(at::kSlideLeftBound)),
};
Callees g = kOriginals;

}  // namespace menu_draw_helpers

// original 0x596FA0: record handler 3 of Field_RunTaskRecords - the battle
// window by its kind byte +2, from eight handlers on its stack: 0 the party
// row 0x597000, 1 the command cross 0x597090, 2 the command label 0x5971B0,
// 3 the action banner 0x597200, 4 an enemy's gauge 0x597320, 5 a member's
// gauges 0x5975D0, 6 and 7 group CM's 0x597C70 and 0x597D50.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_Run() {
    const unsigned kind = rv::As<records::WindowRecord>(Current())->kind;
    if (kind >= 8) PastTable("BattleWin_Run", "kind", kind, 8);
    Run(battle_win_states::g.kinds[kind]);
}

// original 0x597000: kind 0 (the party row) by its state +3: 0x597030,
// 0x597070, 0x437CC0 (idle).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_PartyRowStates() {
    const unsigned state = rv::As<records::WindowRecord>(Current())->state;
    if (state >= 3) PastTable("BattleWin_PartyRowStates", "state", state, 3);
    Run(battle_win_states::g.party[state]);
}

// original 0x597030: state 0 - the row slides up 4 a frame to y 0xC8 (a
// signed word compare); on arriving, the state steps on. Drawn every frame.
extern "C" void __cdecl BattleWin_PartyRowSlideIn() {
    records::WindowRecord* win = rv::As<records::WindowRecord>(Current());
    const auto y = static_cast<std::int16_t>(win->y);
    if (y <= 0xC8) {
        win->y = 0xC8;
        ++rv::As<records::WindowRecord>(Current())->state;
    } else {
        win->y = static_cast<unsigned>(y - 4);
    }
    win = rv::As<records::WindowRecord>(Current());
    battle_win_states::g.party_status(win->x, win->y);
}

// original 0x597070: state 1 - the row drawn where it is.
extern "C" void __cdecl BattleWin_PartyRowDraw() {
    auto* const win = rv::As<records::WindowRecord>(Current());
    battle_win_states::g.party_status(win->x, win->y);
}

// original 0x597090: kind 1 (the command cross): 0x5970C0, 0x597160, idle.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_CrossStates() {
    const unsigned state = rv::As<records::WindowRecord>(Current())->state;
    if (state >= 3) PastTable("BattleWin_CrossStates", "state", state, 3);
    Run(battle_win_states::g.cross[state]);
}

// original 0x5970C0: state 0 - the five arms' icons grow in. Arm i's growth
// g (0x904ABC + i, read before its icon) draws icon i through Menu_DrawIcon at
// the window's +4 / +6 plus the arm's point (0x64E2AC) plus g / 2, 0x10 - g
// square, shade 0x80; the record re-read per arm. Then, when arm 0's growth
// (re-read) is 0, the state steps on; else all seven growths lose 1.
extern "C" void __cdecl BattleWin_CrossGrow() {
    for (unsigned i = 0; i < 5; ++i) {
        const unsigned grow = battle_win_states::At(::bof3::Img(battle_win_states::at::kCrossGrow.v) + i)[0];
        const unsigned size = (0x10u - grow) & 0xFFu;
        const unsigned half = grow >> 1;
        const unsigned char* const arm = battle_win_states::At(::bof3::Img(battle_win_states::at::kCrossArms.v) + 2 * i);
        auto* const win = rv::As<records::WindowRecord>(Current());
        const unsigned y = win->y + arm[1] + half;
        const unsigned x = win->x + arm[0] + half;
        battle_win_states::g.icon(i, x, y, size, size, 0x80);
    }
    if (battle_win_states::At(::bof3::Img(battle_win_states::at::kCrossGrow.v))[0] == 0) {
        ++rv::As<records::WindowRecord>(Current())->state;
        return;
    }
    for (unsigned i = 0; i < 7; ++i) --battle_win_states::At(::bof3::Img(battle_win_states::at::kCrossGrow.v) + i)[0];
}

// original 0x597160: state 1 - the selected arm's growth (the selection
// 0x904AB4 re-read per arm) up by 2 while below 8, every other arm's down by
// 2 while not 0 (a growth of 1 wraps to 0xFF, as the original's); then the
// cross drawn at the window's +4 / +6.
extern "C" void __cdecl BattleWin_CrossFrame() {
    for (unsigned i = 0; i < 7; ++i) {
        const unsigned sel = battle_win_states::At(::bof3::Img(battle_win_states::at::kCrossSel.v))[0];
        unsigned char* const grow = battle_win_states::At(::bof3::Img(battle_win_states::at::kCrossGrow.v) + i);
        if (sel == i) {
            if (*grow < 8) *grow = static_cast<unsigned char>(*grow + 2);
        } else if (*grow != 0) {
            *grow = static_cast<unsigned char>(*grow - 2);
        }
    }
    auto* const win = rv::As<records::WindowRecord>(Current());
    battle_win_states::g.command_cross(win->x, win->y);
}

// original 0x5971B0: kind 2 (the command label): idle, 0x5971E0, idle.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_LabelStates() {
    const unsigned state = rv::As<records::WindowRecord>(Current())->state;
    if (state >= 3) PastTable("BattleWin_LabelStates", "state", state, 3);
    Run(battle_win_states::g.label[state]);
}

// original 0x5971E0: state 1 - the selected command's label banner, unless
// the byte 0x903A5D is set.
extern "C" void __cdecl BattleWin_LabelFrame() {
    if (BattleWinStates_LabelGate != 0) return;
    battle_win_states::g.command_label(battle_win_states::At(::bof3::Img(battle_win_states::at::kCrossSel.v))[0]);
}

// original 0x597200: kind 3 (the action banner): idle, 0x597230, idle.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_BannerStates() {
    const unsigned state = rv::As<records::WindowRecord>(Current())->state;
    if (state >= 3) PastTable("BattleWin_BannerStates", "state", state, 3);
    Run(battle_win_states::g.banner[state]);
}

extern "C" void __cdecl BattleWin_BannerFrame() {
    unsigned char* w = Current();
    records::WindowRecord* win = rv::As<records::WindowRecord>(w);
    const auto text = [](std::uint32_t off) {
        return battle_win_states::At(static_cast<std::uint32_t>(battle_win_states::Long(battle_win_states::At(::bof3::Img(battle_win_states::at::kBanners.v) + 4 + off))));
    };
    const unsigned n = battle_win_states::g.glyph_count(text(battle_win_states::at::kBannerStride * w[0xA]));
    w = Current();
    win = rv::As<records::WindowRecord>(w);
    const std::uint32_t off = battle_win_states::at::kBannerStride * w[0xA];
    const bool wide = battle_win_states::At(::bof3::Img(battle_win_states::at::kBanners.v) + 2 + off)[0] != 0;
    const unsigned x = win->x, y = win->y;
    if (wide) {
        battle_win_states::g.medium_box(static_cast<std::uint16_t>(x - 0x10), y);
    } else {
        battle_win_states::g.small_box(x, y);
    }
    w = Current();
    win = rv::As<records::WindowRecord>(w);
    const std::uint32_t o = battle_win_states::at::kBannerStride * w[0xA];
    // eax: the record offset with its low byte replaced by the colour
    const unsigned colour = (o & ~0xFFu) | battle_win_states::At(::bof3::Img(battle_win_states::at::kBanners.v) + 0xA + o)[0];
    const unsigned ty = static_cast<std::uint16_t>(win->y + 3);
    const unsigned tx = static_cast<std::uint16_t>(BannerTextX(win->x, n, wide, text(o)));
    battle_win_states::g.text_draw_at(static_cast<int>(tx), static_cast<int>(ty), static_cast<int>(colour), BannerTextCount(wide), text(o));
}

// original 0x597320: kind 4 (an enemy's gauge, slot +0xA = 3..8): its state
// (0x597400, 0x5974C0, 0x597510, idle), then every frame the gauge's step
// through Window_DispatchKind - (+0xB, the enemy's top +0x30, +0x1C, +0x14,
// its HP +0x24, +0xD, +0x18, +8: the kind byte +8 picks the handler); when
// Battle_ActorIsOut(slot) and +0xD is 0 the state becomes 3 (idle); last the
// enemy's low-HP flag. The record is re-read after each call.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_EnemyGaugeStates() {
    const unsigned state = rv::As<records::WindowRecord>(Current())->state;
    if (state >= 4) PastTable("BattleWin_EnemyGaugeStates", "state", state, 4);
    Run(battle_win_states::g.enemy[state]);
    unsigned char* w = Current();
    unsigned char* e = Enemy(w[0xA]);
    records::BattleEnemy* enemy_rec = rv::As<records::BattleEnemy>(e - 0x80);
    battle_win_states::g.dispatch_kind(Addr(w + 0xB), Addr(e + 0x30), Addr(w + 0x1C), Addr(w + 0x14), Addr(e + 0x24), Addr(w + 0xD),
                    Addr(w + 0x18), Addr(w + 8));
    w = Current();
    if (battle_win_states::g.actor_is_out(w[0xA]) != 0) {
        w = Current();
        if (w[0xD] == 0) w[3] = 3;
    }
    w = Current();
    e = Enemy(w[0xA]);
    enemy_rec = rv::As<records::BattleEnemy>(e - 0x80);
    LowFlag(e + 0x12, enemy_rec->hp, enemy_rec->max_hp);
}

// original 0x597400: state 0 - the gauge set up from the enemy's HP +0x24 and
// top +0x30: +0xB the length (55 HP / top), at least 1 while HP is not 0;
// +0x14 the HP, +0x1C the top, +0xD 0; the state steps on.
extern "C" void __cdecl BattleWin_EnemyGaugeOpen() {
    unsigned char* w = Current();
    auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy(w[0xA]) - 0x80);
    w[0xB] = Gauge(enemy_rec->hp, enemy_rec->max_hp, "BattleWin_EnemyGaugeOpen");
    w = Current();
    if (rv::As<records::BattleEnemy>(Enemy(w[0xA]) - 0x80)->hp != 0 && w[0xB] == 0) {
        w[0xB] = 1;
        w = Current();
    }
    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->hp_shown, rv::As<records::BattleEnemy>(Enemy(w[0xA]) - 0x80)->hp);
    w = Current();
    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->hp_top_shown, rv::As<records::BattleEnemy>(Enemy(w[0xA]) - 0x80)->max_hp);
    Current()[0xD] = 0;
    ++rv::As<records::WindowRecord>(Current())->state;
}

// original 0x5974C0: state 1 - the enemy's banner (BattleWin_DrawEnemyStatus
// at +4 / +6 for the slot); while a target is chosen (0x904AAF), a target byte
// of this slot steps the state on (Window_FlagAndAdvance), and one with 0x40
// (all enemies) steps it on too - both are tested, the target re-read.
extern "C" void __cdecl BattleWin_EnemyGaugeFrame() {
    const unsigned char* w = Current();
    auto* const win = rv::AsConst<records::WindowRecord>(w);
    battle_win_states::g.enemy_status(win->x, win->y, w[0xA]);
    if (!Choosing()) return;
    const unsigned char t = Target()[0];
    if (t == Current()[0xA]) battle_win_states::g.flag_advance();
    if (Target()[0] & 0x40) battle_win_states::g.flag_advance();
}

// original 0x597510: state 2 - the enemy's target banner. Its place: the
// enemy's s8 +0x72 / +0x73 plus the words at -0x52 / -0x50, x plus the side
// offset of the byte at -0x78 (0x64E2BC for 0 and 3, else 0x64E2BE), y plus
// 0xC; the slot taken as a byte, (+0xA - 3) & 0xFF. Drawn by
// BattleWin_DrawTargetEnemy; then, unless a target is being chosen and it is
// this slot (no side bits) or all enemies (0x40), Window_RestoreAndBack.
extern "C" void __cdecl BattleWin_EnemyTargetFrame() {
    unsigned char* w = Current();
    records::WindowRecord* win = rv::As<records::WindowRecord>(w);
    const std::uint32_t off = battle_win_states::at::kEnemyStride * ((w[0xA] - 3u) & 0xFFu);
    const std::uint32_t e = ::bof3::Img(Battle_Enemies_addr) + 0x80 + off;
    const int dx = SideDx(battle_win_states::At(e - 0x78)[0]);
    win->x = static_cast<unsigned>(S8(e + 0x72) + battle_win_states::Word(battle_win_states::At(e - 0x52)) + dx);
    const unsigned y = static_cast<unsigned>(S8(e + 0x73) + battle_win_states::Word(battle_win_states::At(e - 0x50)) + 0xC);
    rv::As<records::WindowRecord>(Current())->y = y;
    w = Current();
    win = rv::As<records::WindowRecord>(w);
    battle_win_states::g.target_enemy(win->x, win->y, w[0xA]);
    if (!Choosing()) {
        battle_win_states::g.restore_back();
        return;
    }
    const unsigned char t = Target()[0];
    if ((t & 0xC0) == 0) {
        if (t == Current()[0xA]) return;
    } else if (t & 0x40) {
        return;
    }
    battle_win_states::g.restore_back();
}

// original 0x5975D0: kind 5 (a member's gauges, slot +0xA = 0..2): its state
// (0x5976D0, 0x597850, 0x5978B0 - three, no idle entry), then the HP gauge's
// step (+0xB, top +0xA0, +0x1C, +0x14, HP +0x98, +0xD, +0x18, +8) and the AP
// gauge's (+0xC, +0xA2, +0x1E, +0x16, +0x9A, +0xE, +0x1A, +9) through
// Window_DispatchKind, then the member's low-HP flag (+0x90 bit 13). The
// record is re-read after each call.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_MemberGaugeStates() {
    const unsigned state = rv::As<records::WindowRecord>(Current())->state;
    if (state >= 3) PastTable("BattleWin_MemberGaugeStates", "state", state, 3);
    Run(battle_win_states::g.member[state]);
    unsigned char* w = Current();
    unsigned char* m = MemberBattleWinStates(w[0xA]);
    records::PartyMember* member_rec = rv::As<records::PartyMember>(m);
    battle_win_states::g.dispatch_kind(Addr(w + 0xB), Addr(m + 0xA0), Addr(w + 0x1C), Addr(w + 0x14), Addr(m + 0x98), Addr(w + 0xD),
                    Addr(w + 0x18), Addr(w + 8));
    w = Current();
    m = MemberBattleWinStates(w[0xA]);
    member_rec = rv::As<records::PartyMember>(m);
    battle_win_states::g.dispatch_kind(Addr(w + 0xC), Addr(m + 0xA2), Addr(w + 0x1E), Addr(w + 0x16), Addr(m + 0x9A), Addr(w + 0xE),
                    Addr(w + 0x1A), Addr(w + 9));
    w = Current();
    m = MemberBattleWinStates(w[0xA]);
    member_rec = rv::As<records::PartyMember>(m);
    LowFlag(m + 0x90, member_rec->hp, member_rec->max_hp);
}

// original 0x5976D0: state 0 - both gauges set up: HP (+0x98 over +0xA0)
// into +0xB (at least 1 while HP is not 0), +0x14, +0x1C, +0xD 0; AP (+0x9A
// over +0xA2) into +0xC - 0 when the top is 0, else at least 1 while AP is
// not 0 - +0x16, +0x1E, +0xE 0; the state steps on.
extern "C" void __cdecl BattleWin_MemberGaugeOpen() {
    unsigned char* w = Current();
    records::PartyMember* member_rec = rv::As<records::PartyMember>(MemberBattleWinStates(w[0xA]));
    w[0xB] = Gauge(member_rec->hp, member_rec->max_hp, "BattleWin_MemberGaugeOpen");
    w = Current();
    if (rv::As<records::PartyMember>(MemberBattleWinStates(w[0xA]))->hp != 0 && w[0xB] == 0) {
        w[0xB] = 1;
        w = Current();
    }
    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->hp_shown, rv::As<records::PartyMember>(MemberBattleWinStates(w[0xA]))->hp);
    w = Current();
    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->hp_top_shown, rv::As<records::PartyMember>(MemberBattleWinStates(w[0xA]))->max_hp);
    Current()[0xD] = 0;
    unsigned char* const v = Current();
    member_rec = rv::As<records::PartyMember>(MemberBattleWinStates(v[0xA]));
    const unsigned top = member_rec->max_ap;
    if (top == 0) {
        v[0xC] = 0;
        w = Current();
    } else {
        v[0xC] = static_cast<unsigned char>((55u * member_rec->ap) / top);
        w = Current();
        if (rv::As<records::PartyMember>(MemberBattleWinStates(w[0xA]))->ap != 0 && w[0xC] == 0) {
            w[0xC] = 1;
            w = Current();
        }
    }
    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->ap_shown, rv::As<records::PartyMember>(MemberBattleWinStates(w[0xA]))->ap);
    w = Current();
    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->ap_top_shown, rv::As<records::PartyMember>(MemberBattleWinStates(w[0xA]))->max_ap);
    Current()[0xE] = 0;
    ++rv::As<records::WindowRecord>(Current())->state;
}

// original 0x597850: state 1 - nothing drawn. While a target is chosen: when
// the member is out (Battle_ActorIsOut), only if Battle_ReturnTrue answers (a
// second try when the first says 0); then a target byte of this slot steps
// the state on, and one with 0x80 (all members) steps it on too.
extern "C" void __cdecl BattleWin_MemberGaugeWait() {
    if (!Choosing()) return;
    if (battle_win_states::g.actor_is_out(Current()[0xA]) != 0) {
        if (battle_win_states::g.return_true() == 0 && battle_win_states::g.return_true() == 0) return;
    }
    const unsigned char t = Target()[0];
    if (t == Current()[0xA]) battle_win_states::g.flag_advance();
    if (Target()[0] & 0x80) battle_win_states::g.flag_advance();
}

// original 0x5978B0: state 2 - the member's target banner. Its place: the
// member's words +0x2E / +0x30 plus the pair of BattleWin_MemberTargetOffsets
// 0x64DF70 at row +0x89 (the character), column +8, x plus the side offset of
// +8, y plus 0xC. Then, unless a target is being chosen and it is this slot
// (no side bits) or all members (0x80): +0xF 0 and the state back one, and
// nothing drawn; else BattleWin_DrawTargetMember. (Unlike the enemy's, the
// place is not put back.)
extern "C" void __cdecl BattleWin_MemberTargetFrame() {
    unsigned char* w = Current();
    records::WindowRecord* win = rv::As<records::WindowRecord>(w);
    const records::PartyMember* member_rec = rv::As<records::PartyMember>(MemberBattleWinStates(w[0xA]));
    const unsigned id = member_rec->char_id, side = member_rec->direction;
    const int dx = SideDx(side);
    const std::uint32_t pair = ::bof3::Img(BattleWin_MemberTargetOffsets_addr) + 2 * (side + 4 * id);
    win->x = static_cast<unsigned>(S8(pair) + static_cast<std::uint16_t>(member_rec->screen_x) + dx);
    w = Current();
    win = rv::As<records::WindowRecord>(w);
    member_rec = rv::As<records::PartyMember>(MemberBattleWinStates(w[0xA]));
    win->y = static_cast<unsigned>(static_cast<std::uint16_t>(member_rec->screen_y) + S8(pair + 1) + 0xC);
    if (!Choosing()) {
        Current()[0xF] = 0;
        --rv::As<records::WindowRecord>(Current())->state;
        return;
    }
    const unsigned char t = Target()[0];
    w = Current();
    win = rv::As<records::WindowRecord>(w);
    const bool here = (t & 0xC0) == 0 ? t == w[0xA] : (t & 0x80) != 0;
    if (!here) {
        w[0xF] = 0;
        --rv::As<records::WindowRecord>(Current())->state;
        return;
    }
    battle_win_states::g.target_member(win->x, win->y, w[0xA]);
}

// original 0x5979E0: the current window record's +0xF set and its state +3
// stepped on.
extern "C" void __cdecl Window_FlagAndAdvance() {
    CurrentBattleWindows()[0xF] = 1;
    ++rv::As<records::WindowRecord>(CurrentBattleWindows())->state;
}

// original 0x597A00: the current window record's position +4 / +6 put back
// from +0x10 / +0x12, +0xF cleared, its state +3 stepped back.
extern "C" void __cdecl Window_RestoreAndBack() {
    unsigned char* w = CurrentBattleWindows();
    records::WindowRecord* win = rv::As<records::WindowRecord>(w);
    win->x = rv::As<records::BattleWindow>(w)->home_x;
    w = CurrentBattleWindows();
    win = rv::As<records::WindowRecord>(w);
    win->y = rv::As<records::BattleWindow>(w)->home_y;
    CurrentBattleWindows()[0xF] = 0;
    --rv::As<records::WindowRecord>(CurrentBattleWindows())->state;
}

// original 0x597A30: a window kind's handler by the byte its eighth argument
// points at, from three on its stack (0x597A80, 0x597BD0, 0x597C10), handed
// all eight arguments; the answer is the handler's. Past 2 the original
// indexes its frame without a bound: 3 calls its own return address, 4..11
// its arguments as pointers, 12 and up the caller's frame - ours reads the
// same slots (`frame` is our EBP = entry ESP - 4; the table's dword i sits at
// entry ESP - 0xC + 4 i). docs/battle_windows.md, section 4.
extern "C" __attribute__((disable_tail_calls, noinline)) unsigned __cdecl Window_DispatchKind(
    std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5,
    std::uint32_t a6, std::uint32_t a7) {
    const auto* const frame = static_cast<const unsigned char*>(__builtin_frame_address(0));
    const unsigned i = battle_windows::At(a7)[0];
    std::uint32_t handler;
    if (i < 3) {
        handler = battle_windows::g.kind_handlers[i];
    } else {
        std::memcpy(&handler, frame - 8 + 4 * i, sizeof handler);
    }
    using Handler = unsigned (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t, std::uint32_t);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(::bof3::CodePlace(handler)))(a0, a1, a2, a3, a4, a5, a6, a7);
}

// original 0x597A80, gauge state 0 (settled). When the max HP changed since
// it was last seen: the HP clamped to it, the gauge recomputed as 55 * HP /
// max (at least 1 while the HP is non-zero) when the shown HP is the HP, else
// 55 * shown / max capped at 0x37 (the shown HP then catching up), and the
// max remembered. Then, if the HP fell below the shown HP, state 1: the gauge
// drops to its new length at once and the difference becomes the drained part
// (all of it when the HP is 0), stepped by a sixteenth of it; if it rose,
// state 2, stepped by a sixteenth of the pixel difference. Either way the
// step is at least 1 and the shown HP becomes the HP.
//
// As the original has it: every value is re-read from memory where the
// original re-reads it (the HP after each gauge store, the max for each
// divide); the rise's two divides share the one max read; the old gauge
// length passes through SharedScratch's first byte 0x903850, which is left
// holding it. No bound on the max: a max HP of 0 divides by zero in both.
extern "C" void __cdecl Window_HpGaugeTrack(unsigned char* gauge, const unsigned char* max_hp, unsigned char* max_seen,
                                            unsigned char* shown, unsigned char* hp, unsigned char* drain,
                                            unsigned char* step, unsigned char* kind) {
    const short max = S(max_hp);
    if (S(max_seen) != max) {
        if (!(max > S(hp))) window_kinds::SetWord(hp, static_cast<std::uint16_t>(max));
        const short was = S(shown);
        const short now = S(hp);
        if (was == now) {
            const auto q = static_cast<unsigned char>(Scale(now, S(max_hp)));
            gauge[0] = q;
            if (S(hp) != 0 && q == 0) gauge[0] = 1;
        } else {
            const auto q = static_cast<unsigned char>(Scale(was, S(max_hp)));
            gauge[0] = q;
            if (q > 0x37) {
                gauge[0] = 0x37;
                window_kinds::SetWord(shown, window_kinds::Word(hp));
            }
        }
        window_kinds::SetWord(max_seen, window_kinds::Word(max_hp));
    }
    const short was = S(shown);
    const short now = S(hp);
    if (was > now) {
        if (now == 0) {
            drain[0] = gauge[0];
            gauge[0] = 0;
        } else {
            window_kinds::at::kScratch = gauge[0];
            const auto q = static_cast<unsigned char>(Scale(S(hp), S(max_hp)));
            gauge[0] = q;
            if (S(hp) != 0 && q == 0) gauge[0] = 1;
            drain[0] = static_cast<unsigned char>(window_kinds::at::kScratch - gauge[0]);
        }
        window_kinds::SetWord(step, static_cast<unsigned>(drain[0] >> 4));
        kind[0] = 1;
    } else if (was < now) {
        const int max_now = S(max_hp);
        const int from = Scale(was, max_now);
        const int to = Scale(now, max_now);
        window_kinds::SetWord(step, static_cast<unsigned>((to - from) / 16));
        kind[0] = 2;
    } else {
        return;
    }
    if (window_kinds::Word(step) == 0) window_kinds::SetWord(step, 1);
    window_kinds::SetWord(shown, window_kinds::Word(hp));
}

// original 0x597BD0, gauge state 1 (draining): the drained part loses the
// step while it is longer than the step, else it and the state go to 0. The
// gauge is kept at 1 or more while the HP is non-zero.
//
// As the original has it: the drained byte is compared, zero-extended, with
// the step as a signed word; the subtraction takes the step's low byte.
extern "C" void __cdecl Window_HpGaugeDrain(unsigned char* gauge, const unsigned char* max_hp, unsigned char* max_seen,
                                            unsigned char* shown, unsigned char* hp, unsigned char* drain,
                                            unsigned char* step, unsigned char* kind) {
    (void)max_hp;
    (void)max_seen;
    (void)shown;
    const unsigned char d = drain[0];
    if (static_cast<int>(d) > static_cast<int>(S(step))) {
        drain[0] = static_cast<unsigned char>(d - step[0]);
    } else {
        drain[0] = 0;
        kind[0] = 0;
    }
    if (window_kinds::Word(hp) != 0 && gauge[0] == 0) gauge[0] = 1;
}

// original 0x597C10, gauge state 2 (filling): the gauge grows by the step
// while it stays more than a step short of 55 * HP / max, else it takes that
// length and the state goes to 0. At 1 or more while the HP is non-zero.
//
// As the original has it: the HP, the step and the max are read once, before
// any store; the growth adds the step's low byte to the gauge byte.
extern "C" void __cdecl Window_HpGaugeFill(unsigned char* gauge, const unsigned char* max_hp, unsigned char* max_seen,
                                           unsigned char* shown, unsigned char* hp, unsigned char* drain,
                                           unsigned char* step, unsigned char* kind) {
    (void)max_seen;
    (void)shown;
    (void)drain;
    const int target = Scale(S(hp), S(max_hp));
    const int by = S(step);
    const unsigned char c = gauge[0];
    if (target - by > static_cast<int>(c)) {
        gauge[0] = static_cast<unsigned char>(step[0] + c);
    } else {
        gauge[0] = static_cast<unsigned char>(target);
        kind[0] = 0;
    }
    if (window_kinds::Word(hp) != 0 && gauge[0] == 0) gauge[0] = 1;
}

// original 0x597C70: the banner window's state (record byte +3, three on its
// stack: 0x597CF0, the bare ret 0x437CC0, 0x597D10), then the message box at
// the record's +4 / +6 and the text of banner pool entry +0xA
// (0x93B8E0 + 0xC n: the text +4, drawn in its byte +0xA) through Text_DrawAt
// at (+4 + 4, +6 + 3), to its end.
//
// As the original has it: the record is re-read after the state and after the
// box; the colour argument is eax after `mov al, [...]` on 0xC n, so it
// carries 0xC n's bits above the low byte (n of 22 or more makes them
// non-zero) - ours passes the same dword. The coordinates' upper halves are
// the registers' leftovers, which the callees never read (they take words).
extern "C" void __cdecl BattleWin_BannerRun(void) {
    const unsigned state = Rec()[3];
    if (state >= 3) bof3::Fatal("a battle banner window's state byte is %u; the original's stack table holds three", state);
    window_kinds::g.banner[state]();
    const unsigned char* w = Rec();
    window_kinds::g.message_box(rv::As<records::WindowRecord>(w)->x, rv::As<records::WindowRecord>(w)->y);
    w = Rec();
    const std::uint32_t e = static_cast<std::uint32_t>(w[0xA]) * 0xC;
    const auto* const text = As<const unsigned char*>(static_cast<std::uint32_t>(window_kinds::Long(window_kinds::At(::bof3::Img(window_kinds::at::kBannerText.v) + e))));
    const std::uint32_t colour = (e & ~0xFFu) | window_kinds::At(::bof3::Img(window_kinds::at::kBannerColour.v) + e)[0];
    window_kinds::g.text_draw_at(static_cast<std::uint16_t>(rv::As<records::WindowRecord>(w)->x + 4), static_cast<std::uint16_t>(rv::As<records::WindowRecord>(w)->y + 3),
                   static_cast<int>(colour), 0xFF, text);
}

// original 0x597CF0, banner state 0: the window slides down 8 a frame until
// its y (+6) is 0x12, then the state steps on. A y that is not 0x12 modulo 8
// never arrives (the word wraps), as in the original.
extern "C" void __cdecl BattleWin_BannerSlideIn(void) {
    unsigned char* const w = Rec();
    const std::uint16_t y = rv::As<records::WindowRecord>(w)->y;
    if (y == 0x12) {
        ++w[3];
        return;
    }
    rv::As<records::WindowRecord>(w)->y = static_cast<std::uint16_t>(y + 8);
}

// original 0x597D10, banner state 2: the window slides up 8 a frame until its
// y is -0x16 (0xFFEA); there the kind bits of the banner entry 0x93B8C0 points
// at (+1) are cleared from the mask 0x904AE9 and the record is freed (a tail
// jump to Window_FreeCurrent).
//
// As the original has it: 0x93B8C0 is whichever entry BattleBanner_Dispatch
// visited last, not one this record names.
extern "C" void __cdecl BattleWin_BannerSlideOut(void) {
    unsigned char* const w = Rec();
    const std::uint16_t y = rv::As<records::WindowRecord>(w)->y;
    if (y == 0xFFEA) {
        const auto* const entry = As<const unsigned char*>(BattleMisc_BannerCurrent);
        const auto keep = static_cast<unsigned char>(~entry[1]);
        BattleMisc_BannerKinds = static_cast<unsigned char>(BattleMisc_BannerKinds & keep);
        window_kinds::g.free_current();
        return;
    }
    rv::As<records::WindowRecord>(w)->y = static_cast<std::uint16_t>(y - 8);
}

// original 0x597D50: the message window's four states by record byte +3
// (0x597D90, 0x597DC0, 0x597DF0, 0x597E60, on its stack).
extern "C" void __cdecl BattleWin_MessageRun(void) {
    const unsigned state = Rec()[3];
    if (state >= 4) bof3::Fatal("a battle message window's state byte is %u; the original's stack table holds four", state);
    window_kinds::g.message[state]();
}

// original 0x597D90, message state 0: nothing drawn; when the ring has an
// entry (write index 0x93C2A1 not the read index 0x93C2A0), the state steps
// on and 0x939F60 is set.
extern "C" void __cdecl BattleWin_MessageWait(void) {
    if (BattleActorCopies_QueueWrite == BattleWindows_MsgHead) return;
    unsigned char* const w = Rec();
    w[3] = static_cast<unsigned char>(w[3] + 1);
    BattleE1_MessageUp = 1;
}

// original 0x597DC0, message state 1: 0x939F60 set; the window slides down 8
// a frame until its y is 0x12, then the state steps on; then the message
// (a tail jump to BattleWin_DrawMessage).
extern "C" void __cdecl BattleWin_MessageSlideIn(void) {
    unsigned char* const w = Rec();
    BattleE1_MessageUp = 1;
    const std::uint16_t y = rv::As<records::WindowRecord>(w)->y;
    if (y == 0x12)
        ++w[3];
    else
        rv::As<records::WindowRecord>(w)->y = static_cast<std::uint16_t>(y + 8);
    window_kinds::g.draw_message();
}

// original 0x597DF0, message state 2: the message drawn; then, when the ring
// entry has flag bit 0 and a button went down this frame (Input_Pressed), the
// read index steps on and, when that empties the ring, the state becomes 3;
// then, for the entry now at the read index, when it has flag bit 1 and a
// timer (+1) other than 0xFF, the timer counts down, and at 0 the same step.
//
// As the original has it: the read index is re-read between the two tests,
// so a button that advances the ring makes the timer test look at the NEXT
// entry in the same frame; the record is re-read for each state store.
extern "C" void __cdecl BattleWin_MessageShow(void) {
    window_kinds::g.draw_message();
    unsigned i = BattleWindows_MsgHead;
    if ((window_kinds::At(::bof3::Img(window_kinds::at::kMsgRing.v) + 8 * i)[0] & 1) != 0 && Input_Pressed != 0) {
        if (window_kinds::g.msg_advance() != 0) Rec()[3] = 3;
    }
    i = BattleWindows_MsgHead;
    unsigned char* const entry = window_kinds::At(::bof3::Img(window_kinds::at::kMsgRing.v) + 8 * i);
    if ((entry[0] & 2) == 0) return;
    unsigned char timer = entry[1];
    if (timer == 0xFF) return;
    --timer;
    entry[1] = timer;
    if (timer != 0) return;
    if (window_kinds::g.msg_advance() != 0) Rec()[3] = 3;
}

// original 0x597E60, message state 3: the window slides up 8 a frame until
// its y is -0x16; there 0x939F60 is cleared and the state goes back to 0 (the
// record is kept, waiting for the next message). Every frame the box, and
// the ring entry BEFORE the read index (the one just shown) through
// Text_DrawAt at (+4 + 4, +6 + 3), colour 0, to its end.
extern "C" void __cdecl BattleWin_MessageSlideOut(void) {
    unsigned char* const w = Rec();
    const std::uint16_t y = rv::As<records::WindowRecord>(w)->y;
    if (y == 0xFFEA) {
        BattleE1_MessageUp = 0;
        w[3] = 0;
    } else {
        rv::As<records::WindowRecord>(w)->y = static_cast<std::uint16_t>(y - 8);
    }
    const unsigned char* r = Rec();
    window_kinds::g.message_box(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y);
    const unsigned i = (BattleWindows_MsgHead - 1u) & 0xFu;
    const auto* const text = As<const unsigned char*>(static_cast<std::uint32_t>(window_kinds::Long(window_kinds::At(::bof3::Img(window_kinds::at::kMsgRing.v) + 4 + 8 * i))));
    r = Rec();
    window_kinds::g.text_draw_at(static_cast<std::uint16_t>(rv::As<records::WindowRecord>(r)->x + 4), static_cast<std::uint16_t>(rv::As<records::WindowRecord>(r)->y + 3), 0, 0xFF,
                   text);
}

// original 0x597ED0: the battle message - the current window's box
// (BattleWin_DrawMessageBox at +4, +6), then the ring's entry 0x93C2A0
// through Text_DrawAt at (+4 + 4, +6 + 3), to its end. The record and the
// index are read after the box.
extern "C" void __cdecl BattleWin_DrawMessage() {
    const records::WindowRecord* win = rv::As<records::WindowRecord>(CurrentBattleWindows());
    battle_windows::g.message_box(win->x, win->y);
    const unsigned index = BattleWindows_MsgHead;
    const auto* const text = battle_windows::At(static_cast<std::uint32_t>(move_script::Long(battle_windows::At(::bof3::Img(battle_windows::at::kMsgRing.v) + 8 * index))));
    win = rv::As<records::WindowRecord>(CurrentBattleWindows());
    battle_windows::g.text_draw_at(static_cast<std::uint16_t>(win->x + 4), static_cast<std::uint16_t>(win->y + 3), 0, 0xFF,
                   text);
}

// original 0x597F20: the battle message ring's read index 0x93C2A0 stepped
// on (mod 16); 1 when it has reached the write index 0x93C2A1.
extern "C" unsigned char __cdecl BattleMsg_Advance() {
    const auto head = static_cast<unsigned char>((BattleWindows_MsgHead + 1) & 0xF);
    const unsigned char tail = BattleActorCopies_QueueWrite;
    BattleWindows_MsgHead = head;
    return tail == head ? 1 : 0;
}

// original 0x597F40: the characters of a string, a byte of 0x80 or more
// taking the next with it; a byte count.
extern "C" unsigned char __cdecl Text_GlyphCount(const unsigned char* text) {
    unsigned char n = 0;
    const unsigned char* p = text;
    unsigned char c = *p;
    while (c != 0) {
        if (c & 0x80) {
            if (TextPair_At(p)) ++n;      // DIV-0057: a pair is two characters wide
            ++p;
        }
        c = p[1];
        ++p;
        ++n;
    }
    return n;
}

// original 0x597F60: handler 4 of Field_RunTaskRecords' nine - one of six by
// record byte +2, on its stack: 0x597FA0, 0x5984B0, 0, 0, 0x598570,
// 0x5986C0. The two zeros are stored from eax, not immediates: kinds 2 and 3
// call address 0 in the original (an access violation); ours aborts loudly
// there, as for a kind past the table.
extern "C" void __cdecl Window_Handler4Kinds(void) {
    const unsigned kind = Rec()[2];
    if (kind >= 6) bof3::Fatal("a record-handler-4 window's kind byte is %u; the original's stack table holds six", kind);
    if (window_kinds::g.result[kind] == nullptr)
        bof3::Fatal("a record-handler-4 window has kind %u, whose slot in the original's stack table is 0", kind);
    window_kinds::g.result[kind]();
}

// original 0x597FA0: kind 0 of Window_Handler4Kinds - `call [esp + 4 +3]` over
// a stack table of one, BattleResultWin_DrawLevelUp. A Fatal at any other +3,
// where the original calls its own return address (state 1) or the caller's
// stack.
extern "C" void __cdecl BattleResultWin_LevelUpStates(void) {
    static const U kStates[1] = {bof3::addr::BattleResultWin_DrawLevelUp};
    StackState("BattleResultWin_LevelUpStates", kStates, 1, RecRest2g()[3], "+3");
}

// original 0x597FC0: the level-up window (0x597FA0's one state, kind 0 of
// Window_Handler4Kinds): the frame (0x14, 0x28, 0x118, record +9); for each
// stat 1..6 whose gain 0x432170(member +0xA, stat) is not 0, the gain again
// into its own buffer 0x904D20 + 0x20 (stat - 1) by Area08_MessageFormat and
// the system message 0xC, 0xD, 8, 9, 0xA, 0xB drawn at (0x1E, y), y from 0x2C
// by 0x10; then the two abilities byte +6 / +7 of the member's level row
// (Char_ExpTable, member +0xA, level +0xB) names: into 0x904DE0 / 0x904E00
// by 0x654900 and the system messages 0x12 / 0x13. The record re-read before
// each read, as the original.
extern "C" void __cdecl BattleResultWin_DrawLevelUp(void) {
    static constexpr unsigned char kLabels[6] = {0xC, 0xD, 8, 9, 0xA, 0xB};
    BH_CALL(BattleResultWin_DrawFrame)(0x14, 0x28, 0x118, Win()[9]);
    unsigned y = 0x2C;
    for (unsigned stat = 1; stat <= 6; ++stat) {
        if (StatGain(Win()[0xA], stat) == 0) continue;
        const unsigned gain = StatGain(Win()[0xA], stat);
        BH_CALL(Crt_sprintf)(Buf(::bof3::Img(battle_e7::at::kLevelBufs.v) + 0x20 * (stat - 1)), Str(::bof3::Img(battle_e7::at::kFmtNumber.v)), gain);
        const unsigned char* const label = BH_CALL(Msg_SystemPtr)(kLabels[stat - 1]);
        BH_CALL(Text_DrawAt)(0x1E, static_cast<int>(y), 0, 0xFF, label);
        y += 0x10;
    }
    const unsigned char* rec = Win();
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned row = rec[0xA] * 99u + rec[0xB];
        const unsigned ability = BBattleE7(::bof3::Img(battle_e7::at::kLevelTable.v) + 6 + k + row * 8);
        if (ability == 0) continue;
        BH_CALL(Crt_sprintf)(Buf(::bof3::Img(battle_e7::at::kLevelBufs.v) + 0xC0 + 0x20 * k), Str(::bof3::Img(battle_e7::at::kFmtAbility.v)),
                             Str(::bof3::Img(battle_e7::at::kAbilityRecords.v) + ability * 24));
        const unsigned char* const label = BH_CALL(Msg_SystemPtr)(0x12 + k);
        BH_CALL(Text_DrawAt)(0x1E, static_cast<int>(y), 0, 0xFF, label);
        rec = Win();
        y += 0x10;
    }
}

// original 0x5982D0: the result windows' frame at (x, y), w by h (each read
// as a word): a TILE in the window colour, semi-transparent, over page
// (0x3C0, 0); BattleWin_DrawQuadF4 shapes 8 and 9 at its top and bottom; two
// added lines (the top, the left) and two halved (the right, the bottom), in
// the window colour's three bytes. Callers: BattleResultWin_DrawLevelUp,
// BattleResultWin_DrawDrops, BattleResultWin_DrawExp.
//
// The colour arguments carry the stack slots they are built in (the red's
// upper bytes y's, the green's second byte h's, the blue's upper bytes h's);
// the lines read their colours as bytes.
extern "C" void __cdecl BattleResultWin_DrawFrame(U x, U y, U w, U h) {
    OpenPage(0, 0x3C0, 0);
    unsigned char* const p = Gfx_PacketNext;
    BH_CALL(Gpu_SetTile)(p);
    PutFloat(p + 8, S16(x));
    PutFloat(p + 0xC, S16(y) + 2);
    PutFloat(p + 0x14, static_cast<std::int32_t>(w & 0xFFFF));
    PutFloat(p + 0x18, static_cast<std::int32_t>(h & 0xFFFF));
    p[4] = static_cast<unsigned char>(ColourField(0));
    p[5] = static_cast<unsigned char>(ColourField(5));
    p[6] = static_cast<unsigned char>(ColourField(10));
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
    const unsigned r = ColourField(0), g = ColourField(5), b = ColourField(10);
    const U red = (y & 0xFFFFFF00u) | r;
    const U green = (h & 0xFF00u) | g;
    const U blue = (h & 0xFFFFFF00u) | b;
    BH_CALL(BattleWin_DrawQuadF4)(static_cast<int>(x), static_cast<int>(y), 8, 1);
    BH_CALL(BattleWin_DrawQuadF4)(static_cast<int>(x), static_cast<int>(y + h + 2), 9, 1);
    const int x2 = static_cast<int>(x + 2), xr = static_cast<int>(x + w - 3);
    const int y2 = static_cast<int>(y + 2), yb = static_cast<int>(y + h + 1);
    BH_CALL(BattleWin_DrawLineAdd)(x2, y2, xr, y2, static_cast<int>(red), static_cast<int>(green), static_cast<int>(blue));
    BH_CALL(BattleWin_DrawLineAdd)(x2, static_cast<int>(y + 3), x2, yb, static_cast<int>(red), static_cast<int>(green),
                                   static_cast<int>(blue));
    BH_CALL(BattleWin_DrawLineHalf)(xr, y2, xr, yb, static_cast<int>(red), static_cast<int>(green), static_cast<int>(blue));
    BH_CALL(BattleWin_DrawLineHalf)(x2, yb, xr, yb, static_cast<int>(red), static_cast<int>(green), static_cast<int>(blue));
}

// original 0x5984B0: the drops window (kind 1 of Window_Handler4Kinds): the
// frame at the record's (+4, +6), 0x118 wide, +9 high; then for each of the
// 0x904AE7 drops (the count re-read each time) whose word 0x904AF4[i]
// (category << 8 | item) is not 0, its line at (+4 + 5 + 138 (i & 1), +6 + 4
// + 13 (i >> 1)) - each sum a word, the +5 / +4 after - with the count
// 0x904B14[i], colour 0, not dimmed.
extern "C" void __cdecl BattleResultWin_DrawDrops(void) {
    const unsigned char* const rec = Win();
    auto* const win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(BattleResultWin_DrawFrame)(static_cast<unsigned>(win->x), static_cast<unsigned>(win->y), 0x118, rec[9]);
    for (unsigned i = 0; (i & 0xFF) < Battle_DropCount; i = (i + 1) & 0xFF) {
        const unsigned word = W(move_script::At(::bof3::Img(battle_e7::at::kDropItems.v) + 2 * i));
        if (word == 0) continue;
        const unsigned count = BBattleE7(::bof3::Img(battle_e7::at::kDropCounts.v) + i);
        auto* const win2 = rv::As<records::WindowRecord>(Win());
        const U y = ((13u * (i >> 1) + static_cast<unsigned>(win2->y)) & 0xFFFF) + 4;
        const U x = ((138u * (i & 1) + static_cast<unsigned>(win2->x)) & 0xFFFF) + 5;
        BH_CALL(BattleResultWin_DrawItem)(x, y, 0, word >> 8, word & 0xFF, count, 0);
    }
}

// original 0x598570 (PSX 0x801F065C, paired): state 4 of window kind 4 (the
// EXP): entry (byte +3 of the record 0x905B84) of a two-entry table the
// original builds on its own stack - 0x5986F0 (the step one on), 0x5985A0
// (the EXP list). The index is not checked: 2..255 would call through the
// words above the table on the original's stack; ours aborts (CLAUDE.md
// rule 4, as battle_flow's Battle_PhaseDispatch).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_ExpState(void) {
    const unsigned index = rv::As<records::WindowRecord>(CurrentWindow())->state;
    if (index >= 2) bof3::Fatal("BattleResultWin_ExpState: step %u, past the two-entry table", index);
    battle_result::g.exp_window[index]();
}

// original 0x5985A0 (PSX 0x801F06D4, paired): the EXP window's frame -
// 0x5982D0(0x14, 0x28, 0x118, the record's height +9) - and one line per
// party slot, y = 0x2C + 16 n (a byte): the name (Text_DrawAt at 0x19, 5
// characters); at level 99 (byte +0xA of the slot's record is 0x63)
// Msg_SystemPtr(0x41) at 0x55; otherwise the next level ("%2d" of level +
// 1, Text_DrawFont12 at 0x85), the EXP still to go ("%6d" of 0x598810(n),
// at 0xE3) and Msg_SystemPtr(0x15) at 0x55. Text record 2 is the scratch.
//
// As the original has it: the record 0x905B84 is read once, before the
// frame; the count 0x904AB0 after the frame and after each line; the level
// after the name. The original's frame height and y carry the upper half of
// a register the caller left (ecx, esi) - 0x5982D0, Text_DrawAt and
// Text_DrawFont12 read those arguments as words, and ours passes them
// zero-extended; 0x598810 reads its slot as a byte.
//
// DIVERGENCE DIV-0060: under a Latin language overlay the line takes the US
// release's layout - the EXP still to go right-aligned before the sentence,
// the sentence, the next level at the right edge: "%6d" at 0x36, the
// message at 0x82, "%2d" at 0x114 (read off the owner's photograph of the
// US screen, 2026-09-27; the level-99 message at 0x82 too). The Chinese
// layout puts the sentence at 0x55 and the level at 0x85, which the US
// sentence "EXP to next level:" (144 units) runs over - the owner's "a
// digit hidden in 'next'". Same calls, same order, other x. 0 until
// BattleResult_Inject patches it to 1 under the name BattleResultExpLayout.
unsigned char g_exp_layout_us = 0;

extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_DrawExp(void) {
    const unsigned char* const w = CurrentWindow();
    battle_result::g.draw_frame(0x14, 0x28, 0x118, w[9]);
    const bool us = g_exp_layout_us != 0;
    const int x_msg = us ? 0x82 : 0x55, x_level = us ? 0x114 : 0x85, x_exp = us ? 0x36 : 0xE3;
    unsigned char y = 0x2C;
    for (unsigned char slot = 0; slot < Byte(::bof3::Img(battle_result::at::kPartyCount.v)); ++slot, y = static_cast<unsigned char>(y + 0x10)) {
        unsigned char* const m = MemberBattleResult(slot);
        auto* const member_rec = rv::As<records::PartyMember>(m - 0x80);
        battle_result::g.text_draw_at(0x19, y, 0, 5, m);
        const unsigned level = member_rec->level;
        if (level == 0x63) {
            battle_result::g.text_draw_at(x_msg, y, 0, 0xFF, battle_result::g.msg_system(0x41));
            continue;
        }
        battle_result::g.crt_sprintf(TextBattleResult(::bof3::Img(battle_result::at::kText2.v)), Format(::bof3::Img(Boss26Fx_CountFormat_addr)), level + 1);
        battle_result::g.text_draw_font12(x_level, y, 0, battle_result::At(::bof3::Img(battle_result::at::kText2.v)));
        battle_result::g.crt_sprintf(TextBattleResult(::bof3::Img(battle_result::at::kText2.v)), Format(::bof3::Img(battle_result::at::kFormat6d.v)), battle_result::g.exp_to_next(slot));
        battle_result::g.text_draw_font12(x_exp, y, 0, battle_result::At(::bof3::Img(battle_result::at::kText2.v)));
        battle_result::g.text_draw_at(x_msg, y, 0, 0xFF, battle_result::g.msg_system(0x15));
    }
}

// original 0x5986C0 (no PSX twin paired): state 5 of window kind 4 (the
// zenny): the same two-entry stack table - 0x5986F0, 0x598700 (the zenny
// line). Ours aborts past it, as BattleResultWin_ExpState.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_ZennyState(void) {
    const unsigned index = rv::As<records::WindowRecord>(CurrentWindow())->state;
    if (index >= 2) bof3::Fatal("BattleResultWin_ZennyState: step %u, past the two-entry table", index);
    battle_result::g.zenny_window[index]();
}

// original 0x5986F0 (no PSX twin paired): the record 0x905B84's step +3 one
// on - both states' entry 0, the frame the window opens.
extern "C" void __cdecl BattleResultWin_NextStep(void) {
    auto* const window_rec = rv::As<records::WindowRecord>(CurrentWindow());
    window_rec->state = static_cast<unsigned char>(window_rec->state + 1);
}

// original 0x598700 (no PSX twin paired): the zenny window - the medium box
// at (0x6E, 0x28), the party's zenny 0x904058 in "%7d" at (0x70, 0x2A) and
// the text 0x66A31C after it at 0xC4.
//
// As the original has it: the zenny is read after the box.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleResultWin_DrawZenny(void) {
    battle_result::g.draw_medium_box(0x6E, 0x28);
    battle_result::g.crt_sprintf(TextBattleResult(::bof3::Img(battle_result::at::kText2.v)), Format(::bof3::Img(battle_result::at::kFormat7d.v)), Party_Zenny);
    battle_result::g.text_draw_font12(0x70, 0x2A, 0, battle_result::At(::bof3::Img(battle_result::at::kText2.v)));
    battle_result::g.text_draw_at(0xC4, 0x2A, 0, 0xFF, battle_result::At(::bof3::Img(battle_result::at::kZennyUnit.v)));
}

// original 0x598750: one item's line of the drops window at (x, y): nothing
// unless the item and the count are not 0; the icon (0x66AF24 by
// Item_IconKind(category, item)) at (x, y + 2), dimmed by `dim`; the name
// (Item_NamePtr) at (x + 10, y) in `colour`, as many characters as
// Text_CharCount counts; and for a count above 1, the count by 0x653EC0 into
// 0x904BA0 in the 8-pixel font at (x + 0x6D, y + 2).
extern "C" void __cdecl BattleResultWin_DrawItem(U x, U y, U colour, U category, U item, U count, U dim) {
    if ((item & 0xFF) == 0 || (count & 0xFF) == 0) return;
    const unsigned kind = BH_CALL(Item_IconKind)(category, item);
    BH_CALL(Menu_DrawIcon8)(static_cast<int>(x), static_cast<int>(y + 2), BBattleE7(::bof3::Img(battle_e7::at::kIconKinds.v) + (kind & 0xFF)),
                            static_cast<int>(dim));
    const unsigned char* const name = BH_CALL(Item_NamePtr)(category, item);
    const unsigned n = BH_CALL(Text_CharCount)(name);
    BH_CALL(Text_DrawAt)(static_cast<int>(x + 10), static_cast<int>(y), static_cast<int>(colour), static_cast<int>(n), name);
    if ((count & 0xFF) <= 1) return;
    BH_CALL(Crt_sprintf)(Buf(::bof3::Img(battle_e7::at::kPrintBuf.v)), Str(::bof3::Img(battle_e7::at::kFmtCount.v)), count & 0xFF);
    BH_CALL(Text_DrawFont8)(static_cast<int>(x + 0x6D), static_cast<int>(y + 2), static_cast<int>(colour), Text(::bof3::Img(battle_e7::at::kPrintBuf.v)));
}

// original 0x598810: the EXP party slot `slot` (a byte) still needs for its
// next level, or 0 when it has as much: the character id ObjTrio[slot] +0x89
// made a roster index r by 0x4469D0 (R3B's; the argument the id under (5 slot)'s
// upper bytes, as the original's ecx holds them); the sum of the first word of
// rows 0..level of Char_ExpTable's r (99 rows of 8 bytes), level the record
// r's +0xA; less the record's EXP dword +0xC. Called by BattleResultWin_DrawExp
// (battle_result.cpp).
//
// As the original has it: the roster index is the answer's low byte, stored
// over the argument's; level + 1 rows are summed (256 at level 255); the
// compare is unsigned, and the answer the whole eax.
extern "C" unsigned __cdecl BattleResultWin_ExpToNext(unsigned slot) {
    const U s = slot & 0xFF;
    const unsigned id = BRest2g(::bof3::Img(rest_2g::at::kObjTrio.v) + 0x89 + rest_2g::at::kObjStride * s);
    const U arg = ((s * 5) & 0xFFFFFF00u) | id;
    const unsigned r = SH_AT(unsigned (__cdecl*)(unsigned), rest_2g::kRosterIndex.v)(arg) & 0xFF;
    const unsigned char* const rec = move_script::At(::bof3::Img(CharacterRecords_addr) + rest_2g::at::kCharStride * r);
    const unsigned rows = rec[0xA] + 1u;
    U sum = 0;
    for (unsigned k = 0; k < rows; ++k) sum += move_script::Word(move_script::At(::bof3::Img(rest_2g::at::kExpTable.v) + 0x318 * r + 8 * k));
    const U exp = static_cast<U>(rv::As<records::CharRecord>(rec)->exp);
    return sum >= exp ? sum - exp : 0;
}

// original 0x598890: record handler 5 of Field_RunTaskRecords - `call [esp + 4
// kind]` on the record's +2 over a stack table of five: GeneWin_GridStates and
// BE7's GeneWin_ChoiceStates, _ListStates, _List2States, _List3States. A Fatal
// past five.
extern "C" void __cdecl Window_Handler5Kinds(void) {
    static const U kKinds[5] = {bof3::addr::GeneWin_GridStates, bof3::addr::GeneWin_ChoiceStates,
                                bof3::addr::GeneWin_ListStates, bof3::addr::GeneWin_List2States,
                                bof3::addr::GeneWin_List3States};
    StackState("Window_Handler5Kinds", kKinds, 5, RecRest2g()[2], "kind +2");
}

// original 0x5988D0: kind 0 of handler 5, the gene grid: `call [esp + 4 +3]`
// over a stack table of three - GeneWin_GridOpen, _GridSlideIn, _GridSlideOut.
extern "C" void __cdecl GeneWin_GridStates(void) {
    static const U kStates[3] = {bof3::addr::GeneWin_GridOpen, bof3::addr::GeneWin_GridSlideIn,
                                 bof3::addr::GeneWin_GridSlideOut};
    StackState("GeneWin_GridStates", kStates, 3, RecRest2g()[3], "+3");
}

// original 0x598900: the grid's open state: the record's +8 = 0x18 columns and
// +9 = 0x11 rows (the record re-read for each); the low byte of each of the
// eighteen words 0x939820.. zeroed; for each of the eighteen cells 0x939A60.. that is not 0, a battle
// task BattleTask_Create(0, 9) whose +0xB and +0x4B take the cell's index;
// then one BattleTask_Create(0, 0xA); +3 + 1 (the record re-read).
//
// As the original has it: each cell byte is read after the call before it; a
// slot the call answers past the pool's 48 (0xFF when none is free) is a Fatal
// here, where the original writes past the pool (docs/rest_2g.md section 7).
extern "C" void __cdecl GeneWin_GridOpen(void) {
    RecRest2g()[8] = 0x18;
    RecRest2g()[9] = 0x11;
    for (unsigned i = 0; i < 0x12; ++i) move_script::At(::bof3::Img(rest_2g::at::kGridWords.v) + 2 * i)[0] = 0;
    for (unsigned i = 0; i < 0x12; ++i) {
        if (BRest2g(::bof3::Img(rest_2g::at::kGridCells.v) + i) == 0) continue;
        const unsigned slot = SH_CALL(BattleTask_Create)(0, 9) & 0xFFu;
        if (slot >= rest_2g::at::kTaskCount)
            bof3::Fatal("GeneWin_GridOpen: BattleTask_Create answered slot %u, past the %u of the pool - the original "
                        "writes its +0xB / +0x4B past the pool (docs/rest_2g.md section 7)",
                        slot, rest_2g::at::kTaskCount);
        unsigned char* const task = move_script::At(::bof3::Img(BattleTask_Slots_addr) + rest_2g::at::kTaskStride * slot);
        task[0x4B] = static_cast<unsigned char>(i);
        task[0xB] = static_cast<unsigned char>(i);
    }
    SH_CALL(BattleTask_Create)(0, 0xA);
    unsigned char* const r = RecRest2g();
    r[3] = static_cast<unsigned char>(r[3] + 1);
}

// original 0x5989B0: the grid slides in, after its draw (GeneWin_DrawGrid; the
// record read once after it): from the right (+0xA 0) x moves left by 0x20
// while above 0x42, else is set to 0x42; from the left (+0xA not 0) right by
// 0x20 while below 0x42, else 0x42. As the original has it: a step can pass
// 0x42, which the next frame sets back; +3 is never changed here.
extern "C" void __cdecl GeneWin_GridSlideIn(void) {
    SH_CALL(GeneWin_DrawGrid)();
    unsigned char* const r = RecRest2g();
    const short x = SRest2g(r + 4);
    if (r[0xA] == 0) rv::As<records::WindowRecord>(r)->x = x > 0x42 ? static_cast<unsigned>(x - 0x20) : 0x42u;
    else rv::As<records::WindowRecord>(r)->x = x < 0x42 ? static_cast<unsigned>(x + 0x20) : 0x42u;
}

// original 0x5989F0: the grid slides out, after its draw: to the right (+0xA
// 0) by 0x20 until x reaches 0x142, to the left by 0x20 until x is -0xBE or
// less; there the window is freed instead (a tail jump to Window_FreeCurrent).
// Both bounds are operands of DIV-0041's faster slide (`cmp cx, imm16` at 0x598A05 /
// 0x598A19), read there. DIVERGENCE DIV-0041 (amended 2026-10-13, the faster
// slide): decided by Capcom's bounds at every width; in the wide view the
// draw sees the x on the line to the bound moved out by the columns, and the
// free test Capcom's x again (Widescreen_SlideDraw / _SlideUndraw,
// docs/ultrawide-slides.md).
extern "C" void __cdecl GeneWin_GridSlideOut(void) {
    {
        unsigned char* const w = RecRest2g();
        const bool right = w[0xA] == 0;
        Widescreen_SlideDraw(w, Imm16Bound(right ? rest_2g::at::kGridOutRight.v : rest_2g::at::kGridOutLeft.v), right ? 0x20 : -0x20);
    }
    SH_CALL(GeneWin_DrawGrid)();
    Widescreen_SlideUndraw(RecRest2g());
    unsigned char* const r = RecRest2g();
    const short x = SRest2g(r + 4);
    if (r[0xA] == 0) {
        if (x >= Imm16Bound(rest_2g::at::kGridOutRight.v)) {
            SH_CALL(Window_FreeCurrent)();
            return;
        }
        rv::As<records::WindowRecord>(r)->x = static_cast<unsigned>(x + 0x20);
        return;
    }
    if (x <= Imm16Bound(rest_2g::at::kGridOutLeft.v)) {
        SH_CALL(Window_FreeCurrent)();
        return;
    }
    rv::As<records::WindowRecord>(r)->x = static_cast<unsigned>(x - 0x20);
}

// original 0x598A30 (PSX 0x800AD3C0): the gene grid window (kind 0's two slide states call it
// before they move the record): its frame at (+4, +6), +8 by +9 cells; three
// boxes in the window colour - (+4 + 6, +6 + 0x21) sized (+8 * 8 - 0xF, +9 *
// 8 - 0x27), (+4 + 0x36, +6 + 6) 0x48 by 0x18, (+4 + 0x87, +6 + 0xA) 0x30 by
// 0x10; the label 0x66AF38 and the cost 0x904B78 (by 0x64D3EC into 0x904BA0)
// in the 12-pixel font at (+4 + 0x88 / + 0xA0, +6 + 0xC), in colour 7 when
// the menu actor's AP (+0x9A) is below the cost, else 0; and in step 4
// (0x904AA3), the hand at the first box (0x904AA6 = 0xFF) or at cell
// (0x904AA7, 0x904AA6): (+4 + 0xE + 30 column, +6 + 0x2C + 32 row). Every
// coordinate a word; the record re-read before each draw, as the original.
extern "C" void __cdecl GeneWin_DrawGrid(void) {
    const unsigned char* rec = Win();
    const records::WindowRecord* win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(GeneWin_DrawFrame)(static_cast<unsigned>(win->x), static_cast<unsigned>(win->y), rec[8], rec[9]);
    unsigned colour = static_cast<unsigned char>(Config_WindowColour);
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(Menu_DrawBox)(static_cast<int>(static_cast<unsigned>(win->x) + 6), static_cast<int>(static_cast<unsigned>(win->y) + 0x21),
                          static_cast<int>(rec[8] * 8u - 0xF), static_cast<int>(rec[9] * 8u - 0x27), 0,
                          static_cast<int>(colour));
    colour = static_cast<unsigned char>(Config_WindowColour);
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(Menu_DrawBox)(static_cast<int>(static_cast<unsigned>(win->x) + 0x36), static_cast<int>(static_cast<unsigned>(win->y) + 6), 0x48, 0x18, 0,
                          static_cast<int>(colour));
    colour = static_cast<unsigned char>(Config_WindowColour);
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(Menu_DrawBox)(static_cast<int>(static_cast<unsigned>(win->x) + 0x87), static_cast<int>(static_cast<unsigned>(win->y) + 0xA), 0x30, 0x10, 0,
                          static_cast<int>(colour));
    auto* const member = static_cast<const records::PartyMember*>(BattleE4_MenuActor);
    const unsigned text_colour = static_cast<unsigned>(member->ap) < BattleActions_ApCost ? 7u : 0u;
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(Text_DrawFont12)(static_cast<int>(static_cast<unsigned>(win->x) + 0x88), static_cast<int>(static_cast<unsigned>(win->y) + 0xC),
                             static_cast<int>(text_colour), Text(::bof3::Img(battle_e7::at::kApLabel.v)));
    BH_CALL(Crt_sprintf)(Buf(::bof3::Img(battle_e7::at::kPrintBuf.v)), Str(::bof3::Img(Boss26Fx_CountFormat_addr)), BattleActions_ApCost);
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(Text_DrawFont12)(static_cast<int>(static_cast<unsigned>(win->x) + 0xA0), static_cast<int>(static_cast<unsigned>(win->y) + 0xC),
                             static_cast<int>(text_colour), Text(::bof3::Img(battle_e7::at::kPrintBuf.v)));
    if (BattleE4_Step3 != 4) return;
    const unsigned row = BattleE5_GeneRow;
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    if (row == 0xFF) {
        BH_CALL(Menu_DrawHand)(static_cast<int>(static_cast<unsigned>(win->x) + 0xE), static_cast<int>(static_cast<unsigned>(win->y) + 0xC), 0);
        return;
    }
    const U hy = (row << 5) + static_cast<unsigned>(win->y) + 0x2C;
    const U hx = BattleE1_Roster * 30u + static_cast<unsigned>(win->x) + 0xE;
    BH_CALL(Menu_DrawHand)(static_cast<int>(hx), static_cast<int>(hy), 0);
}

// original 0x598BE0 (PSX 0x800AD61C): a frame of 8-pixel pieces at (x, y), `cols` by `rows`
// cells (bytes), over page (0x340, 0x100): piece 0x25 inside (cells 1..cols-2
// by 1..rows-2), 0x24 / 0x28 along the top and bottom (cells 2..cols-3),
// 0x2A / 0x2B down the left and right (rows 2..rows-3), and the corners
// 0x23, 0x26, 0x27, 0x29 at 16 pixels in from the far edges.
extern "C" void __cdecl GeneWin_DrawFrame(U x, U y, U cols_arg, U rows_arg) {
    OpenPage(0, 0x340, 0x100);
    const int cols = static_cast<int>(cols_arg & 0xFF), rows = static_cast<int>(rows_arg & 0xFF);
    const int xi = static_cast<int>(x), yi = static_cast<int>(y);
    for (int i = 1; i < rows - 1; ++i)
        for (int j = 1; j < cols - 1; ++j) BH_CALL(Menu_DrawPiece)(xi + 8 * j, yi + 8 * i, 0x25, 1);
    for (int j = 2; j < cols - 2; ++j) {
        BH_CALL(Menu_DrawPiece)(xi + 8 * j, yi, 0x24, 1);
        BH_CALL(Menu_DrawPiece)(xi + 8 * j, yi + 8 * rows - 8, 0x28, 1);
    }
    for (int i = 2; i < rows - 2; ++i) {
        BH_CALL(Menu_DrawPiece)(xi, yi + 8 * i, 0x2A, 1);
        BH_CALL(Menu_DrawPiece)(xi + 8 * cols - 8, yi + 8 * i, 0x2B, 1);
    }
    const int xr = xi + 8 * cols - 0x10, yb = yi + 8 * rows - 0x10;
    BH_CALL(Menu_DrawPiece)(xi, yi, 0x23, 1);
    BH_CALL(Menu_DrawPiece)(xr, yi, 0x26, 1);
    BH_CALL(Menu_DrawPiece)(xi, yb, 0x27, 1);
    BH_CALL(Menu_DrawPiece)(xr, yb, 0x29, 1);
}

// original 0x598DC0 (PSX 0x800AD8F4): kind 1 of 0x598890, the choice box: its stack table by
// the record's +3 - GeneWin_ChoiceOpen, _ChoiceSlideIn, _ChoiceSlideOut.
extern "C" void __cdecl GeneWin_ChoiceStates(void) {
    static const U kStates[3] = {bof3::addr::GeneWin_ChoiceOpen, bof3::addr::GeneWin_ChoiceSlideIn,
                                 bof3::addr::GeneWin_ChoiceSlideOut};
    StackDispatch("GeneWin_ChoiceStates", kStates, 3);
}

// original 0x598DF0 (PSX 0x800AD954): the choice box's open state: +4 = 0x5A, +6 = -0x17, +3
// + 1.
extern "C" void __cdecl GeneWin_ChoiceOpen(void) {
    rv::As<records::WindowRecord>(Win())->x = 0x5A;
    rv::As<records::WindowRecord>(Win())->y = 0xFFE9;
    ++rv::As<records::WindowRecord>(Win())->state;
}

// original 0x598E10 (PSX 0x800AD97C): the box slides down: +6 (signed) to 0x29 by 0x10, then
// its draw at (+4, +6).
extern "C" void __cdecl GeneWin_ChoiceSlideIn(void) {
    auto* const win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->y));
    win->y = v >= 0x29 ? 0x29u : static_cast<unsigned>(v + 0x10);
    auto* const win2 = rv::As<records::WindowRecord>(Win());
    BH_CALL(GeneWin_DrawChoices)(static_cast<unsigned>(win2->x), static_cast<unsigned>(win2->y));
}

// original 0x598E50 (PSX 0x800AD9D8): the box slides up: +6 (signed) to -0x17 by 0x10, then
// its draw.
extern "C" void __cdecl GeneWin_ChoiceSlideOut(void) {
    auto* const win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->y));
    win->y = v <= -0x17 ? 0xFFE9u : static_cast<unsigned>(v - 0x10);
    auto* const win2 = rv::As<records::WindowRecord>(Win());
    BH_CALL(GeneWin_DrawChoices)(static_cast<unsigned>(win2->x), static_cast<unsigned>(win2->y));
}

// original 0x598E90: the three-choice box's draw at (x, y): three boxes of
// 0x2D by 0x14, 48 pixels apart, each with its text (the pointers at
// 0x66A164, 16 characters, at +4 / +3) and its pieces - 0x66AF44 for the
// fixed choice (0x904AA5 bit 7 and bits 0..6 the box), its text colour 0,
// the others 0x66AF3C in colour 7; with no choice fixed every box in colour 0
// and 0x66AF3C, and the hand at box (0x904AA5 & 0x7F), y + 4.
extern "C" void __cdecl GeneWin_DrawChoices(U x, U y) {
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned colour = static_cast<unsigned char>(Config_WindowColour);
        const U bx = x + 48 * k;
        BH_CALL(Menu_DrawBox)(static_cast<int>(bx), static_cast<int>(y), 0x2D, 0x14, 0, static_cast<int>(colour));
        const unsigned choice = BossSb_Countdown;
        const unsigned char* const text = move_script::At(static_cast<U>(move_script::Long(move_script::At(::bof3::Img(battle_e7::at::kChoiceTexts.v) + 4 * k))));
        U pieces = ::bof3::Img(battle_e7::at::kChoicePieces.v);
        unsigned text_colour = 0;
        if (choice & 0x80) {
            if (k == (choice & 0x7F)) pieces = ::bof3::Img(battle_e7::at::kChoicePiecesOn.v);
            else text_colour = 7;
        }
        BH_CALL(Text_DrawAt)(static_cast<int>(bx + 4), static_cast<int>(y + 3), static_cast<int>(text_colour), 0x10, text);
        BH_CALL(Menu_DrawPieces)(static_cast<int>(bx), static_cast<int>(y), Text(pieces), 1);
    }
    const unsigned choice = BossSb_Countdown;
    if (choice & 0x80) return;
    BH_CALL(Menu_DrawHand)(static_cast<int>(x + 48 * (choice & 0x7F)), static_cast<int>(y + 4), 0);
}

// original 0x598F60 (PSX 0x800ADC0C): kind 2 of 0x598890, the first list: its stack table by
// the record's +3 - GeneWin_ListOpen, _ListSlideIn, _ListSlideOut,
// _ListShiftLeft, _ListShiftRight.
extern "C" void __cdecl GeneWin_ListStates(void) {
    static const U kStates[5] = {bof3::addr::GeneWin_ListOpen, bof3::addr::GeneWin_ListSlideIn,
                                 bof3::addr::GeneWin_ListSlideOut, bof3::addr::GeneWin_ListShiftLeft,
                                 bof3::addr::GeneWin_ListShiftRight};
    StackDispatch("GeneWin_ListStates", kStates, 5);
}

// original 0x598FA0 (PSX 0x800ADC7C): the first list's open state: nine tasks (0, 0xD), each
// slot's +9 / +0xA its row and column 0..2; the record's +0x14 = 6, +8 =
// 0x10, +9 = 0x11, +3 + 1.
extern "C" void __cdecl GeneWin_ListOpen(void) {
    OpenTasks("GeneWin_ListOpen", 0xD, 6);
    Win()[8] = 0x10;
    Win()[9] = 0x11;
    ++rv::As<records::WindowRecord>(Win())->state;
}

// original 0x599020 (PSX 0x800ADD4C): the list slides right to +4 = 0x5B by 0x20, after its
// draw.
extern "C" void __cdecl GeneWin_ListSlideIn(void) {
    BH_CALL(GeneWin_DrawList)();
    auto* const win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->x));
    win->x = v >= 0x5B ? 0x5Bu : static_cast<unsigned>(v + 0x20);
}

// original 0x599050 (PSX 0x800ADD94): the list slides left by 0x20, after its draw; at +4 <=
// -0xA5 (signed) the window is freed instead (a tail jump). The bound is the
// operand as widescreen.cpp holds it. DIVERGENCE DIV-0041 (amended 2026-10-13,
// the faster slide): decided by Capcom's bound at every width; in the wide
// view the draw sees the x on the line to the bound moved out by the columns,
// and the free test Capcom's x again (Widescreen_SlideDraw / _SlideUndraw,
// docs/ultrawide-slides.md). The same in the two below.
extern "C" void __cdecl GeneWin_ListSlideOut(void) {
    const short bound = Widescreen_SlideLogic(Widescreen_SlideBound(battle_e7::at::kListOutBound.v));
    Widescreen_SlideDraw(Win(), bound, -0x20);
    BH_CALL(GeneWin_DrawList)();
    Widescreen_SlideUndraw(Win());
    auto* const win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->x));
    if (v <= bound) {
        BH_CALL(Window_FreeCurrent)();
        return;
    }
    win->x = static_cast<unsigned>(v - 0x20);
}

// original 0x599080 (PSX 0x800ADDEC): after the draw, +4 moves left by 0x20 while above 0x11
// (signed), and is held at 0x11 (the record re-read after a move).
extern "C" void __cdecl GeneWin_ListShiftLeft(void) {
    BH_CALL(GeneWin_DrawList)();
    records::WindowRecord* win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->x));
    if (v > 0x11) {
        win->x = static_cast<unsigned>(v - 0x20);
        win = rv::As<records::WindowRecord>(Win());
    }
    if (S16(static_cast<unsigned>(win->x)) < 0x11) win->x = 0x11;
}

// original 0x5990B0 (PSX 0x800ADE58): the same to the right, to 0x5B.
extern "C" void __cdecl GeneWin_ListShiftRight(void) {
    BH_CALL(GeneWin_DrawList)();
    records::WindowRecord* win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->x));
    if (v < 0x5B) {
        win->x = static_cast<unsigned>(v + 0x20);
        win = rv::As<records::WindowRecord>(Win());
    }
    if (S16(static_cast<unsigned>(win->x)) > 0x5B) win->x = 0x5B;
}

// original 0x5990E0: the first list window's draw (kind 2's states): its
// frame at (+4, +6) with title 0; each of the six rows' form byte + 1 into
// 0x939848 and the scroll bar (0x939848, top +0x12, at (+4 + 0x80, +6 + 0x18),
// 3 rows of +0x14, 0x5C high); then rows +0x12 .. +0x12 + 2 of 0x904608 (four
// bytes each): the row's number + 1 (by Area08_MessageFormat into 0x904BA0)
// in the 12-pixel font, and when its form byte is not 0xFF the form's icon,
// label (the system message 0x64ECB0[form & 0x1F], small font) and cost (by
// 0x66AF8C, 8-pixel font); each in colour 7 when the menu actor's AP is below
// the row's cost, else 0. Then in step 6 (0x904AA3) but not sub-step 5
// (0x904AA4) the cursor box at the row +0x10, else the hand there.
extern "C" void __cdecl GeneWin_DrawList(void) {
    const unsigned char* rec = Win();
    const records::WindowRecord* win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(GeneWin_DrawListFrame)(static_cast<unsigned>(win->x), static_cast<unsigned>(win->y), rec[8], rec[9], 0);
    for (unsigned k = 0; k < 6; ++k) BBattleE7(::bof3::Img(battle_e7::at::kScrollMarks.v) + k) = static_cast<unsigned char>(BBattleE7(::bof3::Img(battle_e7::at::kGeneList.v) + 3 + 4 * k) + 1);
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(Menu_DrawScrollBar)(move_script::At(::bof3::Img(battle_e7::at::kScrollMarks.v)), rec[0x12], static_cast<int>(static_cast<unsigned>(win->x) + 0x80),
                                static_cast<int>(static_cast<unsigned>(win->y) + 0x18), 3, rec[0x14], 0x5C);
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    for (unsigned i = 0; i < 3; ++i) {
        const std::int32_t index = S16(W(rec + 0x12)) + static_cast<std::int32_t>(i);
        const unsigned char* const row = move_script::At(::bof3::Img(battle_e7::at::kGeneList.v) + static_cast<U>(index * 4));
        const unsigned cost = RowCost(row);
        const unsigned colour = CostColour(cost);
        BH_CALL(Crt_sprintf)(Buf(::bof3::Img(battle_e7::at::kPrintBuf.v)), Str(::bof3::Img(battle_e7::at::kFmtNumber.v)), index + 1);
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
        BH_CALL(Text_DrawFont12)(static_cast<int>(static_cast<unsigned>(win->x) + 4), static_cast<int>(((i + 1) << 5) + static_cast<unsigned>(win->y)),
                                 static_cast<int>(colour), Text(::bof3::Img(battle_e7::at::kPrintBuf.v)));
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
        const unsigned form = BBattleE7(::bof3::Img(battle_e7::at::kGeneList.v) + 3 + static_cast<U>((S16(W(rec + 0x12)) + static_cast<std::int32_t>(i)) * 4));
        if (form == 0xFF) continue;
        BH_CALL(GeneWin_DrawFormIcon)(static_cast<unsigned>(win->x) + 0x10, static_cast<unsigned>(win->y) + (i << 5) + 0x18, form & 0x1F);
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
        const unsigned f = BBattleE7(::bof3::Img(battle_e7::at::kGeneList.v) + 3 + static_cast<U>((S16(W(rec + 0x12)) + static_cast<std::int32_t>(i)) * 4)) & 0x1F;
        const unsigned char* const label = BH_CALL(Msg_SystemPtr)(BBattleE7(::bof3::Img(battle_e7::at::kFormLabels.v) + f));
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
        BH_CALL(Text_DrawSmall)(static_cast<int>(static_cast<unsigned>(win->x) + 0x10), static_cast<int>(static_cast<unsigned>(win->y) + (i << 5) + 0x2D), colour,
                                0xFF, label);
        BH_CALL(Crt_sprintf)(Buf(::bof3::Img(battle_e7::at::kPrintBuf.v)), Str(::bof3::Img(battle_e7::at::kFmtCost.v)), cost);
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
        BH_CALL(Text_DrawFont8)(static_cast<int>(static_cast<unsigned>(win->x) + 0x58), static_cast<int>(static_cast<unsigned>(win->y) + (i << 5) + 0x2D),
                                static_cast<int>(colour), Text(::bof3::Img(battle_e7::at::kPrintBuf.v)));
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
    }
    if (BattleE4_Step3 == 6 && BattleE4_Step4 != 5) {
        BH_CALL(GeneWin_DrawCursorBox)(static_cast<unsigned>(win->x) + 5, (W(rec + 0x10) << 5) + static_cast<unsigned>(win->y) + 0x18, 0x79, 0x20, 0, 6);
        return;
    }
    BH_CALL(Menu_DrawHand)(static_cast<int>(static_cast<unsigned>(win->x) + 7), static_cast<int>(((W(rec + 0x10) + 1) << 5) + static_cast<unsigned>(win->y)), 0);
}

// original 0x599360 (PSX 0x800AE2B4): kind 3 of 0x598890: GeneWin_List2Open, _List2SlideIn,
// _List2SlideOut by the record's +3.
extern "C" void __cdecl GeneWin_List2States(void) {
    static const U kStates[3] = {bof3::addr::GeneWin_List2Open, bof3::addr::GeneWin_List2SlideIn,
                                 bof3::addr::GeneWin_List2SlideOut};
    StackDispatch("GeneWin_List2States", kStates, 3);
}

// original 0x599390 (PSX 0x800AE314): kind 3's open state: nine tasks (0, 0xE); +0x14 = 0xC,
// +8 = 0x18, +9 = 0x11, +3 + 1.
extern "C" void __cdecl GeneWin_List2Open(void) {
    OpenTasks("GeneWin_List2Open", 0xE, 0xC);
    Win()[8] = 0x18;
    Win()[9] = 0x11;
    ++rv::As<records::WindowRecord>(Win())->state;
}

// original 0x599410 (PSX 0x800AE3E4): after the second list's draw, +4 moves left to 0x5B by
// 0x20.
extern "C" void __cdecl GeneWin_List2SlideIn(void) {
    BH_CALL(GeneWin_DrawList2)();
    auto* const win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->x));
    win->x = v <= 0x5B ? 0x5Bu : static_cast<unsigned>(v - 0x20);
}

// original 0x599440 (PSX 0x800AE42C): after the draw, right by 0x20; at +4 >= 0x15B the
// window is freed instead (DIV-0041's operand; the faster slide as above).
extern "C" void __cdecl GeneWin_List2SlideOut(void) {
    const short bound = Widescreen_SlideLogic(Widescreen_SlideBound(battle_e7::at::kList2OutBound.v));
    Widescreen_SlideDraw(Win(), bound, 0x20);
    BH_CALL(GeneWin_DrawList2)();
    Widescreen_SlideUndraw(Win());
    auto* const win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->x));
    if (v >= bound) {
        BH_CALL(Window_FreeCurrent)();
        return;
    }
    win->x = static_cast<unsigned>(v + 0x20);
}

// original 0x599470 (PSX 0x800AE484): kind 4 of 0x598890: GeneWin_List3Open, _List3SlideIn,
// _List3SlideOut by the record's +3.
extern "C" void __cdecl GeneWin_List3States(void) {
    static const U kStates[3] = {bof3::addr::GeneWin_List3Open, bof3::addr::GeneWin_List3SlideIn,
                                 bof3::addr::GeneWin_List3SlideOut};
    StackDispatch("GeneWin_List3States", kStates, 3);
}

// original 0x5994A0 (PSX 0x800AE4E4): kind 4's open state: nine tasks (0, 0xE); +0x14 = 0xC,
// +3 + 1 (+8 / +9 left as they are).
extern "C" void __cdecl GeneWin_List3Open(void) {
    OpenTasks("GeneWin_List3Open", 0xE, 0xC);
    ++rv::As<records::WindowRecord>(Win())->state;
}

// original 0x599510 (PSX 0x800AE58C): after the second list's draw, +4 moves left by 0x20
// while above 0xA3, held at 0xA3.
extern "C" void __cdecl GeneWin_List3SlideIn(void) {
    BH_CALL(GeneWin_DrawList2)();
    records::WindowRecord* win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->x));
    if (v > 0xA3) {
        win->x = static_cast<unsigned>(v - 0x20);
        win = rv::As<records::WindowRecord>(Win());
    }
    if (S16(static_cast<unsigned>(win->x)) < 0xA3) win->x = 0xA3;
}

// original 0x599540 (PSX 0x800AE5F8): after the draw, right by 0x20; at +4 >= 0x143 the
// window is freed instead (DIV-0041's operand; the faster slide as above).
extern "C" void __cdecl GeneWin_List3SlideOut(void) {
    const short bound = Widescreen_SlideLogic(Widescreen_SlideBound(battle_e7::at::kList3OutBound.v));
    Widescreen_SlideDraw(Win(), bound, 0x20);
    BH_CALL(GeneWin_DrawList2)();
    Widescreen_SlideUndraw(Win());
    auto* const win = rv::As<records::WindowRecord>(Win());
    const std::int32_t v = S16(static_cast<unsigned>(win->x));
    if (v >= bound) {
        BH_CALL(Window_FreeCurrent)();
        return;
    }
    win->x = static_cast<unsigned>(v + 0x20);
}

// original 0x599570: the second list window's draw (kinds 3 and 4): its frame
// with title 1; the twelve rows' form bytes + 1 into 0x939850 and the scroll
// bar over them; then rows +0x12 .. +0x12 + 2 of 0x904620 that have a form
// byte (not 0xFF): the icon, the label and the cost as GeneWin_DrawList's
// (no row number). The hand at row +0x10 unless step 6 sub-step 5.
extern "C" void __cdecl GeneWin_DrawList2(void) {
    const unsigned char* rec = Win();
    const records::WindowRecord* win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(GeneWin_DrawListFrame)(static_cast<unsigned>(win->x), static_cast<unsigned>(win->y), rec[8], rec[9], 1);
    for (unsigned k = 0; k < 12; ++k)
        BBattleE7(::bof3::Img(battle_e7::at::kScrollMarks2.v) + k) = static_cast<unsigned char>(BBattleE7(::bof3::Img(battle_e7::at::kGeneList2.v) + 3 + 4 * k) + 1);
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    BH_CALL(Menu_DrawScrollBar)(move_script::At(::bof3::Img(battle_e7::at::kScrollMarks2.v)), rec[0x12], static_cast<int>(static_cast<unsigned>(win->x) + 0x80),
                                static_cast<int>(static_cast<unsigned>(win->y) + 0x18), 3, rec[0x14], 0x5C);
    rec = Win();
    win = rv::AsConst<records::WindowRecord>(rec);
    for (unsigned i = 0; i < 3; ++i) {
        const std::int32_t index = S16(W(rec + 0x12)) + static_cast<std::int32_t>(i);
        const unsigned char* const row = move_script::At(::bof3::Img(battle_e7::at::kGeneList2.v) + static_cast<U>(index * 4));
        const unsigned form = row[3];
        if (form == 0xFF) continue;
        const unsigned cost = RowCost(row);
        const unsigned colour = CostColour(cost);
        BH_CALL(GeneWin_DrawFormIcon)(static_cast<unsigned>(win->x) + 0x10, static_cast<unsigned>(win->y) + (i << 5) + 0x18, form & 0x1F);
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
        const unsigned f = BBattleE7(::bof3::Img(battle_e7::at::kGeneList2.v) + 3 + static_cast<U>((S16(W(rec + 0x12)) + static_cast<std::int32_t>(i)) * 4)) & 0x1F;
        const unsigned char* const label = BH_CALL(Msg_SystemPtr)(BBattleE7(::bof3::Img(battle_e7::at::kFormLabels.v) + f));
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
        BH_CALL(Text_DrawSmall)(static_cast<int>(static_cast<unsigned>(win->x) + 0x10), static_cast<int>(static_cast<unsigned>(win->y) + (i << 5) + 0x2D), colour,
                                0xFF, label);
        BH_CALL(Crt_sprintf)(Buf(::bof3::Img(battle_e7::at::kPrintBuf.v)), Str(::bof3::Img(battle_e7::at::kFmtCost.v)), cost);
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
        BH_CALL(Text_DrawFont8)(static_cast<int>(static_cast<unsigned>(win->x) + 0x58), static_cast<int>(static_cast<unsigned>(win->y) + (i << 5) + 0x2D),
                                static_cast<int>(colour), Text(::bof3::Img(battle_e7::at::kPrintBuf.v)));
        rec = Win();
        win = rv::AsConst<records::WindowRecord>(rec);
    }
    if (BattleE4_Step3 == 6 && BattleE4_Step4 == 5) return;
    BH_CALL(Menu_DrawHand)(static_cast<int>(static_cast<unsigned>(win->x) + 7), static_cast<int>(((W(rec + 0x10) + 1) << 5) + static_cast<unsigned>(win->y)), 0);
}

// original 0x599780: a list window's frame at (x, y): three boxes in the
// window colour (0x89 by 0x82, by 0x14, and 0x89 by 8 at y + 0x7A, all at x +
// 3), the title (the 5-byte string 0x66AF64 + 5 `title`, 16 characters)
// centred on x + 0x46 by six pixels a character, the piece lists 0x66AF4C and
// 0x66AF58, and its edges of pieces 1, 4, 0x16..0x1A, 0x32, 0x33. The third
// and fourth words are pushed by its callers and never read.
extern "C" void __cdecl GeneWin_DrawListFrame(U x, U y, U, U, U title) {
    const int xi = static_cast<int>(x), yi = static_cast<int>(y);
    BH_CALL(Menu_DrawBox)(xi + 3, yi + 3, 0x89, 0x82, 0, static_cast<unsigned char>(Config_WindowColour));
    BH_CALL(Menu_DrawBox)(xi + 3, yi + 3, 0x89, 0x14, 0, static_cast<unsigned char>(Config_WindowColour));
    BH_CALL(Menu_DrawBox)(xi + 3, yi + 0x7A, 0x89, 8, 0, static_cast<unsigned char>(Config_WindowColour));
    const unsigned char* const text = move_script::At(::bof3::Img(battle_e7::at::kTitles.v) + (title & 0xFF) * 5);
    const unsigned n = BH_CALL(Text_CharCount)(text);
    BH_CALL(Text_DrawAt)(xi + 0x46 - 6 * static_cast<int>(n & 0xFF), yi + 7, 0, 0x10, text);
    BH_CALL(Menu_DrawPieces)(xi, yi, Text(::bof3::Img(battle_e7::at::kListPieces.v)), 1);
    BH_CALL(Menu_DrawPieces)(xi, yi, Text(::bof3::Img(battle_e7::at::kListPieces2.v)), 1);
    for (int j = 0; j < 8; ++j) BH_CALL(Menu_DrawPiece)(xi + 8 * j + 0x28, yi, 1, 1);
    for (int j = 0; j < 12; ++j) BH_CALL(Menu_DrawPiece)(xi, yi + 8 * j + 0x18, 4, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x80, yi + 0x18, 0x16, 1);
    for (int j = 0; j < 9; ++j) BH_CALL(Menu_DrawPiece)(xi + 0x80, yi + 8 * j + 0x28, 0x17, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x80, yi + 0x70, 0x18, 1);
    BH_CALL(Menu_DrawPiece)(xi, yi + 0x78, 0x19, 1);
    for (int j = 0; j < 15; ++j) BH_CALL(Menu_DrawPiece)(xi + 8 * j + 8, yi + 0x78, 0x1A, 1);
    BH_CALL(Menu_DrawPiece)(xi + 8, yi, 0x32, 1);
    BH_CALL(Menu_DrawPiece)(xi + 0x80, yi, 0x33, 1);
}

// original 0x599910 (PSX 0x800AECDC): a form's icon at (s16 x, s16 y): page (0x340, 0x100)
// under abr 1; a SPRT of 0x28 by 0x18 from cell v = 0x66AF70[form] - u (v %
// 6) * 40, v (v / 6) * 24 - in CLUT (v * 16, 0x1FA), shade 0x80.
extern "C" void __cdecl GeneWin_DrawFormIcon(U x, U y, U form) {
    OpenPage(1, 0x340, 0x100);
    unsigned char* const p = Gfx_PacketNext;
    auto* const sprt = rv::As<records::GpuSprt>(p);  // a SPRT: Gpu_SetSprt below starts it
    const unsigned v = BBattleE7(::bof3::Img(battle_e7::at::kFormCells.v) + (form & 0xFF));
    rv::Store<unsigned>(sprt->clut, BH_CALL(Gpu_GetClut)(static_cast<int>(v << 4), 0x1FA));
    p[4] = p[5] = p[6] = 0x80;
    PutFloat(p + 8, S16(x));
    PutFloat(p + 0xC, S16(y));
    p[0x14] = static_cast<unsigned char>((v % 6) * 40);
    rv::Store<unsigned>(sprt->w, 0x28);
    rv::Store<unsigned>(sprt->h, 0x18);
    p[0x15] = static_cast<unsigned char>((v / 6) * 24);
    BH_CALL(Gpu_SetSprt)(p);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

extern "C" void __cdecl GeneWin_DrawCursorBox(U x, U y, U w, U h, U flash, U sides) {
    unsigned bright = 0xFF;
    if (flash & 0xFF) {
        const unsigned c = static_cast<unsigned char>(Frame_Counter);
        bright = (c & 8) ? ((((c & 6) << 5) + 0x3F) & 0xFF) : ((((~c) & 6) << 5) + 0x3F) & 0xFF;
    }
    unsigned char ch[3];
    for (unsigned i = 0; i < 3; ++i) ch[i] = static_cast<unsigned char>((sides & (1u << i)) ? bright : 0);
    std::int32_t xs = static_cast<std::int32_t>(x & 0xFFFF), ys = static_cast<std::int32_t>(y & 0xFFFF);
    std::int32_t yb = static_cast<std::int32_t>(h & 0xFFFF) + ys - 1;
    std::int32_t xr = static_cast<std::int32_t>(w & 0xFFFF) + xs - 1;
    if (battle_e7::g_cursor_box_signed) {
        xs = static_cast<std::int16_t>(x);
        ys = static_cast<std::int16_t>(y);
        yb = static_cast<std::int16_t>(y + h - 1);
        xr = static_cast<std::int16_t>(x + w - 1);
    }
    unsigned char* p = Gfx_PacketNext;
    BH_CALL(Gpu_SetLineF3)(p);
    PutFloat(p + 8, xs);
    PutFloat(p + 0xC, ys);
    p[4] = ch[2];
    PutFloat(p + 0x14, xs);
    p[5] = ch[1];
    p[6] = ch[0];
    PutFloat(p + 0x18, ys);
    PutFloat(p + 0x20, xs);
    PutFloat(p + 0x24, yb);
    BH_CALL(Gfx_CommitPrim)(1, 0x2C);
    p = Gfx_PacketNext;
    BH_CALL(Gpu_SetLineF4)(p);
    PutFloat(p + 8, xs);
    PutFloat(p + 0x2C, xs);
    PutFloat(p + 0x30, yb);
    PutFloat(p + 0xC, ys);
    PutFloat(p + 0x18, ys);
    PutFloat(p + 0x24, yb);
    p[4] = ch[2];
    p[5] = ch[1];
    p[6] = ch[0];
    PutFloat(p + 0x20, xr);
    PutFloat(p + 0x14, xr);
    BH_CALL(Gfx_CommitPrim)(1, 0x38);
}

// original 0x599B50: record handler 6 of Field_RunTaskRecords, `jmp
// [0x66AF94 + 4 kind]` on the current record's +2 - 0 MenuList_MemberPanel,
// 1 MenuList_MoneyBox, 2 MenuList_TimeBox, 3 MenuList_TopBarIcons, 4
// MenuList_TitleBox, and sixteen more kinds of other screens (5..20). The
// kind unchecked; a jump, as the original's.
extern "C" void __cdecl MenuList_Run(void) {
    const Step step = Entry(::bof3::Img(MenuList_Kinds_addr), RecMenuLists()[2]);
    [[clang::musttail]] return step();
}

// original 0x599B70, kind 0: the record's state from 0x66AFE8 (+3,
// unchecked), then MenuList_DrawMemberPanel of the record (re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl MenuList_MemberPanel(void) {
    Entry(::bof3::Img(MenuList_PanelStates_addr), RecMenuLists()[3])();
    menu_lists::g.draw_member(RecMenuLists());
}

// original 0x599B90 (recorded start, 114 calls on the world-map route): a
// member's panel at the record's (+4, +6): 0x573560(x, y, the record of
// 0x66972C[0x904062[3 +0xB + +0xA]], +0xC, 0); then the draw mode (page 0xF)
// and the member's number, the 8 x 8 cell (+0xA + 3, 0x1E) at (x + 9, y + 5)
// in CLUT (0x10, 0x1E0), shade 0x80, through 0x5744B0.
//
// As the original has it: the record is the argument, not 0x905B84; its
// fields are read again after the calls; Gfx_PacketNext after the panel.
extern "C" void __cdecl MenuList_DrawMemberPanel(unsigned char* rec) {
    const unsigned member = menu_lists::At(::bof3::Img(Cond_Flags_addr) + 0xd2 + rec[0xB] * 3u + rec[0xA])[0];
    const unsigned record = menu_lists::At(::bof3::Img(Character_IndexById_addr) + member)[0];
    menu_lists::g.member_body(rv::As<records::WindowRecord>(rec)->x, rv::As<records::WindowRecord>(rec)->y, record, rec[0xC], 0);
    menu_lists::g.draw_mode(Gfx_PacketNext, 0, 0, 0xF, 0);
    menu_lists::g.commit(1, 0xC);
    const unsigned clut = menu_lists::g.get_clut(0x10, 0x1E0);
    menu_lists::g.member_face(rv::As<records::WindowRecord>(rec)->x + 9, rv::As<records::WindowRecord>(rec)->y + 5, rec[0xA] + 3u, 0x1E, clut, 0x80);
}

// original 0x599C30 (MenuList_PanelStates[1]): right to x 0xB6.
extern "C" void __cdecl MenuList_SlideRightTo182(void) { Slide(4, 0x20, 0xB6, Past::kAbove); }

// original 0x599C60 (MenuList_PanelStates[3]): left to x 0x5C.
extern "C" void __cdecl MenuList_SlideLeftTo92(void) { Slide(4, -0x20, 0x5C, Past::kBelow); }

// original 0x599C90 (MenuList_PanelStates[4]): right to x 0x5C.
extern "C" void __cdecl MenuList_SlideRightTo92(void) { Slide(4, 0x20, 0x5C, Past::kAbove); }

// original 0x599CC0 (MenuList_PanelStates[7]): left to x -300 or less (off
// the left); the bound the operand of DIV-0041's faster slide.
extern "C" void __cdecl MenuList_SlideLeftOff300(void) { SlideOff(-0x20, rest_2g::at::kLeftOff300Bound.v, Past::kAtOrBelow); }

// original 0x599CF0 (MenuList_PanelStates[8]): right to x 0x11 or more.
extern "C" void __cdecl MenuList_SlideRightTo17Ge(void) { Slide(4, 0x20, 0x11, Past::kAtOrAbove); }

// original 0x599D20 (MenuList_PanelStates[10]): left to x 0xB6.
extern "C" void __cdecl MenuList_SlideLeftTo182(void) { Slide(4, -0x20, 0xB6, Past::kBelow); }

// original 0x599D50, kind 1: the state from 0x66B014, then Menu_DrawMoneyBox
// at the record's (+4, +6) with the money 0x904058.
extern "C" void __cdecl MenuList_MoneyBox(void) {
    Entry(::bof3::Img(MenuList_MoneyStates_addr), RecMenuLists()[3])();
    const unsigned char* const r = RecMenuLists();
    menu_lists::g.money_box(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y, 0, Party_Zenny);
}

// original 0x599D90 (MenuList_MoneyStates[2]): left to x 0xC8.
extern "C" void __cdecl MenuList_SlideLeftTo200(void) { Slide(4, -0x20, 0xC8, Past::kBelow); }

// original 0x599DC0, kind 2: the state from 0x66B020, then the play-time box
// 0x5746C0 at the record's (+4, +6).
extern "C" void __cdecl MenuList_TimeBox(void) {
    Entry(::bof3::Img(MenuList_TimeStates_addr), RecMenuLists()[3])();
    const unsigned char* const r = RecMenuLists();
    menu_lists::g.time_box(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y);
}

// original 0x599DF0 (MenuList_TimeStates[1]): left to x -100 (off the left);
// DIV-0041's faster slide.
extern "C" void __cdecl MenuList_SlideLeftOff100(void) { SlideOff(-0x20, rest_2g::at::kLeftOff100Bound.v, Past::kBelow); }

// original 0x599E20 (MenuList_TimeStates[2]): right to x 0x10.
extern "C" void __cdecl MenuList_SlideRightTo16(void) { Slide(4, 0x20, 0x10, Past::kAbove); }

// original 0x599E50, kind 3 (the body is the tail jump's target 0x599E70,
// reached from nowhere else): the state from 0x66B02C, then the top bar's
// seven icons, 0x6672AC[i] at (x + 16 i, y), 16 x 16. The one under the
// cursor (+0xA) is drawn last and grows: +0x10 counts the frames since the
// cursor moved (reset when +0xB, the cursor last drawn, differs; at most 3)
// and the icon is (16 + 2 n) square, n up and left. Camp's (6) is shaded
// 0x40 while +0xC refuses it, else 0x80. +0xB takes the cursor.
//
// As the original has it: the record is re-read after every call and every
// store; the count is compared as a signed word; the size is the count's low
// byte plus 8, doubled, as a byte.
extern "C" void __cdecl MenuList_TopBarIcons(void) {
    Entry(::bof3::Img(MenuList_IconStates_addr), RecMenuLists()[3])();
    unsigned char* r = RecMenuLists();
    if (r[0xB] != r[0xA]) {
        menu_lists::SetWord(r + 0x10, 0);
        r = RecMenuLists();
    }
    const short count = SMenuLists(r + 0x10);
    if (count < 3) {
        menu_lists::SetWord(r + 0x10, static_cast<std::uint16_t>(count + 1));
        r = RecMenuLists();
    }
    for (unsigned i = 0; i < 7; ++i) {
        if (i == r[0xA]) continue;
        const unsigned shade = i == 6 && r[0xC] != 0 ? 0x40 : 0x80;
        const unsigned y = rv::As<records::WindowRecord>(r)->y;
        const unsigned x = ((i << 4) + rv::As<records::WindowRecord>(r)->x) & 0xFFFF;
        menu_lists::g.icon(menu_lists::At(::bof3::Img(FieldMenu_IconIds_addr) + i)[0], x, y, 0x10, 0x10, shade);
        r = RecMenuLists();
    }
    const unsigned sel = r[0xA];
    const unsigned shade = sel == 6 && r[0xC] != 0 ? 0x40 : 0x80;
    const unsigned grow = menu_lists::Word(r + 0x10);
    const unsigned size = static_cast<unsigned char>((r[0x10] + 8) << 1);
    const unsigned y = (rv::As<records::WindowRecord>(r)->y - grow) & 0xFFFF;
    const unsigned x = ((((sel << 4) + rv::As<records::WindowRecord>(r)->x) & 0xFFFF) - grow) & 0xFFFF;
    menu_lists::g.icon(menu_lists::At(::bof3::Img(FieldMenu_IconIds_addr) + sel)[0], x, y, size, size, shade);
    r = RecMenuLists();
    r[0xB] = r[0xA];
}

// original 0x599F70 (MenuList_IconStates[2]): down to y 0x2A.
extern "C" void __cdecl MenuList_SlideDownTo42(void) { Slide(6, 0x10, 0x2A, Past::kAbove); }

// DIVERGENCE DIV-0058: under a Latin language overlay the screen title is
// centred on the width the pen will really cover (DIV-0006's advances)
// instead of on 12 units a character - the owner's "Ability sits left of
// centre" (known-defects.md D86). 0 until MenuLists_Inject patches it to 1
// under the name MenuTitleCentre, so BOF3X_ORIGINAL=MenuTitleCentre and the
// fuzz (which runs before the patch) keep the original's arithmetic.
unsigned char g_title_centre = 0;

// original 0x599FA0, kind 4: the state from 0x66B038, then the screen title -
// Menu_DrawTitleBox(x, y, 0x48, 0x13, Config's colour) at the record's (+4,
// +6) and the system text +0x10 (Msg_SystemPtr) through Text_DrawAt at (x +
// 0x25 - 6 n, y + 3), colour 0, count n, n its Text_CharCount. The Chinese
// title of each top-bar entry (docs/menu_lists.md section 3): centred for
// 12-pixel glyphs on the box's middle, x + 0x25.
//
// As the original has it: the record is re-read after the box, after the
// first count (for y) and after the second (for x); the text is counted
// twice, the first answer the count, the second the offset. DIV-0058 keeps
// every read and call and changes only the x: x + 0x25 - width / 2, the
// width in pixels - the same number as the original's for 12-unit glyphs.
extern "C" void __cdecl MenuList_TitleBox(void) {
    Entry(::bof3::Img(MenuList_TitleStates_addr), RecMenuLists()[3])();
    const unsigned char colour = static_cast<unsigned char>(Config_WindowColour);
    const unsigned char* r = RecMenuLists();
    menu_lists::g.title_box(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y, 0x48, 0x13, colour);
    r = RecMenuLists();
    const unsigned char* const text = menu_lists::g.msg(menu_lists::Word(r + 0x10));
    const unsigned count = menu_lists::g.char_count(text);
    r = RecMenuLists();
    const unsigned y = (rv::As<records::WindowRecord>(r)->y + 3u) & 0xFFFF;
    const unsigned width = menu_lists::g.char_count(text);
    r = RecMenuLists();
    const unsigned half = g_title_centre ? TextAdvance_Width(text) / 2 : width * 6;
    const int x = static_cast<int>((rv::As<records::WindowRecord>(r)->x - half + 0x25u) & 0xFFFF);
    menu_lists::g.text(x, static_cast<int>(y), 0, static_cast<int>(count), text);
}

// original 0x59A030, kind 5: the state from MenuList_StatsStates, then
// Menu_DrawStatsPanel at the record's (+4, +6) for the character record of
// party slot +0xA.
extern "C" void __cdecl MenuList_StatsPanel(void) {
    State("MenuList_StatsPanel", ::bof3::Img(MenuList_StatsStates_addr), 7);
    const unsigned char* const r = RecRest2g();
    SH_CALL(Menu_DrawStatsPanel)(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y, PartyRecord(r[0xA]));
}

// original 0x59A070 (MenuList_StatsStates[2]): left by 0x20 until x is below
// window record 4's x + 0x78 (the field menu's first member panel's right),
// where it takes that. As the original has it: the bound is the s16 at
// 0x8031F4 plus 0x78 as an int, read after the step and before the record is
// re-read; the store its low word.
extern "C" void __cdecl MenuList_SlideLeftBesidePanel(void) {
    unsigned char* const moved = RecRest2g();
    rv::As<records::WindowRecord>(moved)->x = rv::As<records::WindowRecord>(moved)->x - 0x20u;
    const U panel = WindowRecords[4].x;
    unsigned char* const r = RecRest2g();
    if (Sx(r + 4) >= static_cast<std::int32_t>(static_cast<short>(panel)) + 0x78) return;
    rv::As<records::WindowRecord>(r)->x = panel + 0x78;
    RecRest2g()[3] = 0;
}

// original 0x59A0B0 (MenuList_PanelStates[5], MenuList_StatsStates[3]): up to
// y 0x3E or less.
extern "C" void __cdecl MenuList_SlideUpTo62Le(void) { Slide(6, -0x10, 0x3E, Past::kAtOrBelow); }

// original 0x59A0E0 (MenuList_PanelStates[6], MenuList_StatsStates[4]): down
// by 0x10 until y reaches its member panel's row, 0x3E + 0x36 +0xA (the rows
// FieldMenu_PlaceWindows gives records 4..6). As the original has it: +0xA is
// read from the record re-read after the step; the bound an int, the store its
// low word.
extern "C" void __cdecl MenuList_SlideDownToPanelRow(void) {
    unsigned char* const moved = RecRest2g();
    rv::As<records::WindowRecord>(moved)->y = rv::As<records::WindowRecord>(moved)->y + 0x10u;
    unsigned char* const r = RecRest2g();
    const std::int32_t bound = 0x36 * static_cast<std::int32_t>(r[0xA]) + 0x3E;
    if (Sx(r + 6) < bound) return;
    rv::As<records::WindowRecord>(r)->y = static_cast<unsigned>(bound);
    RecRest2g()[3] = 0;
}

// original 0x59A130 (MenuList_StatsStates[5]): left to x -180 or less;
// DIV-0041's faster slide.
extern "C" void __cdecl MenuList_SlideLeftOff180(void) { SlideOff(-0x20, rest_2g::at::kLeftOff180Bound.v, Past::kAtOrBelow); }

// original 0x59A160 (MenuList_StatsStates[6]): right to x 0x89 or more.
extern "C" void __cdecl MenuList_SlideRightTo137Ge(void) { Slide(4, 0x20, 0x89, Past::kAtOrAbove); }

// original 0x59A190, kind 6: the state from MenuList_EquipStates, then
// Menu_DrawEquipPanel likewise.
extern "C" void __cdecl MenuList_EquipPanel(void) {
    State("MenuList_EquipPanel", ::bof3::Img(MenuList_EquipStates_addr), 3);
    const unsigned char* const r = RecRest2g();
    SH_CALL(Menu_DrawEquipPanel)(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y, PartyRecord(r[0xA]));
}

// original 0x59A1D0 (MenuList_EquipStates[2]): left to x 0xA6.
extern "C" void __cdecl MenuList_SlideLeftTo166(void) { Slide(4, -0x20, 0xA6, Past::kBelow); }

// original 0x59A200, kind 7: the state from MenuList_ExpStates, then
// Menu_DrawExpPanel likewise with `next` 0 (the record's EXP).
extern "C" void __cdecl MenuList_ExpPanel(void) {
    State("MenuList_ExpPanel", ::bof3::Img(MenuList_ExpStates_addr), 3);
    const unsigned char* const r = RecRest2g();
    SH_CALL(Menu_DrawExpPanel)(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y, PartyRecord(r[0xA]), 0);
}

// original 0x59A240 (MenuList_ExpStates[2], MenuList_NextLevelStates[2]):
// right to x 0x2D.
extern "C" void __cdecl MenuList_SlideRightTo45(void) { Slide(4, 0x20, 0x2D, Past::kAbove); }

// original 0x59A270, kind 8: the state from MenuList_NextLevelStates, then
// Menu_DrawExpPanel with `next` 1 (the EXP of the next level).
extern "C" void __cdecl MenuList_NextLevelPanel(void) {
    State("MenuList_NextLevelPanel", ::bof3::Img(MenuList_NextLevelStates_addr), 3);
    const unsigned char* const r = RecRest2g();
    SH_CALL(Menu_DrawExpPanel)(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y, PartyRecord(r[0xA]), 1);
}

// original 0x59A2B0 (MenuList_ExpStates[1], MenuList_NextLevelStates[1]): left
// to x -110; DIV-0041's faster slide.
extern "C" void __cdecl MenuList_SlideLeftOff110(void) { SlideOff(-0x20, rest_2g::at::kLeftOff110Bound.v, Past::kBelow); }

// original 0x59A2E0, kind 9: the state from MenuList_WideTitleStates; the
// title box Menu_DrawTitleBox(+4, +6, 0x118, 0x13, the style byte); when the
// word +0x10 is not 0, its system text (Msg_SystemPtr) at (+4 + 7, +6 + 3) in
// colour 0, and for +0x10 0x1A or 0x31 system text 0xF over it at the same
// place. The record re-read after each call, as the original.
extern "C" void __cdecl MenuList_WideTitleBox(void) {
    State("MenuList_WideTitleBox", ::bof3::Img(MenuList_WideTitleStates_addr), 3);
    const unsigned colour = static_cast<unsigned char>(Config_WindowColour);
    const unsigned char* r = RecRest2g();
    SH_CALL(Menu_DrawTitleBox)(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y, 0x118, 0x13, static_cast<int>(colour));
    r = RecRest2g();
    if (move_script::Word(r + 0x10) == 0) return;
    const unsigned char* text = SH_CALL(Msg_SystemPtr)(move_script::Word(r + 0x10));
    r = RecRest2g();
    SH_CALL(Text_DrawAt)(static_cast<int>((rv::As<records::WindowRecord>(r)->x + 7) & 0xFFFF), static_cast<int>((rv::As<records::WindowRecord>(r)->y + 3) & 0xFFFF), 0, 0xFF,
                         text);
    r = RecRest2g();
    const unsigned id = move_script::Word(r + 0x10);
    if (id != 0x1A && id != 0x31) return;
    text = SH_CALL(Msg_SystemPtr)(0xF);
    r = RecRest2g();
    SH_CALL(Text_DrawAt)(static_cast<int>((rv::As<records::WindowRecord>(r)->x + 7) & 0xFFFF), static_cast<int>((rv::As<records::WindowRecord>(r)->y + 3) & 0xFFFF), 0, 0xFF,
                         text);
}

// original 0x59A3A0: up 0x10 a frame to y -20 (off the top).
extern "C" void __cdecl MenuSlide_UpOff(void) { SlideY(-0x10, -20, false); }

// original 0x59A3D0: down 0x10 a frame to y 0x10.
extern "C" void __cdecl MenuSlide_DownTo16(void) { SlideY(0x10, 0x10, true); }

// original 0x59A400, kind 10 (no state table): Menu_DrawCursorBox at the
// record's (+4, +6), its width and height the word pair +0xA of
// MenuList_CursorBoxSizes (four pairs; the byte unbounded, read in place), the
// blink +0xB, the sides 6.
extern "C" void __cdecl MenuList_CursorBox(void) {
    const unsigned char* const r = RecRest2g();
    const U pair = ::bof3::Img(MenuList_CursorBoxSizes_addr) + 4u * r[0xA];
    SH_CALL(Menu_DrawCursorBox)(rv::As<records::WindowRecord>(r)->x, rv::As<records::WindowRecord>(r)->y, move_script::Word(move_script::At(pair)), move_script::Word(move_script::At(pair + 2)), r[0xB], 6);
}

// original 0x59A440, kind 11: the state from MenuList_IconWheelStates;
// Menu_DrawIconWheel(+4, +6, +0xA, +0xC, the s16 +0x10); then while +0xC (the
// lit bits) is not 0 the angle +0x10 steps by 0x40 and Menu_DrawCursorBox
// frames the wheel at (+4 + 4, +6), 0x44 by 0x34, sides 6.
//
// As the original has it: x and the kind carry the angle's sign-extension
// above them (the register it was loaded into); the record re-read after the
// wheel and after the step.
extern "C" void __cdecl MenuList_IconWheel(void) {
    State("MenuList_IconWheel", ::bof3::Img(MenuList_IconWheelStates_addr), 3);
    const unsigned char* r = RecRest2g();
    const std::int32_t angle = Sx(r + 0x10);
    const U above = static_cast<U>(angle);
    SH_CALL(Menu_DrawIconWheel)(static_cast<int>((above & 0xFFFF0000u) | rv::As<records::WindowRecord>(r)->x), rv::As<records::WindowRecord>(r)->y,
                                (above & 0xFFFFFF00u) | r[0xA], r[0xC], angle);
    unsigned char* const lit = RecRest2g();
    if (lit[0xC] == 0) return;
    move_script::SetWord(lit + 0x10, move_script::Word(lit + 0x10) + 0x40u);
    r = RecRest2g();
    SH_CALL(Menu_DrawCursorBox)(static_cast<int>((rv::As<records::WindowRecord>(r)->x + 4) & 0xFFFF), rv::As<records::WindowRecord>(r)->y, 0x44, 0x34, 0, 6);
}

// original 0x59A4B0 (MenuList_IconWheelStates[1]): right by 0x20 until x is
// past 0x140 + 75 (+0xB & 1) - off the right, one column further for an odd
// +0xB. As the original has it: +0xB from the record re-read after the step.
extern "C" void __cdecl MenuList_SlideRightOffColumn(void) {
    unsigned char* const moved = RecRest2g();
    rv::As<records::WindowRecord>(moved)->x = rv::As<records::WindowRecord>(moved)->x + 0x20u;
    unsigned char* const r = RecRest2g();
    const std::int32_t bound = 75 * static_cast<std::int32_t>(r[0xB] & 1) + 0x140;
    if (Sx(r + 4) <= bound) return;
    rv::As<records::WindowRecord>(r)->x = static_cast<unsigned>(bound);
    RecRest2g()[3] = 0;
}

// original 0x59A510 (MenuList_IconWheelStates[2]): left by 0x20 until x is
// below 75 ((+0xB & 1) + 2) - 150 or 225 - where it takes that.
extern "C" void __cdecl MenuList_SlideLeftToColumn(void) {
    unsigned char* const moved = RecRest2g();
    rv::As<records::WindowRecord>(moved)->x = rv::As<records::WindowRecord>(moved)->x - 0x20u;
    unsigned char* const r = RecRest2g();
    const std::int32_t bound = 75 * (static_cast<std::int32_t>(r[0xB] & 1) + 2);
    if (Sx(r + 4) >= bound) return;
    rv::As<records::WindowRecord>(r)->x = static_cast<unsigned>(bound);
    RecRest2g()[3] = 0;
}

// original 0x59A560, kind 12: the state from MenuList_ItemListStates, then
// Menu_DrawItemList of the record (re-read).
extern "C" void __cdecl MenuList_ItemList(void) {
    State("MenuList_ItemList", ::bof3::Img(MenuList_ItemListStates_addr), 5);
    SH_CALL(Menu_DrawItemList)(RecRest2g());
}

// original 0x59A580: left 0x20 a frame to x -200 (off the left), the bound
// read as widescreen.cpp holds the operand. DIVERGENCE DIV-0041 (amended
// 2026-10-13, the faster slide): decided by that bound at every width; in the
// wide view the box is drawn to the bound moved out by the columns
// (Widescreen_SlideHeld, docs/ultrawide-slides.md).
extern "C" void __cdecl MenuSlide_LeftOff(void) {
    const short bound = Widescreen_SlideLogic(Widescreen_SlideBound(menu_lists::at::kLeftOffBound));
    const short x_read = SMenuLists(RecMenuLists() + 4);
    SlideX(-0x20, bound, false);
    Widescreen_SlideHeld(RecMenuLists(), x_read, bound);
}

// original 0x59A5B0: right 0x20 a frame to x 0x11.
extern "C" void __cdecl MenuSlide_RightTo17(void) { SlideX(0x20, 0x11, true); }

// original 0x59A5E0: right 0x20 a frame to x 0x140 (off the right); DIV-0041's
// faster slide as MenuSlide_LeftOff's.
extern "C" void __cdecl MenuSlide_RightOff(void) {
    const short bound = Widescreen_SlideLogic(Widescreen_SlideBound(menu_lists::at::kRightOffBound));
    const short x_read = SMenuLists(RecMenuLists() + 4);
    SlideX(0x20, bound, true);
    Widescreen_SlideHeld(RecMenuLists(), x_read, bound);
}

// original 0x59A610 (MenuList_ItemListStates[4], MenuList_ItemPanelStates[4]
// and two tables of other handlers): left to x 0x98.
extern "C" void __cdecl MenuList_SlideLeftTo152(void) { Slide(4, -0x20, 0x98, Past::kBelow); }

// original 0x59A640, kind 13: the state from MenuList_ButtonRowStates, then
// Menu_DrawButtonRow(+4, +6, set +0xA, selected +0xB, 0). As the original has
// it: x carries the record pointer's upper half (it is loaded into the
// register that held the pointer).
extern "C" void __cdecl MenuList_ButtonRow(void) {
    State("MenuList_ButtonRow", ::bof3::Img(MenuList_ButtonRowStates_addr), 3);
    const unsigned char* const r = RecRest2g();
    SH_CALL(Menu_DrawButtonRow)(static_cast<int>((Key(r) & 0xFFFF0000u) | rv::As<records::WindowRecord>(r)->x), rv::As<records::WindowRecord>(r)->y, r[0xA], r[0xB], 0);
}

// original 0x59A680: down 0x10 a frame to y 0x26.
extern "C" void __cdecl MenuSlide_DownTo38(void) { SlideY(0x10, 0x26, true); }

// original 0x59A6B0, kind 15 (no state table): MenuList_DrawLabelList of the
// record.
extern "C" void __cdecl MenuList_LabelList(void) { SH_CALL(MenuList_DrawLabelList)(RecRest2g()); }

// original 0x59A6C0: a list of labels in the window `rec` (kind 15's draw):
// row +0xA of the label rows 0x66B158 (6 bytes: a count, then a text index a
// line) gives n lines. A box (x + 3, y + 3) 0x65 by 16 n + 0x19 in the style
// colour; the title (the first text pointer, 0x66B12C) at (x + 0x2A, y + 7);
// the frame's pieces 0x66B0CC; then each line i's text (0x66B12C by its index)
// at (x + 7, y + 16 i + 0x1A) - the line +0xC in colour 7 and again two up in
// colour 2, the line +0xB in 7 and two up in 0, any other once in 0 - with the
// pieces 0x66B0F0 at y + 16 i + 0x18 and y + 16 i + 0x20; the closing pieces
// 0x66B0FC at y + 16 n + 0x18. Every coordinate a word.
//
// As the original has it: +0xA is read again after each line (the count and
// the next index from the row it names then); the row and text indices are
// unbounded bytes, read in place; the fields are read afresh before each call.
extern "C" void __cdecl MenuList_DrawLabelList(unsigned char* rec) {
    auto x = [rec](unsigned d) { return static_cast<int>((rv::As<records::WindowRecord>(rec)->x + d) & 0xFFFF); };
    auto y = [rec](unsigned d) { return static_cast<int>((rv::As<records::WindowRecord>(rec)->y + d) & 0xFFFF); };
    auto text = [](unsigned index) { return move_script::At(static_cast<U>(move_script::Long(move_script::At(::bof3::Img(rest_2g::at::kLabelTexts.v) + 4 * index)))); };
    const unsigned colour = static_cast<unsigned char>(Config_WindowColour);
    const unsigned count = BRest2g(::bof3::Img(rest_2g::at::kLabelRows.v) + 6u * rec[0xA]);
    SH_CALL(Menu_DrawBox)(x(3), y(3), 0x65, static_cast<int>((count << 4) + 0x19), 0, static_cast<int>(colour));
    SH_CALL(Text_DrawAt)(x(0x2A), y(7), 0, 0xFF, text(0));
    SH_CALL(Menu_DrawPieces)(x(0), y(0), move_script::At(::bof3::Img(rest_2g::at::kLabelPieces.v)), 1);
    unsigned row = 0;
    unsigned line = 6u * rec[0xA];
    if (BRest2g(::bof3::Img(rest_2g::at::kLabelRows.v) + line) != 0) {
        do {
            const unsigned char* const t = text(BRest2g(::bof3::Img(rest_2g::at::kLabelRows.v) + 1 + line + row));
            const unsigned dy = row << 4;
            if (row == rec[0xC]) {
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x1A), 7, 0xFF, t);
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x18), 2, 0xFF, t);
            } else if (row == rec[0xB]) {
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x1A), 7, 0xFF, t);
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x18), 0, 0xFF, t);
            } else {
                SH_CALL(Text_DrawAt)(x(7), y(dy + 0x1A), 0, 0xFF, t);
            }
            SH_CALL(Menu_DrawPieces)(x(0), y(dy + 0x18), move_script::At(::bof3::Img(rest_2g::at::kLabelRowPieces.v)), 1);
            SH_CALL(Menu_DrawPieces)(x(0), y(dy + 0x20), move_script::At(::bof3::Img(rest_2g::at::kLabelRowPieces.v)), 1);
            row = (row + 1) & 0xFF;
            line = 6u * rec[0xA];
        } while (row < BRest2g(::bof3::Img(rest_2g::at::kLabelRows.v) + line));
    }
    SH_CALL(Menu_DrawPieces)(x(0), y((row << 4) + 0x18), move_script::At(::bof3::Img(rest_2g::at::kLabelEndPieces.v)), 1);
}

// original 0x59A8E0, kind 16: the state from MenuList_EquipCompareStates, then
// Menu_DrawEquipCompare(the character record of party slot +0xC, +4, +6, the
// set +0x20, +0xD, the record).
//
// As the original has it: x carries the upper half of +0x20 (loaded into the
// register that held it), and the record index x's upper three bytes.
extern "C" void __cdecl MenuList_EquipCompare(void) {
    State("MenuList_EquipCompare", ::bof3::Img(MenuList_EquipCompareStates_addr), 5);
    unsigned char* const r = RecRest2g();
    const U set = static_cast<U>(move_script::Long(r + 0x20));
    const U x = (set & 0xFFFF0000u) | rv::As<records::WindowRecord>(r)->x;
    const U record = (x & 0xFFFFFF00u) | PartyRecord(r[0xC]);
    SH_CALL(Menu_DrawEquipCompare)(record, static_cast<int>(x), rv::As<records::WindowRecord>(r)->y, move_script::At(set), r[0xD], r);
}

// original 0x59A930 (MenuList_EquipCompareStates[2]): left to x 0xA5.
extern "C" void __cdecl MenuList_SlideLeftTo165(void) { Slide(4, -0x20, 0xA5, Past::kBelow); }

// original 0x59A960 (MenuList_PanelStates[2], MenuList_EquipCompareStates[3],
// MenuList_AbilityStates[4] and two tables of other handlers): left to x 0x11.
extern "C" void __cdecl MenuList_SlideLeftTo17(void) { Slide(4, -0x20, 0x11, Past::kBelow); }

// original 0x59A990 (MenuList_EquipCompareStates[4]): right to x 0xA5.
extern "C" void __cdecl MenuList_SlideRightTo165(void) { Slide(4, 0x20, 0xA5, Past::kAbove); }

// original 0x59A9C0, kind 17: the state from MenuList_AbilityStates, then
// Menu_DrawAbilityPanel of the record (re-read).
extern "C" void __cdecl MenuList_AbilityPanel(void) {
    State("MenuList_AbilityPanel", ::bof3::Img(MenuList_AbilityStates_addr), 5);
    SH_CALL(Menu_DrawAbilityPanel)(RecRest2g());
}

// original 0x59A9E0 (MenuList_AbilityStates[3] and a table of another
// handler): right to x 0x96.
extern "C" void __cdecl MenuList_SlideRightTo150(void) { Slide(4, 0x20, 0x96, Past::kAbove); }

// original 0x59AA10, kind 18: the state from MenuList_ItemPanelStates, then
// Menu_DrawItemPanel of the record.
extern "C" void __cdecl MenuList_ItemPanel(void) {
    State("MenuList_ItemPanel", ::bof3::Img(MenuList_ItemPanelStates_addr), 5);
    SH_CALL(Menu_DrawItemPanel)(RecRest2g());
}

// original 0x59AA30, kind 19: the state from MenuList_ReserveStates, then
// 0x59AA80 (R2H's: the reserve list, menu_frame.cpp) of the record.
extern "C" void __cdecl MenuList_ReservePanel(void) {
    State("MenuList_ReservePanel", ::bof3::Img(MenuList_ReserveStates_addr), 3);
    SH_AT(void (__cdecl*)(unsigned char*), rest_2g::kReserveList.v)(RecRest2g());
}

// original 0x59AA50 (MenuList_AbilityStates[2], MenuList_ReserveStates[2], the
// next table's [2] and two of other handlers): left to x 0x96.
extern "C" void __cdecl MenuList_SlideLeftTo150(void) { Slide(4, -0x20, 0x96, Past::kBelow); }

// original 0x59AA80: the reserve list's panels for the record `obj` (called by
// 0x59AA30, MenuList_Kinds[19]): the call DIV-0011 re-aims (x, y, 0x12, 0x15)
// with x, y the words +4 / +6; then for each id of the list obj +0x20 points
// at, below the count obj +0xA (re-read each row), at X = x + 7, Y = y + 6 +
// 0x34 row: the member's record (0x66972C[id]); a record with +0xB bit 1 is
// drawn dim (the box's flags 0x81, the portrait's shade 1, every text colour
// 7), else flags 0x80, shade +0x10 bit 7 as 2, the name colour 0. The box (X,
// Y, 0x7D, 0x30, flags, style); the portrait (X + 0x56, Y, +9, shade); the name
// (X + 0x14, Y + 1, colour, 5); the tile Menu_DrawTile16(X + 2, Y + 0x15, 3,
// dim); the level +0xA at (X + 0x18, Y + 0x15); the status in the small font at
// (X + 0x30, Y + 0x15, the colour or 1, 0x10) - 0x66A0E8 for +0x10 bit 7 (with
// bit 5 as well: on Frame_Counter bit 5, else 0x66A0F0), 0x66A0F0 for bit 5
// alone; the tile (X + 2, Y + 0x1D, 0, dim); HP +0x18 / its maximum +0x20 at
// (X + 0x18, Y + 0x1D), the HP's colour 4 with +0x11 bit 5 and 2 at 1 or less,
// the maximum's 4 with +0x1E set (7 both when dim); the tile (X + 2, Y + 0x25,
// 1, dim); AP +0x1A (4 at a quarter of +0x22 or less, 2 at 0) and +0x22 (0, or
// 7 dim) at (X + 0x18, Y + 0x25).
//
// PartyForm_DrawReserve's twin (field_s.cpp, 0x581300) with the dimming added
// and the list behind a pointer. As the original has it: the colours and the
// dim flag go out in dwords whose upper bytes are the stack's (each callee
// reads the byte); obj +4, +6, +0xA and +0x20 are read once after the first
// call, the count again at each row's end.
extern "C" void __cdecl MenuList_ReserveWinDraw(unsigned char* obj) {
    SH_WORD(void (__cdecl*)(int, int, int, int), MenuFrame_Draw())(XRest2h(obj), YRest2h(obj), 0x12, 0x15);
    const int x7 = XRest2h(obj) + 7;
    int y = YRest2h(obj) + 6;
    if (obj[0xA] == 0) return;
    const unsigned char* list = AtRest2h(static_cast<U>(LongRest2h(obj + 0x20)));
    int text_y = y + 0x15;
    unsigned char row = 0;
    do {
        unsigned char* const rec = Record(RecordOf(*list));
        const unsigned dim = (rec[0xB] & 2) ? 1u : 0u;
        const int colour = dim ? 7 : 0;
        SH_CALL(Menu_DrawBox)(x7, y, 0x7D, 0x30, static_cast<int>(0x80 + dim), Style());
        const unsigned shade = dim ? 1u : (static_cast<unsigned>(rec[0x10]) >> 6) & 2;
        SH_CALL(Menu_DrawItemIcon)(x7 + 0x56, y, rec[9], static_cast<int>(shade));
        SH_CALL(Text_DrawAt)(x7 + 0x14, text_y - 0x14, colour, 5, rec);
        SH_CALL(Menu_DrawTile16)(x7 + 2, text_y, 3, dim);
        SH_CALL(Crt_sprintf)(PrintBuf(), FormatRest2h(::bof3::Img(rest_2h::at::kFmtLevel.v)), static_cast<unsigned>(rec[0xA]));
        const int pen = x7 + 0x18;
        SH_CALL(Text_DrawFont8)(pen, text_y, colour, TextRest2h(::bof3::Img(rest_2h::at::kPrintBuf.v)));
        const unsigned flags = rv::As<records::CharRecord>(rec)->status;
        U status = 0;
        if (flags & 0x80) {
            if (flags & 0x20) status = (Frame_Counter & 0x20) ? ::bof3::Img(rest_2h::at::kStatusA.v) : ::bof3::Img(rest_2h::at::kStatusB.v);
            else status = ::bof3::Img(rest_2h::at::kStatusA.v);
        } else if (flags & 0x20) {
            status = ::bof3::Img(rest_2h::at::kStatusB.v);
        }
        if (status != 0)
            SH_CALL(Text_DrawSmall)(x7 + 0x30, text_y, colour != 0 ? static_cast<unsigned>(colour) : 1u, 0x10, TextRest2h(status));
        const int hp_y = text_y + 8;
        SH_CALL(Menu_DrawTile16)(x7 + 2, hp_y, 0, dim);
        int c = colour;
        if (!dim) {
            if (rec[0x11] & 0x20) c = 4;
            if (rv::As<records::CharRecord>(rec)->hp <= 1) c = 2;
        }
        SH_CALL(Crt_sprintf)(PrintBuf(), FormatRest2h(::bof3::Img(rest_2h::at::kFmtFigureA.v)), static_cast<unsigned>(WordRest2h(rec + 0x18)));
        SH_CALL(Text_DrawFont8)(pen, hp_y, c, TextRest2h(::bof3::Img(rest_2h::at::kPrintBuf.v)));
        c = dim ? 7 : (rec[0x1E] != 0 ? 4 : 0);
        SH_CALL(Crt_sprintf)(PrintBuf(), FormatRest2h(::bof3::Img(rest_2h::at::kFmtFigureB.v)), static_cast<unsigned>(rv::As<records::CharRecord>(rec)->stats.max_hp));
        SH_CALL(Text_DrawFont8)(pen, hp_y, c, TextRest2h(::bof3::Img(rest_2h::at::kPrintBuf.v)));
        const int ap_y = text_y + 0x10;
        SH_CALL(Menu_DrawTile16)(x7 + 2, ap_y, 1, dim);
        c = 0;
        if (dim) {
            c = 7;
        } else {
            const unsigned quarter = (rv::As<records::CharRecord>(rec)->stats.max_ap >> 2) & 0xFFFF;
            const unsigned ap = rv::As<records::CharRecord>(rec)->ap;
            if (ap <= quarter) c = 4;
            if (ap == 0) c = 2;
        }
        SH_CALL(Crt_sprintf)(PrintBuf(), FormatRest2h(::bof3::Img(rest_2h::at::kFmtFigureA.v)), static_cast<unsigned>(rv::As<records::CharRecord>(rec)->ap));
        SH_CALL(Text_DrawFont8)(pen, ap_y, c, TextRest2h(::bof3::Img(rest_2h::at::kPrintBuf.v)));
        SH_CALL(Crt_sprintf)(PrintBuf(), FormatRest2h(::bof3::Img(rest_2h::at::kFmtFigureB.v)), static_cast<unsigned>(rv::As<records::CharRecord>(rec)->stats.max_ap));
        SH_CALL(Text_DrawFont8)(pen, ap_y, dim ? 7 : 0, TextRest2h(::bof3::Img(rest_2h::at::kPrintBuf.v)));
        y += 0x34;
        text_y += 0x34;
        ++list;
        ++row;
    } while (row < obj[0xA]);
}

// original 0x59ADE0, MenuList_Kinds[20]: the step (MenuList_GeneWinSteps by
// +3: a bare ret, MenuSlide_RightOff, 0x59AA50 - R2G's), then
// MenuList_GeneWinDraw(the record, re-read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl MenuList_GeneWinRun(void) {
    StepRest2h("MenuList_GeneWinRun", ::bof3::Img(MenuList_GeneWinSteps_addr), 3);
    SH_CALL(MenuList_GeneWinDraw)(RecRest2h());
}

// original 0x59AE00: the gene list of the window `w`. The list 0x6BE0B0: for
// each of the 18 bits of 0x904650 that is set, its number + 1, then 0s to 18
// bytes. The box (x + 3, y + 3, 0x99, 0x9A, 0, style); the scroll step
// Menu_ListScroll(+0xA, &offset, &moving, +8); from the answered top, while the
// index is below top + moving + 9 and the entry is not 0: system message
// 0x4171 + the entry at (x + 0x11, y + offset + 0x1A + 13 row) with the
// icon Menu_DrawIcon8(x + 7, that + 2, 6, dim) - the row +0xB drawn twice, lit
// (colour 7, the icon's dim 1) and then two up in colour 0 with dim 0, the
// others once. Then +0xC = the entry at +0xB; the title and count boxes
// (x + 3, y + 3, 0x99, 0x14) and (x + 3, y + 0x92, 0x99, 8); the title 0x66B1F0
// centred (x + 6 (13 - its length), y + 7, 0, 0x10); the frame pieces; the
// scroll bar (0x6BE0B0, +0xA, x + 0x90, y + 0x18, 9, 0x12, 0x74).
//
// As the original has it: the first empty entry ends the rows (the masters'
// list skips one); the list is read past its 18 bytes when the top goes past
// 9, in place; +0xB is re-read after the lit row's calls; the title's y is
// read before Text_CharCount and its x after.
extern "C" void __cdecl MenuList_GeneWinDraw(unsigned char* w) {
    const U genes = Gene_Bits;
    unsigned char n = 0;
    for (unsigned i = 0; i < 0x12; ++i)
        if (genes & (1u << i)) BRest2h(::bof3::Img(rest_2h::at::kGeneList.v) + n++) = static_cast<unsigned char>(i + 1);
    if (n < 0x12) std::memset(AtRest2h(::bof3::Img(rest_2h::at::kGeneList.v) + n), 0, 0x12u - n);
    const unsigned char style = Style();
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 3, 0x99, 0x9A, 0, style);
    unsigned char offset = 0, moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(w + 0xA, &offset, &moving, w + 8);
    int text_y = S8Rest2h(offset) + YRest2h(w) + 0x1A;
    int icon_y = text_y + 2;
    const int end = static_cast<int>(moving) + static_cast<int>(top) + 9;
    for (unsigned char i = top; static_cast<int>(i) < end; ++i) {
        if (BRest2h(::bof3::Img(rest_2h::at::kGeneList.v) + i) == 0) break;
        if (w[0xB] == i) {
            SH_CALL(Menu_DrawIcon8)(XRest2h(w) + 7, icon_y, 6, 1);
            const unsigned char* const lit = Message(BRest2h(::bof3::Img(rest_2h::at::kGeneList.v) + i) + 0x4171u);
            SH_CALL(Text_DrawAt)(XRest2h(w) + 0x11, text_y, 7, 0xFF, lit);
            text_y -= 2;
            icon_y -= 2;
        }
        SH_CALL(Menu_DrawIcon8)(XRest2h(w) + 7, icon_y, 6, 0);
        const unsigned char* const name = Message(BRest2h(::bof3::Img(rest_2h::at::kGeneList.v) + i) + 0x4171u);
        SH_CALL(Text_DrawAt)(XRest2h(w) + 0x11, text_y, 0, 0xFF, name);
        if (w[0xB] == i) {
            text_y += 2;
            icon_y += 2;
        }
        text_y += 0xD;
        icon_y += 0xD;
    }
    w[0xC] = BRest2h(::bof3::Img(rest_2h::at::kGeneList.v) + w[0xB]);
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 3, 0x99, 0x14, 0, Style());
    SH_CALL(Menu_DrawBox)(XRest2h(w) + 3, YRest2h(w) + 0x92, 0x99, 8, 0, Style());
    const int title_y = YRest2h(w) + 7;
    const unsigned char length = SH_CALL(Text_CharCount)(TextRest2h(::bof3::Img(rest_2h::at::kGeneTitle.v)));
    SH_CALL(Text_DrawAt)(6 * (0xD - static_cast<int>(length)) + XRest2h(w), title_y, 0, 0x10, TextRest2h(::bof3::Img(rest_2h::at::kGeneTitle.v)));
    Pieces(XRest2h(w), YRest2h(w), ::bof3::Img(rest_2h::at::kGenePiecesA.v));
    Pieces(XRest2h(w), YRest2h(w), ::bof3::Img(rest_2h::at::kGenePiecesB.v));
    for (int i = 0; i < 10; ++i) Piece(8 * i + 0x28 + XRest2h(w), YRest2h(w), 1);
    for (int i = 0; i < 15; ++i) Piece(XRest2h(w), 8 * i + 0x18 + YRest2h(w), 4);
    Piece(XRest2h(w) + 0x90, YRest2h(w) + 0x18, 0x16);
    for (int i = 0; i < 12; ++i) Piece(XRest2h(w) + 0x90, 8 * i + 0x28 + YRest2h(w), 0x17);
    Piece(XRest2h(w) + 0x90, YRest2h(w) + 0x88, 0x18);
    Piece(XRest2h(w), YRest2h(w) + 0x90, 0x19);
    for (int i = 0; i < 17; ++i) Piece(8 * i + 8 + XRest2h(w), YRest2h(w) + 0x90, 0x1A);
    Piece(XRest2h(w) + 8, YRest2h(w), 0x32);
    Piece(XRest2h(w) + 0x90, YRest2h(w), 0x33);
    SH_CALL(Menu_DrawScrollBar)(AtRest2h(::bof3::Img(rest_2h::at::kGeneList.v)), w[0xA], XRest2h(w) + 0x90, YRest2h(w) + 0x18, 9, 0x12, 0x74);
}

// original 0x59B220: record handler 7 of Field_RunTaskRecords' nine (the
// imm32 at 0x59E27C) - `jmp [0x66B1F8 + byte +2 * 4]`, unbounded, into the
// nineteen kinds of Window_Handler7KindTable. The kinds take nothing and
// return nothing; ecx (the record) and eax (the kind) on entry are reloaded
// by every kind (read 2026-09-25), so a call is the jump.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Window_Handler7Kinds(void) {
    Dispatch(::bof3::Img(Window_Handler7KindTable_addr), RecMenuDrawHelpers()[2]);
}

void MenuListFromMenuLists_Inject() {
    // DIVERGENCE DIV-0058: a Latin language overlay only - BOF3X_LANG set,
    // not "original", not a full-width language (as MenuVerbs_Inject
    // tests it): the screen title centred on its real width.
    {
        if (Lang_Latin()) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("MenuTitleCentre",
                             Low32(&g_title_centre),
                             &was, &is, 1);
            bof3::Log("DIV-0058    menu screen title centred on its width: %s", g_title_centre ? "on" : "off");
        }
    }
}

void MenuListFromBattleWinStates_Inject() {
    // DIVERGENCE DIV-0061: a Latin language overlay only, as MenuLists_Inject
    // tests it for DIV-0058.
    {
        if (Lang_Latin()) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("BattleBannerCentre",
                             Low32(&g_banner_centre),
                             &was, &is, 1);
            bof3::Log("DIV-0061    action banners centred on their width: %s", g_banner_centre ? "on" : "off");
        }
    }
}

void MenuListFromBattleResult_Inject() {
    // DIVERGENCE DIV-0060: a Latin language overlay only, as MenuLists_Inject
    // tests it for DIV-0058.
    {
        if (Lang_Latin()) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("BattleResultExpLayout",
                             Low32(&g_exp_layout_us),
                             &was, &is, 1);
            bof3::Log("DIV-0060    EXP line in the US layout: %s", g_exp_layout_us ? "on" : "off");
        }
    }
}

void MenuList_Inject() {
    BOF3_INJECT(BattleWin_Run);
    BOF3_INJECT(BattleWin_PartyRowStates);
    BOF3_INJECT(BattleWin_PartyRowSlideIn);
    BOF3_INJECT(BattleWin_PartyRowDraw);
    BOF3_INJECT(BattleWin_CrossStates);
    BOF3_INJECT(BattleWin_CrossGrow);
    BOF3_INJECT(BattleWin_CrossFrame);
    BOF3_INJECT(BattleWin_LabelStates);
    BOF3_INJECT(BattleWin_LabelFrame);
    BOF3_INJECT(BattleWin_BannerStates);
    BOF3_INJECT(BattleWin_BannerFrame);
    BOF3_INJECT(BattleWin_EnemyGaugeStates);
    BOF3_INJECT(BattleWin_EnemyGaugeOpen);
    BOF3_INJECT(BattleWin_EnemyGaugeFrame);
    BOF3_INJECT(BattleWin_EnemyTargetFrame);
    BOF3_INJECT(BattleWin_MemberGaugeStates);
    BOF3_INJECT(BattleWin_MemberGaugeOpen);
    BOF3_INJECT(BattleWin_MemberGaugeWait);
    BOF3_INJECT(BattleWin_MemberTargetFrame);
    BOF3_INJECT(Window_FlagAndAdvance);
    BOF3_INJECT(Window_RestoreAndBack);
    BOF3_INJECT(Window_DispatchKind);
    BOF3_INJECT(Window_HpGaugeTrack);
    BOF3_INJECT(Window_HpGaugeDrain);
    BOF3_INJECT(Window_HpGaugeFill);
    BOF3_INJECT(BattleWin_BannerRun);
    BOF3_INJECT(BattleWin_BannerSlideIn);
    BOF3_INJECT(BattleWin_BannerSlideOut);
    BOF3_INJECT(BattleWin_MessageRun);
    BOF3_INJECT(BattleWin_MessageWait);
    BOF3_INJECT(BattleWin_MessageSlideIn);
    BOF3_INJECT(BattleWin_MessageShow);
    BOF3_INJECT(BattleWin_MessageSlideOut);
    BOF3_INJECT(BattleWin_DrawMessage);
    BOF3_INJECT(BattleMsg_Advance);
    BOF3_INJECT(Text_GlyphCount);
    BOF3_INJECT(Window_Handler4Kinds);
    BOF3_INJECT(BattleResultWin_LevelUpStates);
    BOF3_INJECT(BattleResultWin_DrawLevelUp);
    BOF3_INJECT(BattleResultWin_DrawFrame);
    BOF3_INJECT(BattleResultWin_DrawDrops);
    BOF3_INJECT(BattleResultWin_ExpState);
    BOF3_INJECT(BattleResultWin_DrawExp);
    BOF3_INJECT(BattleResultWin_ZennyState);
    BOF3_INJECT(BattleResultWin_NextStep);
    BOF3_INJECT(BattleResultWin_DrawZenny);
    BOF3_INJECT(BattleResultWin_DrawItem);
    BOF3_INJECT(BattleResultWin_ExpToNext);
    BOF3_INJECT(Window_Handler5Kinds);
    BOF3_INJECT(GeneWin_GridStates);
    BOF3_INJECT(GeneWin_GridOpen);
    BOF3_INJECT(GeneWin_GridSlideIn);
    BOF3_INJECT(GeneWin_GridSlideOut);
    BOF3_INJECT(GeneWin_DrawGrid);
    BOF3_INJECT(GeneWin_DrawFrame);
    BOF3_INJECT(GeneWin_ChoiceStates);
    BOF3_INJECT(GeneWin_ChoiceOpen);
    BOF3_INJECT(GeneWin_ChoiceSlideIn);
    BOF3_INJECT(GeneWin_ChoiceSlideOut);
    BOF3_INJECT(GeneWin_DrawChoices);
    BOF3_INJECT(GeneWin_ListStates);
    BOF3_INJECT(GeneWin_ListOpen);
    BOF3_INJECT(GeneWin_ListSlideIn);
    BOF3_INJECT(GeneWin_ListSlideOut);
    BOF3_INJECT(GeneWin_ListShiftLeft);
    BOF3_INJECT(GeneWin_ListShiftRight);
    BOF3_INJECT(GeneWin_DrawList);
    BOF3_INJECT(GeneWin_List2States);
    BOF3_INJECT(GeneWin_List2Open);
    BOF3_INJECT(GeneWin_List2SlideIn);
    BOF3_INJECT(GeneWin_List2SlideOut);
    BOF3_INJECT(GeneWin_List3States);
    BOF3_INJECT(GeneWin_List3Open);
    BOF3_INJECT(GeneWin_List3SlideIn);
    BOF3_INJECT(GeneWin_List3SlideOut);
    BOF3_INJECT(GeneWin_DrawList2);
    BOF3_INJECT(GeneWin_DrawListFrame);
    BOF3_INJECT(GeneWin_DrawFormIcon);
    BOF3_INJECT(GeneWin_DrawCursorBox);
    BOF3_INJECT(MenuList_Run);
    BOF3_INJECT(MenuList_MemberPanel);
    BOF3_INJECT(MenuList_DrawMemberPanel);
    BOF3_INJECT(MenuList_SlideRightTo182);
    BOF3_INJECT(MenuList_SlideLeftTo92);
    BOF3_INJECT(MenuList_SlideRightTo92);
    BOF3_INJECT(MenuList_SlideLeftOff300);
    BOF3_INJECT(MenuList_SlideRightTo17Ge);
    BOF3_INJECT(MenuList_SlideLeftTo182);
    BOF3_INJECT(MenuList_MoneyBox);
    BOF3_INJECT(MenuList_SlideLeftTo200);
    BOF3_INJECT(MenuList_TimeBox);
    BOF3_INJECT(MenuList_SlideLeftOff100);
    BOF3_INJECT(MenuList_SlideRightTo16);
    BOF3_INJECT(MenuList_TopBarIcons);
    BOF3_INJECT(MenuList_SlideDownTo42);
    BOF3_INJECT(MenuList_TitleBox);
    BOF3_INJECT(MenuList_StatsPanel);
    BOF3_INJECT(MenuList_SlideLeftBesidePanel);
    BOF3_INJECT(MenuList_SlideUpTo62Le);
    BOF3_INJECT(MenuList_SlideDownToPanelRow);
    BOF3_INJECT(MenuList_SlideLeftOff180);
    BOF3_INJECT(MenuList_SlideRightTo137Ge);
    BOF3_INJECT(MenuList_EquipPanel);
    BOF3_INJECT(MenuList_SlideLeftTo166);
    BOF3_INJECT(MenuList_ExpPanel);
    BOF3_INJECT(MenuList_SlideRightTo45);
    BOF3_INJECT(MenuList_NextLevelPanel);
    BOF3_INJECT(MenuList_SlideLeftOff110);
    BOF3_INJECT(MenuList_WideTitleBox);
    BOF3_INJECT(MenuSlide_UpOff);
    BOF3_INJECT(MenuSlide_DownTo16);
    BOF3_INJECT(MenuList_CursorBox);
    BOF3_INJECT(MenuList_IconWheel);
    BOF3_INJECT(MenuList_SlideRightOffColumn);
    BOF3_INJECT(MenuList_SlideLeftToColumn);
    BOF3_INJECT(MenuList_ItemList);
    BOF3_INJECT(MenuSlide_LeftOff);
    BOF3_INJECT(MenuSlide_RightTo17);
    BOF3_INJECT(MenuSlide_RightOff);
    BOF3_INJECT(MenuList_SlideLeftTo152);
    BOF3_INJECT(MenuList_ButtonRow);
    BOF3_INJECT(MenuSlide_DownTo38);
    BOF3_INJECT(MenuList_LabelList);
    BOF3_INJECT(MenuList_DrawLabelList);
    BOF3_INJECT(MenuList_EquipCompare);
    BOF3_INJECT(MenuList_SlideLeftTo165);
    BOF3_INJECT(MenuList_SlideLeftTo17);
    BOF3_INJECT(MenuList_SlideRightTo165);
    BOF3_INJECT(MenuList_AbilityPanel);
    BOF3_INJECT(MenuList_SlideRightTo150);
    BOF3_INJECT(MenuList_ItemPanel);
    BOF3_INJECT(MenuList_ReservePanel);
    BOF3_INJECT(MenuList_SlideLeftTo150);
    BOF3_INJECT(MenuList_ReserveWinDraw);
    BOF3_INJECT(MenuList_GeneWinRun);
    BOF3_INJECT(MenuList_GeneWinDraw);
    BOF3_INJECT(Window_Handler7Kinds);
}
