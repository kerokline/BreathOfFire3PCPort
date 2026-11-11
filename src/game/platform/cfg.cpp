// Moved here by tools/move_funcs.py from rest_2h.cpp, shell.cpp (docs/source-layout-round.md).
#include "game/platform/cfg.h"

#include <cstdint>
#include <cstring>
#include <dinput.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2h.h"
#include "game/rest_2h_callees.h"
#include "game/scenario_harness.h"
#include "game/shell.h"
#include "game/shell_callees.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/exe_host.h"
#include "hook/host.h"
#include "hook/log.h"
#include "input/bindings.h"

namespace {

namespace at = rest_2h::at;

}  // namespace

using namespace shell;

namespace shell {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

}  // namespace

}  // namespace shell

// original 0x5A9860: Cfg_Load's key table, 32 dwords from `table` into
// Key_Table (rep movsd, forward).
extern "C" void __cdecl Cfg_SetKeyTable(const void* table) {
    const auto* const from = static_cast<const unsigned char*>(table);
    auto* const to = reinterpret_cast<unsigned char*>(Key_Table);
    for (unsigned i = 0; i < 0x20; ++i) {
        std::uint32_t v;
        std::memcpy(&v, from + 4 * i, sizeof v);
        std::memcpy(to + 4 * i, &v, sizeof v);
    }
}

// original 0x5A9880; sole caller Cfg_Load. The 32 dwords of Key_TableDefault
// to Key_Table, first to last (rep movsd; the two do not overlap) - the
// table as the engine's rule makes it (input::DefaultKeyTable), which
// Shell_MapDefaultKeys found equal to BOF3.exe's where there is a file.
extern "C" void __cdecl Cfg_SetDefaultKeys(void) {
    unsigned char rule[0x80];
    bof3x::input::DefaultKeyTable(rule, sizeof rule);
    std::memcpy(At(::bof3::Img(Key_Table_addr)), rule, sizeof rule);
}

void Cfg_Inject() {
    BOF3_INJECT(Cfg_SetKeyTable);
    BOF3_INJECT(Cfg_SetDefaultKeys);
}
