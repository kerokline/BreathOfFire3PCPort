// Group R4E of round fourteen (wave four): 48 functions at 0x45E870..0x460CAD -
// the cut's 48 rows for R4E (analysis/round14_cut.tsv), every one a function
// (26 hidden starts, each with its own frame and ret, reached through a .data
// table or a tail jmp), each read to its last instruction with capstone
// (2026-10-05) and fuzzed through the scenario harness's field mode
// (rest_4e_fuzz.cpp). docs/rest_4e.md has them one row each.
//
// The community band (the faerie village, areas 175..185) is resident code on
// the PC, reached from the field through the tables 0x652A70 and 0x652AE4
// (their readers are R4B's and R4D's) and by R4B's and R4D's calls. Its state
// bytes 0x939A3E (a mode) and 0x939A40 (a step) and the entry 0x9039F5 are
// the facility's; Field_Request 2 is a message open.
//
// Every one is a faithful replacement. Where the original indexes a table by a
// byte it never bounds, ours aborts with a message (round9 doc section 6; the
// policy is docs/rest_4e.md section 7, said once for the group). Every call
// goes through the harness (SH_CALL / SH_AT), so the start-up fuzz can stand
// recorders in for the callees; the cells the originals read again after a
// call are read again here after it.
#include "game/rest_4e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/faerie_name.h"
#include "game/move_script_bytes.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/rest_4e_callees.h"
#include "game/rdata_consts.h"
#include "game/text_advance.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;
using HandlerRest4e = void (__cdecl*)();

// --- the cells ------------------------------------------------------------------------

// kMode: now Rest4b_BoardState (symbols.toml). u8: the facility's mode (the tables' index)
// kNameMode: now Rest4c_Step (symbols.toml). u8: zeroed by the name commits (R4D's step)
// kStep: now Rest4b_BoardStep (symbols.toml). u8: the mode's step
// kStyle: now Config_WindowColour (symbols.toml). u8: the window style (Menu_DrawBox's colour)
// kFontPage: now Rest4f_Setting3 (symbols.toml). u8: 0 or not, which label CommuMusic_DrawList shows

constexpr ::bof3::image::Va kEntryNames{0x9048F0};
constexpr unsigned kEntryCount = 60;
// kEntryDword: now Game_BattleCount (symbols.toml). u32: copied into the entry's +4 when an item is given

constexpr U kMemberStride = 0xA4;
constexpr unsigned kMemberCount = 7;

}  // namespace
namespace {

constexpr ::bof3::image::Va kWinOff{WindowRecords_addr + 3}; // u8: window 0 shut (the slide waits for 0)

constexpr ::bof3::image::Va kTextRecords{0x904CE0};   // Text_Records (0x20 each)
constexpr ::bof3::image::Va kScriptPool{0x803580};    // Msg_OpenScript's pool: base + u16[base + 2 id]

unsigned char& BRest4e(U address) { return move_script::At(address)[0]; }
U W(U address) { return move_script::Word(move_script::At(address)); }
U L(U address) { return static_cast<U>(Long(move_script::At(address))); }
void SetL(U address, U v) { SetLong(move_script::At(address), static_cast<std::int32_t>(v)); }
U Key(const void* p) { return Low32(p); }
template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}
template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }
float F(int v) { return static_cast<float>(v); }
int S16(U v) { return static_cast<short>(v); }
const unsigned char* Message(unsigned id) { return move_script::At(::bof3::Img(kScriptPool.v) + W(::bof3::Img(kScriptPool.v) + 2 * id)); }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void Run(const char* who, U table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4e.md section 7)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<HandlerRest4e>(static_cast<std::uintptr_t>(L(table + 4 * index)))();
}

// --- the callees ------------------------------------------------------------------------
void Commit(unsigned size) { SH_CALL(Gfx_CommitPrim)(1, size); }
void DrawMode(unsigned dtd, unsigned tpage, const short* window) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, static_cast<int>(dtd), tpage,
                             Low32(window));
    Commit(0xC);
}
void Grey(unsigned char* p) { p[4] = p[5] = p[6] = 0x80; }
void OpenMessage(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }
void Box(int x, int y, int w, int h, int flags) { SH_CALL(Menu_DrawBox)(x, y, w, h, flags, static_cast<unsigned char>(Config_WindowColour)); }

}  // namespace

// ===========================================================================
// The panels: an entry's, a member's, and their pieces
// ===========================================================================

// ===========================================================================
// The two name commits
// ===========================================================================

// ===========================================================================
// The track list (CommuMusic_Modes by the mode 0x939A3E)
// ===========================================================================

// ===========================================================================
// The item handed to an entry (CommuItem_Modes by the mode 0x939A3E)
// ===========================================================================

// ===========================================================================
// The ranked lists (CommuRank_Modes by the mode 0x939A3E)
// ===========================================================================

void Rest4E_Inject() {
    if (bof3::WantsShadow("rest_4e")) rest_4e::SelfTest();
}
