// Group R2H of round fourteen (wave two): 36 functions at 0x59AA80..0x5A9874 -
// the cut's 34 rows (analysis/round14_cut.tsv), the start in their span no list
// had (0x59C810) and 0x59E160, a menu draw the catalogue filed under the
// renderer by its address range - each read to its last instruction with
// capstone (2026-10-04) and fuzzed through the scenario harness's field mode
// (rest_2h_fuzz.cpp). docs/rest_2h.md has them one row each.
//
// Most are window kinds: a record of WindowRecords (0x803160, 22 of 0x24) is
// the dword at 0x905B84 while Field_RunTaskRecords runs it; +2 is its kind
// (record handler 7 jumps through Window_Handler7KindTable, handler 8 through
// Window_Handler8KindTable, handler 6 through MenuList_Kinds, all ours), +3 its
// step, +4 / +6 x / y (s16), +0xA.. the kind's bytes. A "run" calls its step
// through a .data step table by +3, then draws the record (re-read); a "slide"
// moves x or y and, past a bound (signed 16-bit compares), stores the bound and
// sets the step to 0 (DI's, docs/menu_draw_helpers.md section 1).
//
// Every one is a faithful replacement. The record pointer is re-read where the
// original re-reads it (after every call; a volatile load). The originals push
// coordinates built by 16-bit moves and bytes built by 8-bit moves in registers
// whose upper bits are left over; every callee reads only the low word or byte
// (docs/rest_2h.md section 3), so ours passes them zero-extended. Where the
// original indexes a step table by +3, which it never bounds, ours aborts past
// the table (round9 doc section 6); where it reads .data in place by an
// unbounded byte that stays inside the image, ours reads the same bytes.
//
// Two call sites inside these bodies are re-aimed by divergences, and ours
// calls whatever the site reaches, read from its rel32, so each divergence and
// its BOF3X_ORIGINAL switch work for ours as for Capcom's body: DIV-0011's
// Menu_DrawFrame at MenuList_ReserveWinDraw's first call (0x59AA98,
// menu_frame.cpp), DIV-0059's ListTitle_DrawAt at BattleEquipWin_DrawItems'
// title (0x59DEFA, battle_draw.cpp, a Latin overlay only). DIV-0041's faster
// slide at MasterWin_SlideOut (bound 0x59C136, read as widescreen.cpp holds it).
#include "game/rest_2h.h"

#include <windows.h>
#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2h_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_2h::at;
using U = std::uint32_t;
constexpr auto& AtRest2h = move_script::At;
constexpr auto& LongRest2h = move_script::Long;
using move_script::SetWord;
constexpr auto& WordRest2h = move_script::Word;
using Handler = void (__cdecl*)();

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

// A slide: x (+4) or y (+6) moved by `by`; past `bound` (signed 16-bit; `below`
// says which side) it is set to `to` and the step +3 goes to 0. The originals
// re-read 0x905B84 between the stores with nothing called in between.
void Slide(unsigned at_word, int by, short bound, bool below, short to) {
    unsigned char* const w = RecRest2h();
    move_script::SetWord(w + at_word, static_cast<unsigned>(WordRest2h(w + at_word) + by));
    const auto v = static_cast<short>(WordRest2h(w + at_word));
    if (below ? v < bound : v > bound) {
        move_script::SetWord(w + at_word, static_cast<std::uint16_t>(to));
        w[3] = 0;
    }
}

// CharacterRecords' record n (0xA4 each), unchecked; a member id's record
// index, 0x66972C[id] read in place (the byte is unbounded, the table is 24).
unsigned char* Record(U n) { return AtRest2h(::bof3::Img(CharacterRecords_addr) + n * rest_2h::at::kRecordStride); }
U RecordOf(U id) { return BRest2h(::bof3::Img(rest_2h::at::kMemberMap.v) + (id & 0xFF)); }

char* PrintBuf() { return reinterpret_cast<char*>(AtRest2h(::bof3::Img(rest_2h::at::kPrintBuf.v))); }
const char* FormatRest2h(U address) { return reinterpret_cast<const char*>(AtRest2h(address)); }
const unsigned char* TextRest2h(U address) { return AtRest2h(address); }
const unsigned char* Message(unsigned id) { return SH_CALL(Msg_SystemPtr)(id & 0xFFFF); }

void PutFloat(unsigned char* at, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

void Piece(int x, int y, unsigned id) { SH_CALL(Menu_DrawPiece)(x, y, id, 1); }
void Pieces(int x, int y, U list) { SH_CALL(Menu_DrawPieces)(x, y, TextRest2h(list), 1); }

}  // namespace

// ===========================================================================
// MenuList_Kinds' reserve list and kind 20, the gene list
// ===========================================================================

// ===========================================================================
// Window_Handler7KindTable's kinds 11..18 (the shop's set after DI's 0..10)
// ===========================================================================

// ===========================================================================
// The masters
// ===========================================================================

// ===========================================================================
// Window_Handler8KindTable's kinds 3..5 and BattleMenuWin_ItemListSteps[3]
// ===========================================================================

// ===========================================================================
// The platform strays
// ===========================================================================

// ===========================================================================

void Rest2H_Inject() {
    bof3::Log("rest_2h: MasterWin_SlideOut's bound %d (0x59C136; Capcom's -120 at every width since DIV-0041's faster slide)",
              static_cast<int>(Widescreen_SlideBound(at::kMasterBound.v)));
    if (bof3::WantsShadow("rest_2h")) rest_2h::SelfTest();
}
