// The field menu's item effects, and the few small functions next to them in
// the takeover queue. docs/item-use.md.
//
//   - ItemUse_Dispatch 0x497680 and the 33 handlers of ItemUse_Handlers
//     (0x496CC0..0x497670): pointer-reached, so in no function list until
//     2026-09-22 - pe_funcs.py had folded them into 0x496AD0's 0xBAA bytes.
//     The PSX twins are START.EMI's 0x801E7E34..0x801E8C1C, read side by side.
//   - The character-stat helpers under them, 0x590CE0..0x590F60 (the PSX
//     boot EXE's 0x80165C70..0x801660DC).
//   - The message box's two commits behind the area descriptors, 0x4981C0 and
//     0x4983C0 (group H left them: docs/msgbox.md section 4), and the sixteen
//     handlers MsgBox_SystemChoice 0x498A30 reaches for choice ids 0x80..0x8F.
//   - The pad auto-repeat Input_AutoRepeat 0x461EB0.
//
// Every call goes through item_use::g (item_use_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike.
#include "game/item_use.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/item_use_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace item_use {

using move_script::At;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Char_HealHp,
    Char_HealAp,
    Char_ClearStatus,
    Stat_AddCap999,
    Stat_AddCap99,
    Stat_AddClamped,
    Party_Count,
    Rand,
    Char_RecalcStats,
    Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(kInventoryAdd.v)),
    Flags_Test,
    Fn<void (__cdecl*)()>(::bof3::Code(kSystemChoice.v)),
};
Callees g = kOriginals;

}  // namespace item_use

using namespace item_use;

namespace {

// The record a (id, battle) pair names, as every function here computes it:
// the battle flag's low byte picks the persistent record or the member's
// working copy, the id's low byte indexes it, unchecked.
unsigned char* CharRecord(unsigned id, unsigned battle) {
    const unsigned i = id & 0xFF;
    return (battle & 0xFF) == 0 ? item_use::At(::bof3::Img(CharacterRecords_addr) + i * item_use::at::kCharStride) : item_use::At(::bof3::Img(item_use::at::kWorkRecords.v) + i * item_use::at::kWorkStride);
}

// Either answer is a records::CharRecord: the persistent record 0x903A70 + i
// * 0xA4, or the working copy 0x802DC0 + i * 0x14C, which is PartyMember
// +0x80, a copy of the character record (records.h CharRecord, PartyMember;
// docs/item-use.md section 2). The fields here are the record's +0x10,
// +0x18, +0x1A, +0x1E, +0x20, +0x22 (item_use_callees.h at::kStatus..kMaxAp).
records::CharRecord* CharRec(unsigned id, unsigned battle) { return rv::As<records::CharRecord>(CharRecord(id, battle)); }

}  // namespace

// ===========================================================================
// The dispatcher

// ===========================================================================
// The 33 handlers, in table order (ItemUse_Handlers entry in brackets)

// original 0x497120 (PSX 0x801E8464) [17], 0x497190 [18], 0x497200 [19], 0x497270 [20]:
// Stat_AddClamped on a base stat and its effective copy, each answer's low
// byte kept. As the original has it: Stat_AddClamped answers 0 unless it
// clamped, so these answer 4 for any stat below 999 - "used" either way.
// ===========================================================================
// The character-stat helpers

// ===========================================================================
// The message box's two commits

// ===========================================================================
// The sixteen system choices (MsgBox_SystemChoice's table, ids 0x80..0x8F)

// ===========================================================================
// The pad auto-repeat

// ===========================================================================

void ItemUse_Inject() {
    if (bof3::WantsShadow("item_use")) item_use::SelfTest();
}
