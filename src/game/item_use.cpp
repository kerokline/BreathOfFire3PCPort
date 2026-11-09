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
    return (battle & 0xFF) == 0 ? item_use::At(::bof3::Img(CharacterRecords_addr) + i * at::kCharStride) : item_use::At(::bof3::Img(at::kWorkRecords.v) + i * at::kWorkStride);
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

// original 0x590CE0 (PSX 0x80165C70): 0 at full HP. Else HP += the amount's
// low word, filled to max when it passes max or the amount is 0, and the
// status bit 0x2000 cleared unless HP is below a quarter of max; 1.
// As the original has it: the addition wraps at 16 bits (HP 0xFFF0 + 0x20 is
// 0x10, below max, kept), and Char_ClearStatus gets the id and battle dwords
// as they were pushed.
extern "C" unsigned char __cdecl Char_HealHp(unsigned id, unsigned amount, unsigned battle) {
    auto* const r = CharRec(id, battle);
    if (r->hp == r->stats.max_hp) return 0;
    rv::Store<unsigned>(r->hp, r->hp + amount);
    if (r->hp > r->stats.max_hp || static_cast<std::uint16_t>(amount) == 0)
        rv::Store<unsigned>(r->hp, r->stats.max_hp);
    if (static_cast<std::uint16_t>(r->stats.max_hp >> 2) <= r->hp) item_use::g.clear_status(id, 0x2000, battle);
    return 1;
}

// original 0x590D70 (PSX 0x80165D3C): Char_HealHp on AP, without the status.
extern "C" unsigned char __cdecl Char_HealAp(unsigned id, unsigned amount, unsigned battle) {
    auto* const r = CharRec(id, battle);
    if (r->ap == r->stats.max_ap) return 0;
    rv::Store<unsigned>(r->ap, r->ap + amount);
    if (r->ap > r->stats.max_ap || static_cast<std::uint16_t>(amount) == 0)
        rv::Store<unsigned>(r->ap, r->stats.max_ap);
    return 1;
}

// original 0x590DE0 (PSX 0x80165E5C): 0 at 999; else add (wrapping at 16
// bits), 999 if above, 1.
extern "C" unsigned char __cdecl Stat_AddCap999(unsigned short* stat, unsigned amount) {
    auto* const p = reinterpret_cast<unsigned char*>(stat);
    const std::uint16_t v = item_use::Word(p);
    if (v == 999) return 0;
    const auto sum = static_cast<std::uint16_t>(v + amount);
    item_use::SetWord(p, sum);
    if (sum > 999) item_use::SetWord(p, 999);
    return 1;
}

// original 0x590E10 (PSX 0x80165EA0): the same on a byte, capped at 99.
extern "C" unsigned char __cdecl Stat_AddCap99(unsigned char* stat, unsigned amount) {
    const unsigned char v = *stat;
    if (v == 99) return 0;
    const auto sum = static_cast<unsigned char>(v + amount);
    *stat = sum;
    if (sum > 99) *stat = 99;
    return 1;
}

// original 0x590E30 (PSX 0x80165EE4): the delta as s16. Up: 0 at 999, else
// add and, past 999 (unsigned), store 999 and answer 999 - old. Down: 0 at 0,
// else add and, below 0 (s16), store 0 and answer -old. 0 otherwise - an
// unclamped change answers 0, as on the PSX. Only the low word of the answer
// is defined in the original (ax), so a short.
extern "C" short __cdecl Stat_AddClamped(unsigned short* stat, unsigned delta) {
    auto* const p = reinterpret_cast<unsigned char*>(stat);
    const auto d = static_cast<std::int16_t>(delta);
    const std::uint16_t v = item_use::Word(p);
    if (d > 0) {
        if (v == 999) return 0;
        const auto sum = static_cast<std::uint16_t>(v + static_cast<std::uint16_t>(d));
        item_use::SetWord(p, sum);
        if (sum <= 999) return 0;
        item_use::SetWord(p, 999);
        return static_cast<short>(999 - v);
    }
    if (d == 0) return 0;
    if (v == 0) return 0;
    const auto sum = static_cast<std::uint16_t>(v + static_cast<std::uint16_t>(d));
    item_use::SetWord(p, sum);
    if (static_cast<std::int16_t>(sum) >= 0) return 0;
    item_use::SetWord(p, 0);
    return static_cast<short>(-static_cast<int>(v));
}

// original 0x590F60 (PSX 0x801660DC): clear the mask's bits from the status
// word if any is set, and say so. Only the mask's low word reaches the word.
extern "C" unsigned char __cdecl Char_ClearStatus(unsigned id, unsigned mask, unsigned battle) {
    auto* const r = CharRec(id, battle);
    const std::uint16_t status = r->status;
    if ((status & mask & 0xFFFF) == 0) return 0;
    rv::Store<unsigned>(r->status, status & ~mask);
    return 1;
}

// ===========================================================================
// The message box's two commits

// ===========================================================================
// The sixteen system choices (MsgBox_SystemChoice's table, ids 0x80..0x8F)

// ===========================================================================
// The pad auto-repeat

// ===========================================================================

void ItemUse_Inject() {
    if (bof3::WantsShadow("item_use")) item_use::SelfTest();
    BOF3_INJECT(Char_HealHp);
    BOF3_INJECT(Char_HealAp);
    BOF3_INJECT(Stat_AddCap999);
    BOF3_INJECT(Stat_AddCap99);
    BOF3_INJECT(Stat_AddClamped);
    BOF3_INJECT(Char_ClearStatus);
}
