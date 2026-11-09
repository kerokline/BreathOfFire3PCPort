// Round fourteen group R4F (docs/rest_4f.md): 52 functions in 0x460CB0..0x464B9E -
// the cut's 50 rows for R4F (analysis/round14_cut.tsv), the Config screen's
// controller step 0x4613B0 no list had, and kind 2's 0x464970 (a table cell and
// two states' shared tail) - each read with capstone to its last instruction
// (2026-10-05). Two things live in the band, neither the community village:
//
//   the Config screen   field menu state 7's own machine (ConfigMenu_Body
//               0x590340 jumps to ConfigScreen_Run): its opening, the top bar's
//               two buttons, the six rows, the controller panel, its closing,
//               and the panel draws (docs/config-screen.md has the text);
//               WorldMap_ExitRecords, the exit list Field_ExitFromCell walks
//   effect kinds 2, 7, 8, 9 and 0xB   the states E1A's dispatchers jump to and
//               the draws they call: kind 7 a full sprite fading (its states 2..4),
//               kind 8 grey lines along the ground to an object, kind 9 a fan
//               that opens blade by blade and spins, kind 0xB a quad that rises
//               from the first extra sprite and follows it, kind 2 a panel that
//               slides in and a copy of the leader's sprite recoloured
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies; Sprite_Current
// and the menu block are read again wherever the original reads them again
// after a call. The call sites and operands DIV-0011, DIV-0017, DIV-0026 and
// DIV-0051 rewrite inside these functions are read at every call as
// menu_frame.cpp and config_text.cpp hold them (docs/operand-store.md), so
// each divergence holds for ours as it does for Capcom's code
// (docs/rest_4f.md section 6). The x87 arithmetic is done in long double, as the
// originals' fild / fsub / fadd / fstp chains are (rest_3f.cpp's form). Where the
// original indexes past a table of handlers or of records, ours aborts with a
// message (docs/rest_4f.md section 7).
#include "game/rest_4f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_4f_callees.h"
#include "game/scenario_harness.h"
#include "game/config_text.h"
#include "game/menu_frame.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_4f::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;
using HandlerRest4f = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }
unsigned char& BRest4f(U a) { return move_script::At(a)[0]; }
U LRest4f(U a) { return static_cast<U>(Long(move_script::At(a))); }
std::int32_t SLRest4f(const unsigned char* p) { return Long(p); }
U WRest4f(U a) { return move_script::Word(move_script::At(a)); }
void SetWRest4f(U a, U v) { move_script::SetWord(move_script::At(a), v); }
// Config_DrawControllerPanel's frame rows: the imm8 of its `push 0xF`
// (at::kPadFrameH), which nothing patches - the function's own constant, ours
// since the function is (registered against the code in Rest4F_Inject).
constexpr int kPadFrameRows = 0xF;
// The call sites and operands DIV-0011 / 0017 / 0026 / 0051 patch inside these
// bodies are read as menu_frame.cpp and config_text.cpp hold them
// (MenuFrame_Draw, ConfigText_Operands; docs/operand-store.md).

// jmp / call [table + byte * 4]: the table's `entries` handlers read in place (the
// fuzz swaps the cells for recorders); a Fatal past them, where the original
// goes through the dword after - the next table's cell or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u handlers of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4f.md section 7)",
                    who, byte, index, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<HandlerRest4f>(static_cast<std::uintptr_t>(LRest4f(table + 4 * index)))();
}

// ===========================================================================
// The Config screen
// ===========================================================================

void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

}  // namespace

// ===========================================================================
// Kind 7's states 2..4 (EffectKind07_States; E1A's EffectKind07_Start /
// _FadeIn are 0 and 1)
// ===========================================================================

// ===========================================================================
// Kind 8 (EffectKind08_States: 0..2 here, 3 Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 9 (EffectKind09_States: ten)
// ===========================================================================

// ===========================================================================
// Kind 0xB (EffectKind0B_States: five)
// ===========================================================================

// ===========================================================================
// Kind 2 (EffectKind02_States: 0, 2, 3, 4 here; 1 is BareRet)
// ===========================================================================

void Rest4F_Inject() {
    {
        const std::uint8_t rows = kPadFrameRows;
        bof3::OperandExpect("Config_DrawControllerPanel frame rows", at::kPadFrameH.v, &rows, 1);
    }
    if (bof3::WantsShadow("rest_4f")) rest_4f::SelfTest();
}
