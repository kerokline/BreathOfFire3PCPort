// Round twelve group BE5 (docs/takeover-queue-field-battle.md section 3,
// analysis/round12_cut.tsv's group column): the battle engine's band
// 0x44B240..0x451480, each function read to its last instruction with
// capstone (2026-09-29) and taken through the boss harness's engine frame
// (boss_harness.h, docs/boss_harness.md section 10). docs/battle_e5.md has
// every function one row each.
//
//   - the enemy AI's row helpers: the other rows' test EnemyAI_TurnCheck's
//     condition 0x26 uses, the done bit, the element test of conditions 0..8
//     and 0x21..0x23, a row applied (seven kinds through a jump table, then
//     the row's common tail and the enemy messages), its stat scaling and its byte
//     setter (the latter two not in the cut: the cut's 0x44B8D0 is a case of
//     the setter's switch), the enemy messages deduplicated (not in the cut);
//   - three Effect_Handlers slots: 22 the HP drain, 23 the AP drain (not in
//     the cut; also 0x44EB50's tail jump), 38 the quarter-chance attack;
//   - BattleForm_ApplyStats: the transformation's block 0x939EE0.. into each
//     party member flagged +0x134 bit 1;
//   - the Dragon command's run below 0x44FF00 (not ours): six dispatchers by
//     0x904AA4 and their 33 steps (two of them not in the cut), two helpers
//     that price a slot against the member's AP.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The six
// dispatchers abort past their tables where the original jumps through
// whatever follows (the owner's rule for an unchecked index, round9 doc
// section 6); EnemyAI_DedupMessages aborts where the original would write a
// ninth entry past its eight-dword stack buffer (onto its return address).
// Every call goes through the harness (BH_CALL / BH_AT), so the start-up fuzz
// can stand recorders in for the callees.
#include "game/battle_e5.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_e5_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_e5::at;
using U = std::uint32_t;
using S32BattleE5 = std::int32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
std::uint16_t W(U address) { return move_script::Word(move_script::At(address)); }
void SetW(U address, unsigned v) { move_script::SetWord(move_script::At(address), v); }
U L(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }
S32BattleE5 S16BattleE5(std::uint16_t v) { return static_cast<std::int16_t>(v); }
unsigned char* Ptr(U cell) { return move_script::At(L(cell)); }
unsigned char* PartyBattleE5(unsigned member) { return move_script::At(::bof3::Img(ObjTrio_addr) + member * battle_e5::at::kPartyStride); }

// The callees nobody owns yet, by address (battle_e5_callees.h).
using Void0 = void (__cdecl*)();

// ===========================================================================
// The enemy AI's row helpers
// ===========================================================================

}  // namespace

// ===========================================================================
// Effect_Handlers slots 22, 23 and 38
// ===========================================================================

namespace {

U Key(const unsigned char* p) { return Low32(p); }

}  // namespace

// ===========================================================================
// The transformation's stats
// ===========================================================================

// ===========================================================================
// The Dragon command's run (0x44FF00 by 0x904AA3 into 0x64ECCC; not ours)
// ===========================================================================

namespace {

unsigned char* Member() { return reinterpret_cast<unsigned char*>(BattleE4_MenuActor.Native()); }
void MarkMember() {
    auto* const member_rec = rv::As<records::PartyMember>(Member());
    member_rec->flags_134 = static_cast<S32BattleE5>(static_cast<U>(static_cast<std::int32_t>(member_rec->flags_134)) | 4u);
}

}  // namespace

// --- part 0 (0x904AA3 0): the load --------------------------------------------

// --- part 1: the opening ---------------------------------------------------------

// --- part 2: the menu --------------------------------------------------------------

// --- part 3: the first list (0x904608, window 18) --------------------------------------

// --- part 4: the gene grid ------------------------------------------------------------

// --- part 5: the second list (0x904620, window 19) ------------------------------------

// --- part 6: the store (a first-list slot into the second list) --------------------------

// original 0x451160 (0x64ED50[3]; PSX 0x800A3294): the overwrite prompt: line 0x62; the
// repeat keys of 0xA000; cancel, or confirm on answer 1: sound 0x106, a step
// back; confirm on 0: sound 0x104, the next step; else a repeat key flips the
// answer (0x904B74 ^= 1); the hand at 0xD5 + 0x22 * answer (the dword), 0x18.
//
// As the original has it: the hand reads the dword at 0x904B74 (0x4511F2)
// where every writer stores a word (0x451042, 0x4511EA, 0x431C70); the
// PlayStation (US 0x800A632C, JP 0x800A332C) reads the answer as a u16. The
// level-up notice's other reader (0x431C77) masks its dword with 0xFFFF, so
// this is the only one that can see the halfword 0x904B76.
//
// DIVERGENCE DIV-0162: 1 through PatchBytes("StoreAskWord") in
// BattleE5_Inject, after the self-test. The hand then reads the word, as the
// PlayStation; the two differ only when 0x904B76 is not 0, which no
// instruction of the exe names (D338).
namespace battle_e5 { std::uint8_t g_store_ask_word = 0; }

// ============================================================================

void BattleE5_Inject() {
    if (bof3::WantsShadow("battle_e5")) battle_e5::SelfTest();
    {
        // DIV-0162: after the self-test, which compared Capcom's dword read.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("StoreAskWord", Low32(&battle_e5::g_store_ask_word), &was, &is, 1);
        bof3::Log("DIV-0162    the dragon store prompt's hand reads its answer as the word every writer stores (on unless the line above says OFF)");
    }
}
