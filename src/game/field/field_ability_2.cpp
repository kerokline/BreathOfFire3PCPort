// Moved here by tools/move_funcs.py from field_s.cpp, rest_2d.cpp, rest_2e.cpp, rest_2f.cpp (docs/source-layout-round.md).
#include "game/field/field_ability_2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field/field_ability_2_callees.h"
#include "game/field/field_idle.h"
#include "game/field_s.h"
#include "game/field_s_callees.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2d.h"
#include "game/rest_2d_callees.h"
#include "game/rest_2e.h"
#include "game/rest_2e_callees.h"
#include "game/rest_2f.h"
#include "game/rest_2f_callees.h"
#include "game/scenario_harness.h"
#include "game/solo/solo.h"
#include "game/solo/solo_screens.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"
#include "input/sigma.h"
#include "os/env.h"

namespace {

namespace at = field_ability_2::at;

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using U = std::uint32_t;

unsigned char* At(U address) { return move_script::At(address); }

unsigned char& B(U address) { return *At(address); }

U W(U address) { return move_script::Word(At(address)); }

void SetW(U address, U v) { move_script::SetWord(At(address), v); }

U L(U address) { return static_cast<U>(move_script::Long(At(address))); }

int S8(U v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

U RecAddr(U n) { return ::bof3::Img(CharacterRecords_addr) + n * field_s::at::kRecordStride; }

// A member id's record index: Character_IndexById[id], unchecked.
U RecordOf(U id) { return Character_IndexById[id & 0xFF]; }

U Pressed() { return Input_Pressed; }

bool Confirmed(U pressed) { return (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed & 0xFFFF) != 0; }

bool Cancelled(U pressed) { return (static_cast<std::uint32_t>(Field_CancelButtons) & pressed & 0xFFFF) != 0; }

void Sound(U id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

U PartyCount() { return static_cast<U>(SH_CALL(Party_Count)(0)) & 0xFF; }

// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& BRest2d(U a) { return move_script::At(a)[0]; }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (its reader's
// reach, docs/rest_2d.md section 3); past it the original jumps through what
// follows the table, which no handler of the screen writes - ours aborts. The
// entry is called as the cell holds it: Capcom's address in the game (Inject's
// jmp to ours where it is ours), a recorder while the fuzz runs.
using Handler = void (__cdecl*)();

void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: step %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(move_script::At(table + 4 * index)))))();
}

// A record the original writes by an index it never bounds. Past the array it
// writes into the next block; no path in play gives such an index (section 5),
// so ours aborts there.
U RecordChecked(U base, U stride, unsigned count, unsigned index, const char* array, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: %s index %u is past its %u records; the original writes at 0x%X", who, array, index, count,
                    static_cast<unsigned>(::bof3::VaOf(base + index * stride)));
    return base + index * stride;
}

U Window(unsigned index, const char* who) {
    return RecordChecked(::bof3::Img(WindowRecords_addr), rest_2d::at::kWindowStride, rest_2d::at::kWindowCount, index, "WindowRecords", who);
}

// Inventory_IdLists' / Inventory_CountLists' list of a category: the original
// reads the five-pointer tables by the category byte unbounded; past them the
// cell is not a list - ours aborts.
U ListOf(U table, unsigned category, const char* who) {
    if (category >= 5)
        bof3::Fatal("%s: category %u is past Inventory_IdLists' five lists; the original reads 0x%X as one", who, category,
                    static_cast<unsigned>(table + 4 * category));
    return static_cast<U>(Long(move_script::At(table + 4 * category)));
}

using UC = unsigned char;

constexpr auto& AtRest2e = move_script::At;

UC& BRest2e(U address) { return *AtRest2e(address); }

U WRest2e(U address) { return move_script::Word(AtRest2e(address)); }

void SetWRest2e(U address, U v) { move_script::SetWord(AtRest2e(address), v); }

U LRest2e(U address) { return static_cast<U>(move_script::Long(AtRest2e(address))); }

int S8Rest2e(U v) { return static_cast<signed char>(static_cast<UC>(v)); }

// A .data table symbols.gen.h declares as a pointer: its address.
U AddrOf(const void* p) { return Low32(p); }

// kMode: now BattleE1_Mode (symbols.toml). the menu block: the screen (FieldMenu_States' index)
// kState: now FieldS_State (symbols.toml). its state
// kStep: now FieldS_Step (symbols.toml). its step
constexpr ::bof3::image::Va kTimer{0x929F04};         // the open / close countdown

// kMember: now BattleActorCopies_ItemActor (symbols.toml). s8: a party member (Items, Equipment, Ability)
// kTarget: now FieldS_Column (symbols.toml). s8: the ability's target member
// kAnswer: now FieldS_Answer (symbols.toml). s8: a yes / no answer (1 yes)
// kBackdrop: now Config_Background (symbols.toml). the Config backdrop kind (Menu_DrawBackdrop's)
constexpr ::bof3::image::Va kPressed{0x7E1BEC};       // Input_Pressed

// kConfirm: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons
constexpr ::bof3::image::Va kParty{0x904062};         // the party's member ids

// kRecords: now CharacterRecords (symbols.toml). CharacterRecords, 0xA4 each
constexpr U kRecordStride = 0xA4;

// The image's tables, read in place.
constexpr ::bof3::image::Va kIdLists{0x656B00};       // Inventory_IdLists, five pointers by category

constexpr ::bof3::image::Va kCountLists{0x656B14};    // Inventory_CountLists, five (the fifth 0)

constexpr ::bof3::image::Va kConsumables{0x656B28};   // NameTable_Consumables, 22 bytes a record

constexpr ::bof3::image::Va kConsumableFlags{0x656B38};  // ... +0x10, the flags byte

constexpr ::bof3::image::Va kWeaponA{0x657464};       // NameTable_Weapons + 28 id + 0x14

constexpr ::bof3::image::Va kWeaponB{0x657466};       // ... + 0x16

constexpr ::bof3::image::Va kArmourType{0x657D7A};    // NameTable_Armour + 26 id + 0x12

constexpr ::bof3::image::Va kArmourA{0x657D7B};       // ... + 0x13

constexpr ::bof3::image::Va kArmourB{0x657D7C};       // ... + 0x14

constexpr ::bof3::image::Va kAbilityHelp{0x65C4DE};   // NameTable_Abilities + 24 id + 6: the help word

constexpr ::bof3::image::Va kAbilityFlags{0x65C4D8};  // NameTable_Abilities + 24 id: the first parameter byte

constexpr ::bof3::image::Va kAbilityRecords{0x65C4C8};  // Ability_Records, 24 bytes an id (the name first)

// The per-member bytes the screens keep (FS's at::kEquipBytes block 0x939880..).
constexpr ::bof3::image::Va kEquipTops{0x939880};     // the equip item list's top by slot + 6 member

constexpr ::bof3::image::Va kItemCursors{0x939898};   // the Items list's cursor by category

constexpr ::bof3::image::Va kItemTops{0x9398B8};      // and its top

constexpr ::bof3::image::Va kAbilityCursors{0x9398C0};  // the ability list's cursor by type + 4 member

// kViewCursor: now Rest2e_AbilityViewCursor (symbols.toml). the ability view's cursor
// kViewTop: now Rest2e_AbilityViewTop (symbols.toml). and its top
constexpr ::bof3::image::Va kPreview{0x6BDFA8};       // the six previewed equipment bytes

void Backdrop() { SH_CALL(Menu_DrawBackdrop)(Config_Background); }

U AutoRepeat(U pressed) { return SH_CALL(Input_AutoRepeat)(pressed); }

UC PartyCountRest2e() { return static_cast<UC>(SH_CALL(Party_Count)(0)); }

// A party member's record index: Character_IndexById[the member's id].
UC RecordOfRest2e(U id) { return Character_IndexById[id & 0xFF]; }

UC PartyRecord(int member) { return RecordOfRest2e(BRest2e(::bof3::Img(kParty.v) + member)); }

unsigned char* Record(U index) { return AtRest2e(::bof3::Img(CharacterRecords_addr) + (index & 0xFF) * kRecordStride); }

// The low byte of 1 << n as the originals compute it (an 8-bit shl, or the low
// byte of a 32-bit one): the count is n's low five bits.
UC Bit8(U n) {
    n &= 31;
    return n < 8 ? static_cast<UC>(1u << n) : 0;
}

// The inventory's list of a category (Inventory_IdLists / CountLists read in
// place). Past the five categories the original reads the next table's words
// as pointers; category 4's count list is a null pointer it writes through.
unsigned char* IdList(U category, const char* who) {
    if (category > 4) bof3::Fatal("%s: category %u reads past Inventory_IdLists (the original reads on)", who, (unsigned)category);
    return AtRest2e(LRest2e(::bof3::Img(kIdLists.v) + 4 * category));
}

// The count list's address as the original reads it (0 for category 4).
U CountListAt(U category, const char* who) {
    if (category > 4) bof3::Fatal("%s: category %u reads past Inventory_CountLists (the original reads on)", who, (unsigned)category);
    return LRest2e(::bof3::Img(kCountLists.v) + 4 * category);
}

// Entry `index` of a count list at its access: category 4's null list aborts
// here, where the original faults - not where the list is taken.
unsigned char* CountAt(U list, U index, U category, const char* who) {
    if (list == 0) bof3::Fatal("%s: category %u's count list is a null pointer (the original faults)", who, (unsigned)category);
    return AtRest2e(list + index);
}

unsigned char* CountList(U category, const char* who) { return CountAt(CountListAt(category, who), 0, category, who); }

// A .data table's entry: past its count the original jumps through whatever
// follows; ours aborts.
U Entry(U table, unsigned count, U index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: index %u past its %u-entry table 0x%X (the original jumps through the next word)", who,
                    (unsigned)index, count, (unsigned)table);
    return LRest2e(table + 4 * index);
}

void Swap(unsigned char* a, unsigned char* b) { SH_CALL(FieldMenu_SwapBytes)(a, b); }

const unsigned char* AbilityList(U member, U type) { return SH_CALL(Char_AbilityList)(member & 0xFF, type & 0xFF, 0); }

UC AbilityUse(U user, U target, U ability) {
    return static_cast<UC>(SH_AT(UC (__cdecl*)(unsigned, unsigned, unsigned, unsigned), rest_2e::at::kAbilityUse.v)(user, target, ability, 0));
}

// The list scroll FieldItems_ArrangeMove, FieldItems_ViewList32 and
// FieldAbility_ViewBrowse share: 0x1000 up a row (the scroll cell 0xF0 above
// the page), 0x4000 down below `max` (0x10 past the page's nine rows), 4 a page
// up, 8 a page down to the last page's top `last` (from above `last - 9` the
// rest of the way). The cursor and top bytes as the code leaves them in cl / al.
struct ListScroll {
    U cursor, top, scroll;
    UC max, last;
    bool scroll_byte;   // FieldAbility_ViewBrowse stores a byte, the Items screens a word
};

void SetScroll(const ListScroll& s, U v) {
    if (s.scroll_byte)
        BRest2e(s.scroll) = static_cast<UC>(v);
    else
        SetWRest2e(s.scroll, v);
}

void Scroll(U repeat, const ListScroll& s, UC& cl, UC& al) {
    cl = BRest2e(s.cursor);
    if (repeat & 0x1000) {
        if (cl > 0) {
            --cl;
            BRest2e(s.cursor) = cl;
        }
        al = BRest2e(s.top);
        if (cl < al) SetScroll(s, 0xF0);
    } else if (repeat & 0x4000) {
        if (cl < s.max) {
            ++cl;
            BRest2e(s.cursor) = cl;
        }
        al = BRest2e(s.top);
        if (static_cast<int>(cl) >= static_cast<int>(al) + 9) SetScroll(s, 0x10);
    } else if (repeat & Sigma(4)) {
        al = BRest2e(s.top);
        if (al == 0) {
            cl = 0;
            BRest2e(s.cursor) = 0;
        } else if (al < 9) {
            cl = static_cast<UC>(cl - al);
            al = 0;
            BRest2e(s.cursor) = cl;
            BRest2e(s.top) = al;
        } else {
            cl = static_cast<UC>(cl - 9);
            al = static_cast<UC>(al - 9);
            BRest2e(s.cursor) = cl;
            BRest2e(s.top) = al;
        }
    } else {
        al = BRest2e(s.top);
        if (repeat & Sigma(8)) {
            if (al == s.last) {
                cl = s.max;
                BRest2e(s.cursor) = cl;
            } else if (al > static_cast<UC>(s.last - 9)) {
                cl = static_cast<UC>(cl + static_cast<UC>(s.last - al));
                al = s.last;
                BRest2e(s.cursor) = cl;
                BRest2e(s.top) = al;
            } else {
                cl = static_cast<UC>(cl + 9);
                al = static_cast<UC>(al + 9);
                BRest2e(s.cursor) = cl;
                BRest2e(s.top) = al;
            }
        }
    }
}

const ListScroll kItemScroll = {::bof3::Img(0x803340), ::bof3::Img(0x80333F), ::bof3::Img(0x803346), 0x7F, 0x77, false};

const ListScroll kShortScroll = {::bof3::Img(0x803340), ::bof3::Img(0x80333F), ::bof3::Img(0x803346), 0x1F, 0x17, false};

const ListScroll kViewScroll = {::bof3::Img(0x8033CF), ::bof3::Img(0x8033CE), ::bof3::Img(0x8033CC), 0x11, 9, true};

// A party member's cursor moved by 0x1000 / 0x4000 over `count` entries, s8,
// wrapping (FieldItems_UseOnMember, FieldAbility_PickTarget): the byte as cl.
UC MoveMemberS8(U cell, U repeat, UC count) {
    UC cl = BRest2e(cell);
    if (repeat & 0x1000) {
        --cl;
        BRest2e(cell) = cl;
        if (S8Rest2e(cl) < 0) {
            cl = static_cast<UC>(count - 1);
            BRest2e(cell) = cl;
        }
    } else if (repeat & 0x4000) {
        ++cl;
        BRest2e(cell) = cl;
        if (S8Rest2e(cl) > static_cast<int>(count) - 1) {
            cl = 0;
            BRest2e(cell) = 0;
        }
    }
    return cl;
}

// The four-entry cursor of FieldEquip_TopMenu and FieldAbility_TopMenu: 0x8000
// down then 0x2000 up (both tested), s8 wrapping through 0..3; cl as left.
UC TurnFour(U cell, U repeat) {
    UC cl = BRest2e(cell);
    if (repeat & 0x8000) {
        --cl;
        BRest2e(cell) = cl;
        if (S8Rest2e(cl) < 0) {
            cl = 3;
            BRest2e(cell) = cl;
        }
    }
    if (repeat & 0x2000) {
        ++cl;
        BRest2e(cell) = cl;
        if (S8Rest2e(cl) > 3) {
            cl = 0;
            BRest2e(cell) = cl;
        }
    }
    return cl;
}

// The ability list's type 0x8033AB turned by 0x8000 / 0x2000 (sound 0x101,
// 0x8033A9 0x32 / 0x31), through 0..3; whether it turned, and the byte as cl.
bool TurnType(U repeat, UC& cl) {
    if (repeat & 0x8000) {
        Sound(0x101);
        cl = static_cast<UC>(Window16_Page - 1);
        Window16_PageCue = 0x32;
        Window16_Page = cl;
        if (S8Rest2e(cl) < 0) {
            cl = 3;
            Window16_Page = cl;
        }
        return true;
    }
    if (repeat & 0x2000) {
        Sound(0x101);
        cl = static_cast<UC>(Window16_Page + 1);
        Window16_PageCue = 0x31;
        Window16_Page = cl;
        if (cl > 3) {
            cl = 0;
            Window16_Page = cl;
        }
        return true;
    }
    return false;
}

// The member cursor 0x8033AA moved by 0x1000 / 0x4000 over `count` with sound
// 0x100 first (FieldAbility_PickMember, FieldAbility_ArrangeMember).
void MoveAbilityMember(U repeat, UC count) {
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto m = static_cast<UC>(Window16_Owner - 1);
        Window16_Owner = m;
        if (S8Rest2e(m) < 0) Window16_Owner = static_cast<UC>(count - 1);
    } else if (repeat & 0x4000) {
        Sound(0x100);
        const auto m = static_cast<UC>(Window16_Owner + 1);
        Window16_Owner = m;
        if (static_cast<int>(m) > static_cast<int>(count) - 1) Window16_Owner = 0;
    }
}

// The bubble pass of the five fixed-list sorts: 127 passes over the list at
// `ids` (its counts 0x200 on), a pair (x, y) of non-empty entries exchanged
// when `before(x, y)` says y goes first.
template <typename Before> void BubbleFixed(U ids, Before before) {
    for (U bound = 0x7F; bound != 0; --bound)
        for (U i = 0; i < bound; ++i) {
            const UC x = BRest2e(ids + i);
            if (x == 0) continue;
            const UC y = BRest2e(ids + i + 1);
            if (y == 0) continue;
            if (!before(x, y)) continue;
            Swap(AtRest2e(ids + i), AtRest2e(ids + i + 1));
            Swap(AtRest2e(ids + i + 0x200), AtRest2e(ids + i + 0x201));
        }
}

constexpr ::bof3::image::Va kConsumableIds{0x904154};   // Inventory_IdLists[0]; the counts 0x904354

constexpr ::bof3::image::Va kWeaponIds{0x9041D4};       // [1]

constexpr ::bof3::image::Va kArmourIds{0x904254};       // [2]

}  // namespace

// DIVERGENCE DIV-0189: the two consumable sorts order each bucket. The
// original's NormalItem / CombatItem sorts are a partition: the items whose
// flag bit (bit 0 field-usable, bit 1 battle-usable) is set go first, and
// within each half the bubble sort, being stable, leaves the list in the
// order the player had it. Ours, once g_sort_by_id is armed (after the
// self-test, PatchBytes "ItemSortById"), sorts within each half by the item
// id ascending - the record's index in the consumables table, the EMI's
// order (Green Apple 1 .. Water Jug 91). Unarmed, the original's partition.
namespace rest_2e { unsigned char g_sort_by_id = 0; }

namespace {

// The pair (x, y) of non-empty entries exchanged: y's flag bit set and x's
// clear (the original's rule), or - armed - both alike and y's id the lower.
bool ConsumableGoesFirst(UC x, UC y, UC bit) {
    const bool fx = (BRest2e(::bof3::Img(kConsumableFlags.v) + 22u * x) & bit) != 0;
    const bool fy = (BRest2e(::bof3::Img(kConsumableFlags.v) + 22u * y) & bit) != 0;
    if (fx != fy) return fy;
    return rest_2e::g_sort_by_id && y < x;
}

using move_script::SetLong;

using rest_2f::kSwapBytes;

// Tactics' own cells (0x6BDFBC..0x6BDFD8).
constexpr ::bof3::image::Va kGrid{0x6BDFBC};        // 3 x 3 bytes: the formation's cells that may be picked

// kSetCol: now Rest2f_SetCol (symbols.toml). the formation set: column 1..2, row (0x904060 = col + 2 row - 1)
// kPickCol: now Rest2f_PickCol (symbols.toml). the cell picked: 0x7F none
// kSetRow: now Rest2f_SetRow (symbols.toml).
// kPickRow: now Rest2f_PickRow (symbols.toml).
// kCurCol: now Rest2f_CursorCol (symbols.toml). the grid cursor
// kCurRow: now Rest2f_CursorRow (symbols.toml).
// kTopChoice: now Rest2f_TopChoice (symbols.toml). the screen's top: 0 formation, 1 members
constexpr ::bof3::image::Va kMemberFlags{0x6BDFCC}; // two bytes a row: party, reserve

// kSwappedA: now Rest2f_SwappedA (symbols.toml). the two rows the last formation swap moved
// kSwappedB: now Rest2f_SwappedB (symbols.toml).
constexpr ::bof3::image::Va kReserve{0x6BDFD4};     // the reserve's member ids

constexpr ::bof3::image::Va kPartyCopy{0x904065};

// kCharacters: now CharacterRecords (symbols.toml). CharacterRecords: 0xA4 a member
constexpr U kCharacterStride = 0xA4;

constexpr ::bof3::image::Va kEffectState{0x66972C}; // Character_IndexById: the record of a member id

// kStyle: now Config_WindowColour (symbols.toml). the window colour
// kBackdrop: now Config_Background (symbols.toml). the Config "Background" byte
// kGameStep: now Game_Step (symbols.toml). Game_Step (u16)
// kPressed: now Input_Pressed (symbols.toml). Input_Pressed (u16)
// kConfirm: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons (u16)
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons (u16)
// kInputFlags: now Field_InputFlags (symbols.toml). Field_InputFlags: bit 0, the members screen allowed
// kCurrent: now BattleWinStates_Current (symbols.toml). the window record Field_RunTaskRecords runs
// kWindows: now WindowRecords (symbols.toml). WindowRecords
constexpr U kWindowStride = 0x24;

// The members screen's portrait cells: 0x9398C0 + 4 row, 0x939880 / 0x9398A0 + 6 row.
constexpr ::bof3::image::Va kPortrait4{0x9398C0};

constexpr ::bof3::image::Va kPortraitA{0x939880};

constexpr ::bof3::image::Va kPortraitB{0x9398A0};

// Image tables, read in place.
constexpr ::bof3::image::Va kFormCells{0x667450};   // 3 x 3 of 8 bytes: x, y words, a byte (the formation's cursor places)

constexpr ::bof3::image::Va kMemberCells{0x6674C8}; // 2 a row of 8 bytes: the members screen's cursor places

constexpr ::bof3::image::Va kApCost{0x65C4DA};      // Ability_Records + 0x12: the AP cost byte, 24 a record

unsigned char& BRest2f(U address) { return move_script::At(address)[0]; }

U WRest2f(U address) { return move_script::Word(move_script::At(address)); }

void SetWRest2f(U address, U v) { SetWord(move_script::At(address), v); }

U Key(const void* p) { return Low32(p); }

// Window record n's byte / word at `at`.
U Rec(unsigned n, unsigned at) { return ::bof3::Img(WindowRecords_addr) + kWindowStride * n + at; }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void Run(const char* who, const unsigned long* table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_2f.md section 7)",
                    who, byte, index, entries, (unsigned)Key(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

void SoundRest2f(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

unsigned char PartyCountRest2f() { return static_cast<unsigned char>(SH_CALL(Party_Count)(0)); }

U Repeat(U pressed) { return SH_CALL(Input_AutoRepeat)(pressed); }

void SwapRest2f(unsigned char* a, unsigned char* b) { SH_AT(void (__cdecl*)(unsigned char*, unsigned char*), kSwapBytes.v)(a, b); }

// The member a CharacterRecords flag test is about: record Character_IndexById[id].
unsigned char* Character(unsigned char id) { return move_script::At(::bof3::Img(CharacterRecords_addr) + kCharacterStride * BRest2f(::bof3::Img(kEffectState.v) + id)); }

// The three sorts' shared shape: nine passes; pass p (9 - p swaps a pass, a
// bubble) fetches the list of window record 16's member +0xA and type +0xB
// (Char_AbilityList, battle 0) afresh and walks it, `test(a, b)` deciding the
// swap of each neighbouring pair through 0x58BD50. The bytes are read again
// after every swap.
template <typename Test> void Bubble(Test swap_if) {
    U span = 9;
    for (unsigned pass = 9; pass != 0; --pass) {
        unsigned char* p = SH_CALL(Char_AbilityList)(BRest2f(Rec(16, 0xA)), BRest2f(Rec(16, 0xB)), 0);
        for (unsigned char i = 0; i < span; ++i, ++p)
            if (swap_if(p)) SwapRest2f(p, p + 1);
        --span;
    }
}

// The party's window records 4.. : `body(i)` for i below Party_Count(0), the
// count asked again after each (the shape of six of the Tactics steps).
template <typename Body> void ForParty(Body body) {
    unsigned char i = 0;
    if (PartyCountRest2f() == 0) return;
    do {
        const unsigned char at = i;
        ++i;
        body(at);
    } while (i < PartyCountRest2f());
}

// The countdown: one less; true when it reached 0.
bool CountedDown() {
    const unsigned char t = static_cast<unsigned char>(BRest2f(::bof3::Img(kTimer.v)) - 1);
    BRest2f(::bof3::Img(kTimer.v)) = t;
    return t == 0;
}

}  // namespace

// original 0x58AAE0 (0xE bytes), FieldMenu_States[2]: jmp [FieldMenuItems_States
// + 4 * the menu state], unchecked.
extern "C" void __cdecl FieldMenuItems_ByState(void) {
    Jump(::bof3::Img(FieldMenuItems_States_addr), rest_2d::at::kItemsStateCount, FieldS_State, "FieldMenuItems_ByState");
}

// original 0x58AAF0 (0x38 bytes), state 0: Menu_DrawBackdrop, the windows placed
// (R2E's 0x58BC30), Sound_PlayEffect(0x102); the state on, the category 0,
// the timer 5.
extern "C" void __cdecl FieldMenuItems_Open(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    SH_AT(void (__cdecl*)(), rest_2d::at::kItemsWindows.v)();
    SH_CALL(Sound_PlayEffect)(0x102);
    const unsigned char state = FieldS_State;
    Rest2d_ItemsCategory = 0;
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = 5;
    FieldS_State = static_cast<unsigned char>(state + 1);
}

// original 0x58AB30 (0x30 bytes), state 1: Menu_DrawBackdrop; the timer down, at
// 0 the timer 0 and the state on.
extern "C" void __cdecl FieldMenuItems_SlideIn(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const auto t = static_cast<unsigned char>(BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) - 1);
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = t;
    if (t != 0) return;
    const unsigned char state = FieldS_State;
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = 0;
    FieldS_State = static_cast<unsigned char>(state + 1);
}

// original 0x58AB60 (0x1C8 bytes), state 2: the category row. Menu_DrawBackdrop;
// window 13's +8 0, window 11's +0x10 the category + 0x13, window 13's +0xC
// 0xFF, the hand (window 21) on at window 12's (x + 48 * category, y + 4),
// window 12's +0xB 0xFF. Input_AutoRepeat(Input_Pressed & 0xA000): 0x8000 the
// category back (below 0 to 3), 0x2000 on (past 3 to 0), signed; a change
// 0x101. Confirm - window 12's +0xB the category, 0x104; category 1: the step
// 0, state + 3; category 3: window 13's pick and top from 0x93989C / 0x9398BC,
// its +0xA 4, state + 7; else window 13's +0xD 0xFF, its top and pick from
// 0x9398B8 / 0x939898 by its own +0xA, state + 1. Cancel - 0x106, 0x102,
// windows 11, 12, 13 state 1, the hand off, the timer + 5, state + 2.
extern "C" void __cdecl FieldMenuItems_Category(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const auto category = Rest2d_ItemsCategory;
    BRest2d(::bof3::Img(0x80333C)) = 0;
    const U window_x = static_cast<U>(Long(move_script::At(::bof3::Img(0x803314))));
    SetWord(move_script::At(::bof3::Img(0x8032FC)), static_cast<unsigned>(category + 0x13));
    const unsigned pressed = Input_Pressed;
    BRest2d(::bof3::Img(0x803340)) = 0xFF;
    const unsigned short y = static_cast<unsigned short>(Word(move_script::At(::bof3::Img(0x803316))) + 4);
    WindowRecords[21].in_use = 1;
    SetWord(move_script::At(::bof3::Img(0x803458)), static_cast<unsigned>(48 * category) + window_x);
    WindowRecords[21].y = y;
    BRest2d(::bof3::Img(0x80331B)) = 0xFF;
    const U held = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
    unsigned char c = static_cast<unsigned char>(Rest2d_ItemsCategory);
    const unsigned char was = c;
    if (held & 0x8000) {
        c = static_cast<unsigned char>(c - 1);
        Rest2d_ItemsCategory = c;
        if (static_cast<signed char>(c) < 0) {
            c = 3;
            Rest2d_ItemsCategory = 3;
        }
    }
    if (held & 0x2000) {
        c = static_cast<unsigned char>(c + 1);
        Rest2d_ItemsCategory = c;
        if (static_cast<signed char>(c) > 3) {
            c = 0;
            Rest2d_ItemsCategory = 0;
        }
    }
    if (was != c) {
        SH_CALL(Sound_PlayEffect)(0x101);
        c = static_cast<unsigned char>(Rest2d_ItemsCategory);
    }
    if (Field_ConfirmButtons & Input_Pressed) {
        BRest2d(::bof3::Img(0x80331B)) = c;
        SH_CALL(Sound_PlayEffect)(0x104);
        const unsigned char chosen = static_cast<unsigned char>(Rest2d_ItemsCategory);
        if (chosen == 1) {
            const unsigned char state = FieldS_State;
            FieldS_Step = 0;
            FieldS_State = static_cast<unsigned char>(state + 3);
            return;
        }
        if (chosen == 3) {
            const unsigned char pick = BRest2d(::bof3::Img(0x93989C));
            const unsigned char top = BRest2d(::bof3::Img(0x9398BC));
            BRest2d(::bof3::Img(0x803340)) = pick;
            const unsigned char state = FieldS_State;
            BRest2d(::bof3::Img(0x80333E)) = 4;
            BRest2d(::bof3::Img(0x80333F)) = top;
            FieldS_State = static_cast<unsigned char>(state + 7);
            return;
        }
        const unsigned char list = BRest2d(::bof3::Img(0x80333E));
        BRest2d(::bof3::Img(0x803341)) = 0xFF;
        const unsigned char top = BRest2d(::bof3::Img(rest_2d::at::kItemsTops.v) + list);
        const unsigned char state = FieldS_State;
        BRest2d(::bof3::Img(0x80333F)) = top;
        const unsigned char pick = BRest2d(::bof3::Img(rest_2d::at::kItemsPicks.v) + list);
        FieldS_State = static_cast<unsigned char>(state + 1);
        BRest2d(::bof3::Img(0x803340)) = pick;
        return;
    }
    if (Field_CancelButtons & Input_Pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(Sound_PlayEffect)(0x102);
        const unsigned char t = BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v));
        const unsigned char state = FieldS_State;
        BRest2d(::bof3::Img(0x8032EF)) = 1;
        BRest2d(::bof3::Img(0x803313)) = 1;
        BRest2d(::bof3::Img(0x803337)) = 1;
        WindowRecords[21].in_use = 0;
        BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = static_cast<unsigned char>(t + 5);
        FieldS_State = static_cast<unsigned char>(state + 2);
    }
}

// original 0x58AD30 (0x3F5 bytes), state 3: the item list of window 13's
// category +0xA. Menu_DrawBackdrop; window 13's +8 = 3 with the category row's
// cursor not 0, else 1; window 11's +0x10 = Item_HelpMessage(category, the
// picked item); the hand (window 21) on at (window 13's x + 7, its y + 13 *
// (pick - top + 2)). Input_AutoRepeat(Input_Pressed & 0xF00C): 0x8000 / 0x2000
// another category (window 13's +0x10 0x32 / 0x31; the old one's pick and top
// kept in 0x939898 / 0x9398B8, the new one's taken; 0..3 wrapping); else 0x1000
// / 0x4000 the pick up / down (0..0x7F; leaving the nine rows shown sets window
// 13's +0x12 to 0xF0 / 0x10, the scroll), 4 / 8 a page up / down (the top 0..0x77
// by nine); the pick and top kept; a moved pick 0x100. While the scroll word is
// 0: confirm on an empty slot 0x107; else 0x103 - with the category row's cursor
// not 0, Item_CanUse(window 13's +8, 0, category, item) refused 0x107, taken:
// +0xD the pick, the answer 0, state + 3; with it 0, the same test, then an item
// whose flags have bit 4 used at once through ItemUse_Dispatch(0, item, 0) -
// answers 0 or 4: 0x104 and one fewer (the slot emptied at 0), any other 0x107
// - and one without: the cursor 0, +0xD the pick, the hand off, state + 4.
// Cancel - 0x106, the state back.
extern "C" void __cdecl FieldMenuItems_List(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const unsigned char cursor_row = static_cast<unsigned char>(Rest2d_ItemsCategory);
    const unsigned char category = BRest2d(::bof3::Img(0x80333E));
    const U ids = ListOf(::bof3::Img(rest_2d::at::kItemIdLists.v), category, "FieldMenuItems_List");
    BRest2d(::bof3::Img(0x80333C)) = cursor_row != 0 ? 3 : 1;
    const unsigned char item = BRest2d(ids + BRest2d(::bof3::Img(0x803340)));
    const U help = SH_CALL(Item_HelpMessage)(category, item);
    const unsigned char top = BRest2d(::bof3::Img(0x80333F));
    const U window_x = static_cast<U>(Long(move_script::At(::bof3::Img(0x803338))));
    SetWord(move_script::At(::bof3::Img(0x8032FC)), help);
    const int row = static_cast<int>(BRest2d(::bof3::Img(0x803340))) - static_cast<int>(top) + 2;
    const unsigned pressed = Input_Pressed;
    SetWord(move_script::At(::bof3::Img(0x803458)), window_x + 7);
    WindowRecords[21].in_use = 1;
    WindowRecords[21].y = static_cast<unsigned>(row * 13) + Word(move_script::At(::bof3::Img(0x80333A)));
    const U held = SH_CALL(Input_AutoRepeat)(pressed & Sigma(0xF00C));
    if (held & 0x8000) {
        SH_CALL(Sound_PlayEffect)(0x101);
        const unsigned char old = BRest2d(::bof3::Img(0x80333E));
        const unsigned char pick = BRest2d(::bof3::Img(0x803340));
        auto next = static_cast<unsigned char>(old - 1);
        SetWord(move_script::At(::bof3::Img(0x803344)), 0x32);
        BRest2d(::bof3::Img(0x80333E)) = next;
        BRest2d(::bof3::Img(rest_2d::at::kItemsPicks.v) + old) = pick;
        BRest2d(::bof3::Img(rest_2d::at::kItemsTops.v) + old) = BRest2d(::bof3::Img(0x80333F));
        if (static_cast<signed char>(next) < 0) {
            next = 3;
            BRest2d(::bof3::Img(0x80333E)) = 3;
        }
        BRest2d(::bof3::Img(0x803340)) = BRest2d(::bof3::Img(rest_2d::at::kItemsPicks.v) + next);
        BRest2d(::bof3::Img(0x80333F)) = BRest2d(::bof3::Img(rest_2d::at::kItemsTops.v) + next);
        return;
    }
    if (held & 0x2000) {
        SetWord(move_script::At(::bof3::Img(0x803344)), 0x31);
        SH_CALL(Sound_PlayEffect)(0x101);
        const unsigned char old = BRest2d(::bof3::Img(0x80333E));
        const unsigned char pick = BRest2d(::bof3::Img(0x803340));
        auto next = static_cast<unsigned char>(old + 1);
        BRest2d(::bof3::Img(0x80333E)) = next;
        BRest2d(::bof3::Img(rest_2d::at::kItemsPicks.v) + old) = pick;
        BRest2d(::bof3::Img(rest_2d::at::kItemsTops.v) + old) = BRest2d(::bof3::Img(0x80333F));
        if (next > 3) {
            next = 0;
            BRest2d(::bof3::Img(0x80333E)) = 0;
        }
        BRest2d(::bof3::Img(0x803340)) = BRest2d(::bof3::Img(rest_2d::at::kItemsPicks.v) + next);
        BRest2d(::bof3::Img(0x80333F)) = BRest2d(::bof3::Img(rest_2d::at::kItemsTops.v) + next);
        return;
    }
    unsigned char pick = BRest2d(::bof3::Img(0x803340));
    const unsigned char was = pick;
    unsigned char first = BRest2d(::bof3::Img(0x80333F));
    if (held & 0x1000) {
        if (pick != 0) {
            pick = static_cast<unsigned char>(pick - 1);
            BRest2d(::bof3::Img(0x803340)) = pick;
        }
        first = BRest2d(::bof3::Img(0x80333F));
        if (pick < first) SetWord(move_script::At(::bof3::Img(0x803346)), 0xF0);
    } else if (held & 0x4000) {
        if (pick < 0x7F) {
            pick = static_cast<unsigned char>(pick + 1);
            BRest2d(::bof3::Img(0x803340)) = pick;
        }
        first = BRest2d(::bof3::Img(0x80333F));
        if (static_cast<int>(BRest2d(::bof3::Img(0x803340))) >= static_cast<int>(first) + 9) SetWord(move_script::At(::bof3::Img(0x803346)), 0x10);
    } else if (held & Sigma(4)) {
        first = BRest2d(::bof3::Img(0x80333F));
        if (first == 0) {
            pick = 0;
            BRest2d(::bof3::Img(0x803340)) = 0;
        } else {
            if (first < 9) {
                pick = static_cast<unsigned char>(pick - first);
                first = 0;
            } else {
                pick = static_cast<unsigned char>(pick - 9);
                first = static_cast<unsigned char>(first - 9);
            }
            BRest2d(::bof3::Img(0x803340)) = pick;
            BRest2d(::bof3::Img(0x80333F)) = first;
        }
    } else {
        first = BRest2d(::bof3::Img(0x80333F));
        if (held & Sigma(8)) {
            if (first == 0x77) {
                pick = 0x7F;
                BRest2d(::bof3::Img(0x803340)) = 0x7F;
            } else {
                if (first > 0x6E) {
                    pick = static_cast<unsigned char>(pick + (0x77 - first));
                    first = 0x77;
                } else {
                    pick = static_cast<unsigned char>(pick + 9);
                    first = static_cast<unsigned char>(first + 9);
                }
                BRest2d(::bof3::Img(0x803340)) = pick;
                BRest2d(::bof3::Img(0x80333F)) = first;
            }
        }
    }
    const unsigned char list = BRest2d(::bof3::Img(0x80333E));
    BRest2d(::bof3::Img(rest_2d::at::kItemsTops.v) + list) = first;
    BRest2d(::bof3::Img(rest_2d::at::kItemsPicks.v) + list) = pick;
    if (was != pick) SH_CALL(Sound_PlayEffect)(0x100);
    const unsigned slot = BRest2d(::bof3::Img(0x803340));
    const unsigned char now = BRest2d(::bof3::Img(0x80333E));
    const U id_at = ListOf(::bof3::Img(rest_2d::at::kItemIdLists.v), now, "FieldMenuItems_List") + slot;
    const U count_list = ListOf(::bof3::Img(rest_2d::at::kItemCountLists.v), now, "FieldMenuItems_List");
    if (Word(move_script::At(::bof3::Img(0x803346))) != 0) return;
    if (Field_ConfirmButtons & Input_Pressed) {
        if (BRest2d(id_at) == 0) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x103);
        if (static_cast<unsigned char>(Rest2d_ItemsCategory) != 0) {
            const unsigned char cat = BRest2d(::bof3::Img(0x80333E));
            const unsigned char it = BRest2d(ListOf(::bof3::Img(rest_2d::at::kItemIdLists.v), cat, "FieldMenuItems_List") + BRest2d(::bof3::Img(0x803340)));
            if (SH_CALL(Item_CanUse)(BRest2d(::bof3::Img(0x80333C)), 0, cat, it) == 0) {
                SH_CALL(Sound_PlayEffect)(0x107);
                return;
            }
            const unsigned char p = BRest2d(::bof3::Img(0x803340));
            BRest2d(::bof3::Img(0x803341)) = p;
            const unsigned char state = FieldS_State;
            FieldS_Answer = 0;
            FieldS_State = static_cast<unsigned char>(state + 3);
            return;
        }
        if (SH_CALL(Item_CanUse)(BRest2d(::bof3::Img(0x80333C)), 0, BRest2d(::bof3::Img(0x80333E)), BRest2d(id_at)) == 0) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        const unsigned char it = BRest2d(id_at);
        if ((BRest2d(::bof3::Img(rest_2d::at::kItemFlags.v) + 22u * it) & 0x10) == 0) {
            const unsigned char state = FieldS_State;
            const unsigned char p = BRest2d(::bof3::Img(0x803340));
            BattleActorCopies_ItemActor = 0;
            BRest2d(::bof3::Img(0x803341)) = p;
            WindowRecords[21].in_use = 0;
            FieldS_State = static_cast<unsigned char>(state + 4);
            return;
        }
        const unsigned char used = SH_CALL(ItemUse_Dispatch)(0, it, 0);
        if (used != 0 && used != 4) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x104);
        if (count_list == 0)
            bof3::Fatal("FieldMenuItems_List: category %u has no count list (Inventory_CountLists' fifth is 0); the original "
                        "writes at 0x%X",
                        static_cast<unsigned>(now), slot);
        const auto left = static_cast<unsigned char>(BRest2d(count_list + slot) - 1);
        BRest2d(count_list + slot) = left;
        if (left == 0) BRest2d(id_at) = 0;
        return;
    }
    if (Field_CancelButtons & Input_Pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        FieldS_State = static_cast<unsigned char>(FieldS_State - 1);
    }
}

// original 0x58B130 (0x8D bytes), state 4: Menu_DrawBackdrop; the timer down, at
// 0 the windows taken down (R2E's 0x58C2A0), windows 4 + i state 3 for each
// member, the timer 0, windows 7..10 state 2, back to the top bar.
extern "C" void __cdecl FieldMenuItems_Close(void) {
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const auto t = static_cast<unsigned char>(BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) - 1);
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = t;
    if (t != 0) return;
    SH_AT(void (__cdecl*)(), rest_2d::at::kItemsReset.v)();
    for (unsigned char i = 0; i < static_cast<unsigned char>(SH_CALL(Party_Count)(0));) {
        const unsigned char k = i;
        ++i;
        BRest2d(Window(4 + k, "FieldMenuItems_Close") + 3) = 3;
    }
    BRest2d(::bof3::Img(rest_2d::at::kMenuTimer.v)) = 0;
    BRest2d(::bof3::Img(0x80325F)) = 2;
    BRest2d(::bof3::Img(0x803283)) = 2;
    BRest2d(::bof3::Img(0x8032A7)) = 2;
    BRest2d(::bof3::Img(0x8032CB)) = 2;
    BattleE1_Mode = 1;
    FieldS_State = 0;
}

// original 0x58B1C0 (0xE bytes), FieldMenuItems_States[5]: jmp
// [FieldMenuItems_State5Steps + 4 * the menu step], unchecked.
extern "C" void __cdecl FieldMenuItems_State5ByStep(void) {
    Jump(::bof3::Img(FieldMenuItems_State5Steps_addr), rest_2d::at::kItemsState5StepCount, FieldS_Step, "FieldMenuItems_State5ByStep");
}

// original 0x58B1D0: FieldMenuItems_State5Steps[0] - the category.
extern "C" void __cdecl FieldItems_ArrangeCategory(void) {
    Backdrop();
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0xF00C);
    const U x = LRest2e(::bof3::Img(0x803338)) + 0x2A;
    const U y = WRest2e(::bof3::Img(0x80333A)) + 7;
    SetWRest2e(::bof3::Img(0x8032FC), 0x17);
    WindowRecords[21].in_use = 1;
    SetWRest2e(::bof3::Img(0x803458), x);
    SetWRest2e(::bof3::Img(0x80345A), y);
    const U repeat = AutoRepeat(pressed);
    BRest2e(::bof3::Img(0x803340)) = 0xFF;
    bool turned = false;
    U category = 0;
    if (repeat & 0x8000) {
        Sound(0x101);
        const auto c = static_cast<UC>(BRest2e(::bof3::Img(0x80333E)) - 1);
        SetWRest2e(::bof3::Img(0x803344), 0x32);
        BRest2e(::bof3::Img(0x80333E)) = c;
        if (S8Rest2e(c) < 0) BRest2e(::bof3::Img(0x80333E)) = 3;
        category = BRest2e(::bof3::Img(0x80333E));
        turned = true;
    } else if (repeat & 0x2000) {
        Sound(0x101);
        auto c = static_cast<UC>(BRest2e(::bof3::Img(0x80333E)) + 1);
        SetWRest2e(::bof3::Img(0x803344), 0x31);
        BRest2e(::bof3::Img(0x80333E)) = c;
        if (c > 3) {
            c = 0;
            BRest2e(::bof3::Img(0x80333E)) = c;
        }
        category = c;
        turned = true;
    }
    if (turned) BRest2e(::bof3::Img(0x80333F)) = BRest2e(::bof3::Img(kItemTops.v) + category);
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        Sound(0x103);
        BRest2e(::bof3::Img(0x803362)) = BRest2e(::bof3::Img(0x80333E));
        const auto step = static_cast<UC>(FieldS_Step + 1);
        BRest2e(::bof3::Img(0x803359)) = 6;
        BRest2e(::bof3::Img(0x80335A)) = 0xF;
        BRest2e(::bof3::Img(0x803358)) = 1;
        SetWRest2e(::bof3::Img(0x80335C), 0xB4);
        SetWRest2e(::bof3::Img(0x80335E), 0x3C);
        BRest2e(::bof3::Img(0x803363)) = 0;
        BRest2e(::bof3::Img(0x803364)) = 0xFF;
        FieldS_Step = step;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    FieldS_State = static_cast<UC>(FieldS_State - 3);
}

// original 0x58B330: FieldMenuItems_State5Steps[1] - the sort, a row of the
// category's FieldItems_ArrangeRows.
extern "C" void __cdecl FieldItems_ArrangeHow(void) {
    Backdrop();
    const U row = BRest2e(::bof3::Img(0x803363));
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    const U x = LRest2e(::bof3::Img(0x80335C)) + 5;
    const U y = (row << 4) + WRest2e(::bof3::Img(0x80335E)) + 0x1A;
    SetWRest2e(::bof3::Img(0x8032FC), 0x18);
    WindowRecords[21].in_use = 1;
    SetWRest2e(::bof3::Img(0x803458), x);
    SetWRest2e(::bof3::Img(0x80345A), y);
    BRest2e(::bof3::Img(0x803340)) = 0xFF;
    BRest2e(::bof3::Img(0x803364)) = 0xFF;
    const U repeat = AutoRepeat(pressed);
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto c = static_cast<UC>(BRest2e(::bof3::Img(0x803363)) - 1);
        BRest2e(::bof3::Img(0x803363)) = c;
        if (S8Rest2e(c) < 0) BRest2e(::bof3::Img(0x803363)) = static_cast<UC>(BRest2e(AddrOf(FieldItems_ArrangeRows) + 6u * BRest2e(::bof3::Img(0x803362))) - 1);
    } else if (repeat & 0x4000) {
        Sound(0x100);
        const auto c = static_cast<UC>(BRest2e(::bof3::Img(0x803363)) + 1);
        const int last = static_cast<int>(BRest2e(AddrOf(FieldItems_ArrangeRows) + 6u * BRest2e(::bof3::Img(0x803362)))) - 1;
        BRest2e(::bof3::Img(0x803363)) = c;
        if (static_cast<int>(c) > last) BRest2e(::bof3::Img(0x803363)) = 0;
    }
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        Sound(0x103);
        const UC how = BRest2e(AddrOf(FieldItems_ArrangeRows) + 1 + 6u * BRest2e(::bof3::Img(0x803362)) + BRest2e(::bof3::Img(0x803363)));
        if (how != 0) {
            SH_CALL(FieldItems_Sort)(how);
            return;
        }
        BRest2e(::bof3::Img(0x803341)) = 0xFF;
        const UC cursor = BRest2e(::bof3::Img(kItemCursors.v) + BRest2e(::bof3::Img(0x80333E)));
        const auto step = static_cast<UC>(FieldS_Step + 1);
        BRest2e(::bof3::Img(0x803340)) = cursor;
        FieldS_Step = step;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto step = static_cast<UC>(FieldS_Step - 1);
    BRest2e(::bof3::Img(0x803358)) = 0;
    FieldS_Step = step;
}

// original 0x58B4B0: FieldMenuItems_State5Steps[2] - an entry picked and swapped
// with another, the list scrolled.
extern "C" void __cdecl FieldItems_ArrangeMove(void) {
    Backdrop();
    const U category = BRest2e(::bof3::Img(0x80333E));
    const UC item = IdList(category, "FieldItems_ArrangeMove")[BRest2e(::bof3::Img(0x803340))];
    const U help = SH_CALL(Item_HelpMessage)(category, item);
    const U top = BRest2e(::bof3::Img(0x80333F));
    SetWRest2e(::bof3::Img(0x8032FC), help);
    const U x = LRest2e(::bof3::Img(0x803338)) + 7;
    BRest2e(::bof3::Img(0x803364)) = 0;
    SetWRest2e(::bof3::Img(0x803458), x);
    WindowRecords[21].in_use = 1;
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0x500C);
    WindowRecords[21].y = 13u * (BRest2e(::bof3::Img(0x803340)) - top + 2) + WRest2e(::bof3::Img(0x80333A));
    const U repeat = AutoRepeat(pressed);
    const UC was = BRest2e(::bof3::Img(0x803340));
    UC cl, al;
    Scroll(repeat, kItemScroll, cl, al);
    const U now = BRest2e(::bof3::Img(0x80333E));
    BRest2e(::bof3::Img(kItemCursors.v) + now) = cl;
    BRest2e(::bof3::Img(kItemTops.v) + now) = al;
    if (was != cl) Sound(0x100);
    if (WRest2e(::bof3::Img(0x803346)) != 0) return;
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        if (BRest2e(::bof3::Img(0x803341)) == 0xFF) {
            Sound(0x103);
            if (IdList(BRest2e(::bof3::Img(0x80333E)), "FieldItems_ArrangeMove")[BRest2e(::bof3::Img(0x803340))] != 0) {
                BRest2e(::bof3::Img(0x803341)) = BRest2e(::bof3::Img(0x803340));
                return;
            }
            Sound(0x107);
            return;
        }
        Sound(0x103);
        {
            unsigned char* const ids = IdList(BRest2e(::bof3::Img(0x80333E)), "FieldItems_ArrangeMove");
            const U cursor = BRest2e(::bof3::Img(0x803340));
            Swap(ids + BRest2e(::bof3::Img(0x803341)), ids + cursor);
        }
        {
            const U cursor = BRest2e(::bof3::Img(0x803340));
            unsigned char* const counts = CountList(BRest2e(::bof3::Img(0x80333E)), "FieldItems_ArrangeMove");
            Swap(counts + BRest2e(::bof3::Img(0x803341)), counts + cursor);
        }
        BRest2e(::bof3::Img(0x803341)) = 0xFF;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    if (BRest2e(::bof3::Img(0x803341)) != 0xFF) {
        BRest2e(::bof3::Img(0x803341)) = 0xFF;
        return;
    }
    FieldS_Step = static_cast<UC>(FieldS_Step - 1);
}

// original 0x58B750: the Items screen's state 6 - the yes / no to discard.
extern "C" void __cdecl FieldItems_DiscardConfirm(void) {
    Backdrop();
    const U category = BRest2e(::bof3::Img(0x80333E));
    const UC item = IdList(category, "FieldItems_DiscardConfirm")[BRest2e(::bof3::Img(0x803340))];
    const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
    SH_CALL(TextRecord_Set)(0, 0x10, name);
    const int answer = FieldS_Answer;
    const U x = LRest2e(::bof3::Img(0x8032F0)) - static_cast<U>(36 * answer) + 0xE8;
    const U y = WRest2e(::bof3::Img(0x8032F2)) + 5;
    U pressed = LRest2e(::bof3::Img(kPressed.v));
    SetWRest2e(::bof3::Img(0x8032FC), 0x1A);
    WindowRecords[21].in_use = 1;
    SetWRest2e(::bof3::Img(0x803458), x);
    SetWRest2e(::bof3::Img(0x80345A), y);
    if (pressed & 0xA000) {
        Sound(0x101);
        FieldS_Answer = static_cast<UC>(static_cast<unsigned char>(FieldS_Answer) ^ 1);
        pressed = LRest2e(::bof3::Img(kPressed.v));
    }
    if (Confirmed(pressed)) {
        if (static_cast<unsigned char>(FieldS_Answer) != 0) {
            Sound(0x104);
            IdList(BRest2e(::bof3::Img(0x80333E)), "FieldItems_DiscardConfirm")[BRest2e(::bof3::Img(0x803340))] = 0;
            CountList(BRest2e(::bof3::Img(0x80333E)), "FieldItems_DiscardConfirm")[BRest2e(::bof3::Img(0x803340))] = 0;
            const auto state = static_cast<UC>(FieldS_State - 3);
            BRest2e(::bof3::Img(0x803341)) = 0xFF;
            FieldS_State = state;
            return;
        }
    } else if (!Cancelled(pressed)) {
        return;
    }
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State - 3);
    BRest2e(::bof3::Img(0x803341)) = 0xFF;
    FieldS_State = state;
}

// original 0x58B8A0: the Items screen's state 7 - a consumable used on a member.
extern "C" void __cdecl FieldItems_UseOnMember(void) {
    Backdrop();
    const U category = BRest2e(::bof3::Img(0x80333E));
    const U index = BRest2e(::bof3::Img(0x803340));
    unsigned char* const id = IdList(category, "FieldItems_UseOnMember") + index;
    const U counts = CountListAt(category, "FieldItems_UseOnMember");
    SH_CALL(TextRecord_Set)(0, 0x10, AtRest2e(::bof3::Img(kConsumables.v) + 22u * *id));
    const int member = BattleActorCopies_ItemActor;
    SetWRest2e(::bof3::Img(0x8032FC), 0x19);
    WindowRecords[19].in_use = 1;
    SetWRest2e(::bof3::Img(0x803410), 0xB6);
    WindowRecords[19].y = static_cast<U>(54 * member + 0x3E);
    const UC n = PartyCountRest2e();
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    const U repeat = AutoRepeat(pressed);
    const UC was = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    UC cl = MoveMemberS8(::bof3::Img(BattleActorCopies_ItemActor_addr), repeat, n);
    if (was != cl) {
        Sound(0x101);
        cl = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    }
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        const UC item = *id;
        const UC record = PartyRecord(S8Rest2e(cl));
        const UC answer = SH_CALL(ItemUse_Dispatch)(record, item, 0);
        if (answer != 0 && answer != 4) {
            Sound(0x107);
            return;
        }
        Sound(0x104);
        unsigned char* const count = CountAt(counts, index, category, "FieldItems_UseOnMember");
        const auto left = static_cast<UC>(*count - 1);
        *count = left;
        if (left != 0) return;
        *id = 0;
    } else if (Cancelled(p)) {
        Sound(0x106);
    } else {
        return;
    }
    const auto state = static_cast<UC>(FieldS_State - 4);
    WindowRecords[19].in_use = 0;
    BRest2e(::bof3::Img(0x803341)) = 0xFF;
    FieldS_State = state;
}

// original 0x58BA40: the Items screen's state 9 - a 32-entry list scrolled.
extern "C" void __cdecl FieldItems_ViewList32(void) {
    Backdrop();
    const U category = BRest2e(::bof3::Img(0x80333E));
    const UC item = IdList(category, "FieldItems_ViewList32")[BRest2e(::bof3::Img(0x803340))];
    const U help = SH_CALL(Item_HelpMessage)(category, item);
    const U top = BRest2e(::bof3::Img(0x80333F));
    SetWRest2e(::bof3::Img(0x8032FC), help);
    const U x = LRest2e(::bof3::Img(0x803338)) + 7;
    WindowRecords[21].in_use = 1;
    SetWRest2e(::bof3::Img(0x803458), x);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0x500C);
    WindowRecords[21].y = 13u * (BRest2e(::bof3::Img(0x803340)) - top + 2) + WRest2e(::bof3::Img(0x80333A));
    const U repeat = AutoRepeat(pressed);
    const UC was = BRest2e(::bof3::Img(0x803340));
    UC cl, al;
    Scroll(repeat, kShortScroll, cl, al);
    const U now = BRest2e(::bof3::Img(0x80333E));
    BRest2e(::bof3::Img(kItemCursors.v) + now) = cl;
    BRest2e(::bof3::Img(kItemTops.v) + now) = al;
    if (was != cl) Sound(0x100);
    if (WRest2e(::bof3::Img(0x803346)) != 0) return;
    if (!Cancelled(static_cast<std::uint32_t>(Input_Pressed))) return;
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State - 7);
    const UC t = BRest2e(::bof3::Img(kItemTops.v));
    FieldS_State = state;
    BRest2e(::bof3::Img(0x80333E)) = 0;
    BRest2e(::bof3::Img(0x80333F)) = t;
    BRest2e(::bof3::Img(0x803340)) = 0xFF;
    BRest2e(::bof3::Img(0x803341)) = 0xFF;
}

// original 0x58BC30: the Items screen's windows.
extern "C" void __cdecl FieldItems_InitWindows(void) {
    BRest2e(::bof3::Img(0x8032EF)) = 2;
    BRest2e(::bof3::Img(0x803313)) = 2;
    BRest2e(::bof3::Img(0x803337)) = 2;
    BRest2e(::bof3::Img(0x80333F)) = BRest2e(::bof3::Img(kItemTops.v));
    BRest2e(::bof3::Img(0x803340)) = 0xFF;
    BRest2e(::bof3::Img(0x803341)) = 0xFF;
    BRest2e(::bof3::Img(0x8032ED)) = 6;
    BRest2e(::bof3::Img(0x8032EE)) = 9;
    BRest2e(::bof3::Img(0x8032EC)) = 1;
    SetWRest2e(::bof3::Img(0x8032F0), 0x14);
    SetWRest2e(::bof3::Img(0x8032F2), 0xFFEC);
    SetWRest2e(::bof3::Img(0x8032FC), 0);
    BRest2e(::bof3::Img(0x803311)) = 6;
    BRest2e(::bof3::Img(0x803312)) = 0xD;
    BRest2e(::bof3::Img(0x803310)) = 1;
    SetWRest2e(::bof3::Img(0x803314), 0x40);
    SetWRest2e(::bof3::Img(0x803316), 0xFFEC);
    BRest2e(::bof3::Img(0x80331A)) = 0;
    BRest2e(::bof3::Img(0x80331B)) = 0xFF;
    BRest2e(::bof3::Img(0x803335)) = 6;
    BRest2e(::bof3::Img(0x803336)) = 0xC;
    BRest2e(::bof3::Img(0x803334)) = 1;
    SetWRest2e(::bof3::Img(0x803338), 0xFF38);
    SetWRest2e(::bof3::Img(0x80333A), 0x3E);
    BRest2e(::bof3::Img(0x80333E)) = 0;
    BRest2e(::bof3::Img(0x80333C)) = 0;
    BRest2e(::bof3::Img(0x80333D)) = 0;
    SetWRest2e(::bof3::Img(0x803344), 0);
    SetWRest2e(::bof3::Img(0x803346), 0);
    BRest2e(::bof3::Img(0x80340D)) = 6;
    BRest2e(::bof3::Img(0x80340E)) = 0xA;
    WindowRecords[19].in_use = 0;
    BRest2e(::bof3::Img(0x803416)) = 1;
    BRest2e(::bof3::Img(0x803417)) = 1;
    BRest2e(::bof3::Img(0x803455)) = 6;
    BRest2e(::bof3::Img(0x803456)) = 0xE;
    BRest2e(::bof3::Img(0x803457)) = 0;
    WindowRecords[21].in_use = 0;
}

// original 0x58BD40: a tail jump through FieldItems_Sorts by the argument's low
// byte, unchecked; the entries read no argument.
extern "C" void __cdecl FieldItems_Sort(unsigned how) {
    [[clang::musttail]] return reinterpret_cast<void (__cdecl*)(unsigned)>(
        Entry(AddrOf(FieldItems_Sorts), 7, how & 0xFF, "FieldItems_Sort"))(how);
}

// original 0x58BD50: the two bytes exchanged.
extern "C" void __cdecl FieldMenu_SwapBytes(unsigned char* a, unsigned char* b) {
    const UC first = *a;
    const UC second = *b;
    *a = second;
    *b = first;
}

// original 0x58BD70: FieldItems_Sorts[0] - the empty entries moved to the end.
extern "C" void __cdecl FieldItemSort_Compact(void) {
    for (U bound = 0x7F; bound != 0; --bound) {
        const U category = BRest2e(::bof3::Img(0x80333E));
        unsigned char* const ids = IdList(category, "FieldItemSort_Compact");
        const U counts = LRest2e(::bof3::Img(kCountLists.v) + 4 * category);
        for (U i = 0; i < bound; ++i) {
            if (ids[i] != 0 || ids[i + 1] == 0) continue;
            Swap(ids + i, ids + i + 1);
            if (counts == 0) bof3::Fatal("FieldItemSort_Compact: category %u's count list is a null pointer (the original faults)", (unsigned)category);
            Swap(AtRest2e(counts + i), AtRest2e(counts + i + 1));
        }
    }
}

// original 0x58BE00: FieldItems_Sorts[1] - consumables with flag bit 0 first
// (DIV-0189: then by id within each half).
extern "C" void __cdecl FieldItemSort_ConsumableFlag1(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(::bof3::Img(kConsumableIds.v), [](UC x, UC y) { return ConsumableGoesFirst(x, y, 1); });
}

// original 0x58BEB0: FieldItems_Sorts[2] - consumables with flag bit 1 first
// (DIV-0189: then by id within each half).
extern "C" void __cdecl FieldItemSort_ConsumableFlag2(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(::bof3::Img(kConsumableIds.v), [](UC x, UC y) { return ConsumableGoesFirst(x, y, 2); });
}

// original 0x58BF60: FieldItems_Sorts[3] - weapons by +0x16, the highest first.
extern "C" void __cdecl FieldItemSort_WeaponsByPower(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(::bof3::Img(kWeaponIds.v), [](UC x, UC y) { return BRest2e(::bof3::Img(kWeaponB.v) + 28u * x) < BRest2e(::bof3::Img(kWeaponB.v) + 28u * y); });
}

// original 0x58C010: FieldItems_Sorts[4] - armour by +0x14, the highest first.
extern "C" void __cdecl FieldItemSort_ArmourByPower(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(::bof3::Img(kArmourIds.v), [](UC x, UC y) { return BRest2e(::bof3::Img(kArmourB.v) + 26u * x) < BRest2e(::bof3::Img(kArmourB.v) + 26u * y); });
}

// original 0x58C0B0: FieldItems_Sorts[5] - the category's list by Item_IconKind.
extern "C" void __cdecl FieldItemSort_ByIconKind(void) {
    SH_CALL(FieldItemSort_Compact)();
    const UC category = BRest2e(::bof3::Img(0x80333E));
    for (U bound = 0x7F; bound != 0; --bound) {
        unsigned char* const ids = IdList(category, "FieldItemSort_ByIconKind");
        const U counts = CountListAt(category, "FieldItemSort_ByIconKind");
        for (U i = 0; i < bound; ++i) {
            if (ids[i] == 0) continue;
            const UC y = ids[i + 1];
            if (y == 0) continue;
            const auto second = static_cast<UC>(SH_CALL(Item_IconKind)(category, y));
            const UC x = ids[i];
            const auto first = static_cast<UC>(SH_CALL(Item_IconKind)(category, x));
            if (first <= second) continue;
            Swap(ids + i, ids + i + 1);
            Swap(CountAt(counts, i, category, "FieldItemSort_ByIconKind"), AtRest2e(counts + i + 1));
        }
    }
}

// original 0x58C1A0: FieldItems_Sorts[6] - the entries member s8 0x929F06 can
// equip first.
extern "C" void __cdecl FieldItemSort_EquipableFirst(void) {
    SH_CALL(FieldItemSort_Compact)();
    const UC record = PartyRecord(BattleActorCopies_ItemActor);
    const UC category = BRest2e(::bof3::Img(0x80333E));
    const UC bit = Bit8(record);
    for (U bound = 0x7F; bound != 0; --bound) {
        unsigned char* const ids = IdList(category, "FieldItemSort_EquipableFirst");
        const U counts = CountListAt(category, "FieldItemSort_EquipableFirst");
        for (U i = 0; i < bound; ++i) {
            const UC x = ids[i];
            if (x == 0) continue;
            if (ids[i + 1] == 0) continue;
            const U first = SH_CALL(Item_EquipMask)(category, x);
            if ((bit & first) != 0) continue;
            const UC y = ids[i + 1];
            const U second = SH_CALL(Item_EquipMask)(category, y);
            if ((bit & second) == 0) continue;
            Swap(ids + i, ids + i + 1);
            Swap(CountAt(counts, i, category, "FieldItemSort_EquipableFirst"), AtRest2e(counts + i + 1));
        }
    }
}

// original 0x58C2A0: three of the Items screen's windows closed.
extern "C" void __cdecl FieldItems_CloseWindows(void) {
    BRest2e(::bof3::Img(0x803310)) = 0;
    BRest2e(::bof3::Img(0x803334)) = 0;
    BRest2e(::bof3::Img(0x803358)) = 0;
}

// original 0x58C2C0: FieldMenu_States[4], a tail jump through FieldEquip_States
// by the state byte, unchecked.
extern "C" void __cdecl FieldEquip_Run(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(Entry(AddrOf(FieldEquip_States), 9, FieldS_State, "FieldEquip_Run"))();
}

// original 0x58C2D0: FieldEquip_States[0].
extern "C" void __cdecl FieldEquip_Open(void) {
    Backdrop();
    SH_CALL(FieldEquip_InitWindows)();
    Sound(0x102);
    const auto state = static_cast<UC>(FieldS_State + 1);
    Rest2e_EquipCursor = 0;
    BRest2e(::bof3::Img(kTimer.v)) = 5;
    FieldS_State = state;
}

// original 0x58C310: FieldEquip_States[1] and [4], FieldAbility_States[1].
extern "C" void __cdecl FieldMenu_CountdownState(void) {
    Backdrop();
    const auto t = static_cast<UC>(BRest2e(::bof3::Img(kTimer.v)) - 1);
    BRest2e(::bof3::Img(kTimer.v)) = t;
    if (t == 0) FieldS_State = static_cast<UC>(FieldS_State + 1);
}

// original 0x58C340: FieldEquip_States[2] - the four-entry menu.
extern "C" void __cdecl FieldEquip_TopMenu(void) {
    Backdrop();
    const int c = Rest2e_EquipCursor;
    const U help = WRest2e(static_cast<U>(static_cast<int>(AddrOf(FieldEquip_TopHelp)) + 2 * c));
    const U x = LRest2e(::bof3::Img(0x803314)) + static_cast<U>(48 * c);
    SetWRest2e(::bof3::Img(0x8032FC), help);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0xA000;
    WindowRecords[21].in_use = 1;
    const U y = WRest2e(::bof3::Img(0x803316)) + 4;
    SetWRest2e(::bof3::Img(0x803458), x);
    SetWRest2e(::bof3::Img(0x80345A), y);
    BRest2e(::bof3::Img(0x80331B)) = 0xFF;
    BRest2e(::bof3::Img(0x803341)) = 1;
    const U repeat = AutoRepeat(pressed);
    const UC was = static_cast<unsigned char>(Rest2e_EquipCursor);
    const UC cl = TurnFour(::bof3::Img(Rest2e_EquipCursor_addr), repeat);
    if (was != cl) Sound(0x101);
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        Sound(0x104);
        BRest2e(::bof3::Img(0x80331B)) = static_cast<unsigned char>(Rest2e_EquipCursor);
        const auto state = static_cast<UC>(FieldS_State + 1);
        WindowRecords[21].in_use = 0;
        FieldS_State = state;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x102);
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State + 5);
    BRest2e(::bof3::Img(0x8032EF)) = 1;
    BRest2e(::bof3::Img(0x803313)) = 1;
    BRest2e(::bof3::Img(0x803337)) = 1;
    WindowRecords[19].in_use = 0;
    WindowRecords[21].in_use = 0;
    FieldS_State = state;
    BRest2e(::bof3::Img(kTimer.v)) = 0;
}

// original 0x58C4A0: FieldEquip_States[3] - the member.
extern "C" void __cdecl FieldEquip_PickMember(void) {
    Backdrop();
    const U m0 = BRest2e(::bof3::Img(0x803340));
    const U wx = WRest2e(::bof3::Img(0x8031F4));
    WindowRecords[19].in_use = 1;
    SetWRest2e(::bof3::Img(0x803410), wx);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    WindowRecords[19].y = 54 * m0 + 0x3E;
    const U repeat = AutoRepeat(pressed);
    const UC was = BRest2e(::bof3::Img(0x803340));
    const UC n = PartyCountRest2e();
    if (repeat & 0x1000) {
        const auto m = static_cast<UC>(BRest2e(::bof3::Img(0x803340)) - 1);
        BRest2e(::bof3::Img(0x803340)) = m;
        if (S8Rest2e(m) < 0) BRest2e(::bof3::Img(0x803340)) = static_cast<UC>(n - 1);
    } else if (repeat & 0x4000) {
        const auto m = static_cast<UC>(BRest2e(::bof3::Img(0x803340)) + 1);
        BRest2e(::bof3::Img(0x803340)) = m;
        if (static_cast<int>(BRest2e(::bof3::Img(0x803340))) > static_cast<int>(n) - 1) BRest2e(::bof3::Img(0x803340)) = 0;
    }
    if (S8Rest2e(was) != static_cast<int>(BRest2e(::bof3::Img(0x803340)))) Sound(0x101);
    if (static_cast<unsigned char>(Rest2e_EquipCursor) == 1) SH_CALL(FieldEquip_BestByPower)(BRest2e(::bof3::Img(0x803340)));
    if (static_cast<unsigned char>(Rest2e_EquipCursor) == 2) SH_CALL(FieldEquip_BestByOrder)(BRest2e(::bof3::Img(0x803340)));
    SH_CALL(TextRecord_Set)(0, 8, Record(PartyRecord(BRest2e(::bof3::Img(0x803340)))));
    const int c = Rest2e_EquipCursor;
    const U help = WRest2e(static_cast<U>(static_cast<int>(AddrOf(FieldEquip_MemberHelp)) + 2 * c));
    const UC flag = BRest2e(static_cast<U>(static_cast<int>(AddrOf(FieldEquip_MemberFlags)) + 2 * c));
    const U p = Input_Pressed;
    SetWRest2e(::bof3::Img(0x8032FC), help);
    BRest2e(::bof3::Img(0x803341)) = flag;
    if (Confirmed(p)) {
        Sound(0x103);
        const UC cursor = static_cast<unsigned char>(Rest2e_EquipCursor);
        if (cursor == 0) {
            Sound(0x102);
            UC k = 0;
            if (PartyCountRest2e() != 0) {
                do {
                    BRest2e(::bof3::Img(0x8031F3) + 0x24u * k) = 7;
                    ++k;
                } while (k < PartyCountRest2e());
            }
            const UC m = BRest2e(::bof3::Img(0x803340));
            WindowRecords[19].in_use = 0;
            BRest2e(::bof3::Img(0x803337)) = 3;
            BRest2e(::bof3::Img(0x80333E)) = 0;
            BRest2e(::bof3::Img(0x80333F)) = 0xFF;
            BRest2e(::bof3::Img(0x803359)) = 6;
            BRest2e(::bof3::Img(0x803362)) = BRest2e(::bof3::Img(kEquipTops.v) + 6u * m);
            const auto state = static_cast<UC>(FieldS_State + 1);
            BRest2e(::bof3::Img(0x80335A)) = 0x12;
            BRest2e(::bof3::Img(0x80335B)) = 4;
            BRest2e(::bof3::Img(0x803358)) = 1;
            SetWRest2e(::bof3::Img(0x80335C), 0x140);
            SetWRest2e(::bof3::Img(0x80335E), 0x3E);
            BRest2e(::bof3::Img(0x803364)) = m;
            BRest2e(::bof3::Img(0x803363)) = 0xFF;
            BRest2e(::bof3::Img(0x803360)) = 0;
            BRest2e(::bof3::Img(0x803365)) = 0;
            BRest2e(::bof3::Img(0x803361)) = 0;
            SetWRest2e(::bof3::Img(0x803368), 0);
            BattleActorCopies_ItemActor = m;
            BRest2e(::bof3::Img(kTimer.v)) = 5;
            FieldS_State = state;
            return;
        }
        if (cursor == 3) {
            const auto state = static_cast<UC>(FieldS_State + 5);
            BRest2e(::bof3::Img(0x803417)) = 0;
            BRest2e(::bof3::Img(0x80333E)) = 1;
            BRest2e(::bof3::Img(0x80333F)) = 0xFF;
            FieldS_State = state;
            return;
        }
        SH_CALL(FieldEquip_ApplyPreview)();
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State - 1);
    const UC m = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    WindowRecords[19].in_use = 0;
    BRest2e(::bof3::Img(0x937F8E)) = m;
    BRest2e(::bof3::Img(kTimer.v)) = 0;
    FieldS_State = state;
}

// original 0x58C7A0: the slot. The backdrop; the hand on at (window 0x803334's
// x + 6, its y + 13 slot + 0x57); the repeat of 0xF000: 0x4000 down (sound
// 0x101; past 5 to 0), 0x1000 up (below 0, s8, to 5); window +0xB 0xFF, 0x803341
// 1; 0x8000 the member back (sound 0x101; below 0 to Party_Count(0) less one),
// 0x2000 on (past it to 0). The member (0x803340) and slot (0x80333E) to the
// item window 0x803358 (+0xC, +9), its top from 0x939880 [slot + 6 member];
// the slot's byte of the member's record - slot 0 (or past 5) the weapon +0x12
// (category 1), 1..3 the armour +0x13..+0x15 (2), 4 and 5 the accessories
// +0x16 / +0x17 (3) - and its help word (Item_HelpMessage) to 0x8032FC.
// Confirm: sound 0x103, +0xB the slot, the item window's cursor from
// 0x9398A0 [slot + 6 member], the state up one. Pressed bit 4: an item in a
// slot but the weapon's is taken off (sound 0x103, Inventory_Add(category,
// item, 1), the byte 0, Char_RecalcStats), else a buzz (0x107). Cancel:
// sounds 0x106 and 0x102, the member window's +0x1B 3, 0x803337 4, the slot and
// +0xB 0xFF, each member's window (0x8031F0 + 0x24 k) +3 8, the hand off, the
// counter 0, the state down two.
extern "C" void __cdecl Equip_ChooseSlot(void) {
    records::WindowRecord* const win13 = &WindowRecords[0xd];
    records::WindowRecord* const win14 = &WindowRecords[0xe];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const U slot0 = B(::bof3::Img(0x80333E));
    win21->in_use = 1;
    win21->x = L(::bof3::Img(0x803338)) + 6;
    win21->y = 13 * slot0 + static_cast<U>(win13->y) + 0x57;
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & 0xF000);
    B(::bof3::Img(0x80333F)) = 0xFF;
    B(::bof3::Img(0x803341)) = 1;
    if (repeat & 0x4000) {
        Sound(0x101);
        const auto s = static_cast<unsigned char>(B(::bof3::Img(0x80333E)) + 1);
        B(::bof3::Img(0x80333E)) = s;
        if (s > 5) B(::bof3::Img(0x80333E)) = 0;
    } else if (repeat & 0x1000) {
        Sound(0x101);
        const auto s = static_cast<unsigned char>(B(::bof3::Img(0x80333E)) - 1);
        B(::bof3::Img(0x80333E)) = s;
        if (S8(s) < 0) B(::bof3::Img(0x80333E)) = 5;
    }
    auto count = static_cast<unsigned char>(SH_CALL(Party_Count)(0));
    if (repeat & 0x8000) {
        Sound(0x101);
        const auto m = static_cast<unsigned char>(B(::bof3::Img(0x803340)) - 1);
        B(::bof3::Img(0x803340)) = m;
        if (S8(m) < 0) {
            --count;
            B(::bof3::Img(0x803340)) = count;
        }
    } else if (repeat & 0x2000) {
        Sound(0x101);
        const auto m = static_cast<unsigned char>(B(::bof3::Img(0x803340)) + 1);
        B(::bof3::Img(0x803340)) = m;
        if (static_cast<int>(B(::bof3::Img(0x803340))) > S8(count) - 1) B(::bof3::Img(0x803340)) = 0;
    }
    const unsigned char member = B(::bof3::Img(0x803340));
    const unsigned char slot = B(::bof3::Img(0x80333E));
    B(::bof3::Img(0x803364)) = member;
    B(::bof3::Img(0x803361)) = slot;
    B(::bof3::Img(0x803362)) = B(::bof3::Img(field_s::at::kEquipBytes) + slot + 6u * member);
    const U record = RecAddr(RecordOf(B(::bof3::Img(field_s::at::kPartyList) + member)));
    U category, cell;
    switch (slot) {
    case 1: category = 2; cell = record + 0x13; break;
    case 2: category = 2; cell = record + 0x14; break;
    case 3: category = 2; cell = record + 0x15; break;
    case 4: category = 3; cell = record + 0x16; break;
    case 5: category = 3; cell = record + 0x17; break;
    default: category = 1; cell = record + 0x12; break;
    }
    SetW(::bof3::Img(0x8032FC), SH_CALL(Item_HelpMessage)(category, B(cell)));
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        Sound(0x103);
        const U m = B(::bof3::Img(0x803340));
        const unsigned char s = B(::bof3::Img(0x80333E));
        B(::bof3::Img(0x80333F)) = s;
        const auto state = static_cast<unsigned char>(FieldS_State + 1);
        B(::bof3::Img(0x803363)) = B(::bof3::Img(field_s::at::kEquipCursor) + s + 6u * m);
        FieldS_State = state;
        return;
    }
    if (pressed & Sigma(0x10)) {   // DIVERGENCE DIV-0139: Japan's triangle through σ (0x58C9DE)
        if (B(::bof3::Img(0x80333E)) != 0 && B(cell) != 0) {
            Sound(0x103);
            SH_CALL(Inventory_Add)(category, B(cell), 1);
            B(cell) = 0;
            SH_CALL(Char_RecalcStats)(At(record));
            return;
        }
        Sound(0x107);
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    Sound(0x102);
    win14->state = 3;
    win13->state = 4;
    B(::bof3::Img(0x80333F)) = 0xFF;
    B(::bof3::Img(0x80333E)) = 0xFF;
    if (PartyCount() != 0) {
        unsigned char k = 0;
        do {
            B(::bof3::Img(0x8031F3) + 0x24u * k) = 8;
            ++k;
        } while (k < PartyCount());
    }
    const auto state = FieldS_State;
    win21->in_use = 0;
    B(::bof3::Img(field_s::at::kTimer)) = 0;
    FieldS_State = static_cast<unsigned char>(state - 2);
}

// original 0x58CAE0: the item (the item window 0x803358: +0xA the top, +0xB
// the cursor, +8 the category, +0xD the chosen item, +0x10 the scroll word).
// The backdrop; the hand on at (its x + 7, its y + 13 (cursor - top + 2)); the
// chosen item's help word to 0x8032FC; the repeat of 0xF00C; the preview
// 0x58D640; the scroll of a 128-entry list shown nine rows at a time (as
// SharedList_PickShared's, but the page down's test is on the cursor), the
// scroll word 0xF0 / 0x10 set when the cursor leaves the page; the top and
// cursor kept in 0x939880 / 0x9398A0 [slot + 6 member] - slots 4 and 5 both
// (an accessory's two); sound 0x100 when the cursor moved. While the list
// scrolls nothing more. Confirm: with an item, sound 0x104, the change
// 0x58D570, the cursor 0xFF and the state down one; without, a buzz. Cancel:
// sound 0x106, the cursor 0xFF, window 0x803400's +0xC 0, the state down one.
extern "C" void __cdecl Equip_ChooseItem(void) {
    records::WindowRecord* const win14 = &WindowRecords[0xe];
    records::WindowRecord* const win19 = &WindowRecords[0x13];
    records::WindowRecord* const win21 = &WindowRecords[0x15];
    SH_CALL(Menu_DrawBackdrop)(Config_Background);
    const U hand_y = 13 * (static_cast<U>(B(::bof3::Img(0x803363))) - B(::bof3::Img(0x803362)) + 2) + static_cast<U>(win14->y);
    win21->x = L(::bof3::Img(0x80335C)) + 7;
    win21->in_use = 1;
    win21->y = hand_y;
    SetW(::bof3::Img(0x8032FC), SH_CALL(Item_HelpMessage)(B(::bof3::Img(0x803360)), B(::bof3::Img(0x803365))));
    const U repeat = SH_CALL(Input_AutoRepeat)(Pressed() & Sigma(0xF00C));
    SH_AT(void (__cdecl*)(), field_s::at::kEquipPreview.v)();
    unsigned char al = B(::bof3::Img(0x803363));
    const unsigned char before = al;
    unsigned char cl;
    if (repeat & 0x1000) {
        if (al != 0) {
            --al;
            B(::bof3::Img(0x803363)) = al;
        }
        cl = B(::bof3::Img(0x803362));
        if (al < cl) SetW(::bof3::Img(0x803368), 0xF0);
    } else if (repeat & 0x4000) {
        if (al < 0x7F) {
            ++al;
            B(::bof3::Img(0x803363)) = al;
        }
        cl = B(::bof3::Img(0x803362));
        if (static_cast<int>(al) >= static_cast<int>(cl) + 9) SetW(::bof3::Img(0x803368), 0x10);
    } else if (repeat & Sigma(4)) {
        cl = B(::bof3::Img(0x803362));
        if (cl == 0) {
            al = 0;
            B(::bof3::Img(0x803363)) = 0;
        } else if (cl < 9) {
            al = static_cast<unsigned char>(al - cl);
            cl = 0;
            B(::bof3::Img(0x803363)) = al;
            B(::bof3::Img(0x803362)) = cl;
        } else {
            al = static_cast<unsigned char>(al - 9);
            cl = static_cast<unsigned char>(cl - 9);
            B(::bof3::Img(0x803363)) = al;
            B(::bof3::Img(0x803362)) = cl;
        }
    } else {
        cl = B(::bof3::Img(0x803362));
        if (repeat & Sigma(8)) {
            if (cl == 0x77) {
                al = 0x7F;
                B(::bof3::Img(0x803363)) = 0x7F;
            } else if (al > 0x6E) {
                const auto d = static_cast<unsigned char>(0x77 - cl);
                cl = 0x77;
                al = static_cast<unsigned char>(al + d);
                B(::bof3::Img(0x803363)) = al;
                B(::bof3::Img(0x803362)) = cl;
            } else {
                al = static_cast<unsigned char>(al + 9);
                cl = static_cast<unsigned char>(cl + 9);
                B(::bof3::Img(0x803363)) = al;
                B(::bof3::Img(0x803362)) = cl;
            }
        }
    }
    const unsigned char slot = B(::bof3::Img(0x80333E));
    const U row = 6u * B(::bof3::Img(0x803340));
    if (slot < 4) {
        B(::bof3::Img(field_s::at::kEquipBytes) + slot + row) = cl;
        B(::bof3::Img(field_s::at::kEquipCursor) + slot + row) = al;
    } else {
        B(::bof3::Img(field_s::at::kEquipBytes) + 5 + row) = cl;
        B(::bof3::Img(field_s::at::kEquipCursor) + 5 + row) = al;
        B(::bof3::Img(field_s::at::kEquipBytes) + 4 + row) = cl;
        B(::bof3::Img(field_s::at::kEquipCursor) + 4 + row) = al;
    }
    if (before != al) Sound(0x100);
    if (W(::bof3::Img(0x803368)) != 0) return;
    const U pressed = Pressed();
    if (Confirmed(pressed)) {
        if (B(::bof3::Img(0x803365)) != 0) {
            Sound(0x104);
            SH_AT(void (__cdecl*)(), field_s::at::kEquipApply.v)();
            const auto state = FieldS_State;
            B(::bof3::Img(0x803363)) = 0xFF;
            FieldS_State = static_cast<unsigned char>(state - 1);
            return;
        }
        Sound(0x107);
        return;
    }
    if (!Cancelled(pressed)) return;
    Sound(0x106);
    const auto state = FieldS_State;
    B(::bof3::Img(0x803363)) = 0xFF;
    win19->in_use = 0;
    FieldS_State = static_cast<unsigned char>(state - 1);
}

// original 0x58CD40: FieldEquip_States[7] - the screen closed after 4 frames.
extern "C" void __cdecl FieldEquip_Close(void) {
    Backdrop();
    const auto t = static_cast<UC>(BRest2e(::bof3::Img(kTimer.v)) + 1);
    BRest2e(::bof3::Img(kTimer.v)) = t;
    if (t != 4) return;
    SH_CALL(FieldEquip_CloseWindows)();
    U k = 0;
    if (PartyCountRest2e() != 0) {
        do {
            BRest2e(::bof3::Img(0x8031F3) + 0x24u * k) = 4;
            ++k;
        } while (static_cast<int>(k) < static_cast<int>(PartyCountRest2e()));
    }
    const UC m = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    BRest2e(::bof3::Img(0x80325F)) = 2;
    BRest2e(::bof3::Img(0x803283)) = 2;
    BRest2e(::bof3::Img(0x8032A7)) = 2;
    BRest2e(::bof3::Img(0x8032CB)) = 2;
    BRest2e(::bof3::Img(0x937F8E)) = m;
    BRest2e(::bof3::Img(kTimer.v)) = 0;
    BattleE1_Mode = 1;
    FieldS_State = 0;
}

// original 0x58CDD0: FieldEquip_States[8] - a slot's item taken off.
extern "C" void __cdecl FieldEquip_RemoveSlot(void) {
    Backdrop();
    unsigned char* const record = Record(PartyRecord(BRest2e(::bof3::Img(0x803340))));
    U category;
    unsigned char* cell;
    switch (BRest2e(::bof3::Img(0x80333E))) {
    case 1: category = 2; cell = record + 0x13; break;
    case 2: category = 2; cell = record + 0x14; break;
    case 3: category = 2; cell = record + 0x15; break;
    case 4: category = 3; cell = record + 0x16; break;
    case 5: category = 3; cell = record + 0x17; break;
    default: category = 1; cell = record + 0x12; break;
    }
    const U help = SH_CALL(Item_HelpMessage)(category, *cell);
    const U x = LRest2e(::bof3::Img(0x803338)) + 6;
    SetWRest2e(::bof3::Img(0x8032FC), help);
    const U y = 13u * BRest2e(::bof3::Img(0x80333E)) + WRest2e(::bof3::Img(0x80333A)) + 0x57;
    WindowRecords[21].in_use = 1;
    SetWRest2e(::bof3::Img(0x803458), x);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    SetWRest2e(::bof3::Img(0x80345A), y);
    const U repeat = AutoRepeat(pressed);
    if (repeat & 0x4000) {
        Sound(0x101);
        const auto s = static_cast<UC>(BRest2e(::bof3::Img(0x80333E)) + 1);
        BRest2e(::bof3::Img(0x80333E)) = s;
        if (s > 5) BRest2e(::bof3::Img(0x80333E)) = 1;
    } else if (repeat & 0x1000) {
        Sound(0x101);
        const auto s = static_cast<UC>(BRest2e(::bof3::Img(0x80333E)) - 1);
        BRest2e(::bof3::Img(0x80333E)) = s;
        if (s < 1) BRest2e(::bof3::Img(0x80333E)) = 5;
    }
    const UC can = SH_CALL(FieldEquip_PreviewRemove)();
    if (Confirmed(static_cast<std::uint32_t>(Input_Pressed))) {
        if (can != 0) {
            Sound(0x103);
            const UC item = *cell;
            SH_CALL(Inventory_Add)(category, item, 1);
            *cell = 0;
            SH_CALL(Char_RecalcStats)(record);
            SH_CALL(FieldEquip_ApplyPreview)();
        } else {
            Sound(0x107);
        }
    }
    if (!Cancelled(static_cast<std::uint32_t>(Input_Pressed))) return;
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State - 5);
    BRest2e(::bof3::Img(0x803417)) = 1;
    WindowRecords[21].in_use = 0;
    BRest2e(::bof3::Img(0x80333E)) = 0xFF;
    BRest2e(::bof3::Img(kTimer.v)) = 0;
    FieldS_State = state;
}

// original 0x58CFC0: the Equipment screen's windows.
extern "C" void __cdecl FieldEquip_InitWindows(void) {
    BRest2e(::bof3::Img(0x8032ED)) = 6;
    BRest2e(::bof3::Img(0x8032EE)) = 9;
    BRest2e(::bof3::Img(0x8032EF)) = 2;
    BRest2e(::bof3::Img(0x8032EC)) = 1;
    SetWRest2e(::bof3::Img(0x8032F0), 0x14);
    SetWRest2e(::bof3::Img(0x8032F2), 0xFFEC);
    SetWRest2e(::bof3::Img(0x8032FC), 0);
    BRest2e(::bof3::Img(0x803311)) = 6;
    BRest2e(::bof3::Img(0x803312)) = 0xD;
    BRest2e(::bof3::Img(0x803313)) = 2;
    BRest2e(::bof3::Img(0x803310)) = 1;
    SetWRest2e(::bof3::Img(0x803314), 0x40);
    SetWRest2e(::bof3::Img(0x803316), 0xFFEC);
    BRest2e(::bof3::Img(0x80331A)) = 1;
    BRest2e(::bof3::Img(0x80331B)) = 0xFF;
    BRest2e(::bof3::Img(0x803335)) = 6;
    BRest2e(::bof3::Img(0x803336)) = 0x10;
    BRest2e(::bof3::Img(0x803337)) = 2;
    BRest2e(::bof3::Img(0x803334)) = 1;
    SetWRest2e(::bof3::Img(0x803338), 0x140);
    SetWRest2e(::bof3::Img(0x80333A), 0x3E);
    BRest2e(::bof3::Img(0x803340)) = 0;
    BRest2e(::bof3::Img(0x80333E)) = 0xFF;
    BRest2e(::bof3::Img(0x80333F)) = 0xFF;
    BRest2e(::bof3::Img(0x803341)) = 1;
    move_script::SetLong(AtRest2e(::bof3::Img(0x803354)), static_cast<std::int32_t>(::bof3::Img(kPreview.v)));
    BRest2e(::bof3::Img(0x80340D)) = 6;
    BRest2e(::bof3::Img(0x80340E)) = 0xA;
    WindowRecords[19].in_use = 0;
    BRest2e(::bof3::Img(0x803416)) = 1;
    BRest2e(::bof3::Img(0x803417)) = 1;
    BRest2e(::bof3::Img(0x803455)) = 6;
    BRest2e(::bof3::Img(0x803456)) = 0xE;
    BRest2e(::bof3::Img(0x803457)) = 0;
    WindowRecords[21].in_use = 0;
}

// original 0x58D0C0: the preview of the highest +0x16 weapon and +0x14 armour
// the member can equip.
extern "C" void __cdecl FieldEquip_BestByPower(unsigned member) {
    const UC index = PartyRecord(static_cast<int>(member & 0xFF));
    const UC bit = Bit8(index);
    unsigned char* const record = Record(index);
    BRest2e(::bof3::Img(kPreview.v)) = record[0x12];
    U best = BRest2e(::bof3::Img(kWeaponB.v) + 28u * BRest2e(::bof3::Img(kPreview.v)));
    for (U k = 0; k < 0x80; ++k) {
        const UC id = AtRest2e(LRest2e(::bof3::Img(kIdLists.v) + 4))[k];
        const UC power = BRest2e(::bof3::Img(kWeaponB.v) + 28u * id);
        const U mask = SH_CALL(Item_EquipMask)(1, id);
        if ((bit & mask) == 0) continue;
        if (best < power || (best == power && BRest2e(::bof3::Img(kPreview.v)) > id)) {
            best = BRest2e(::bof3::Img(kWeaponB.v) + 28u * id);
            BRest2e(::bof3::Img(kPreview.v)) = id;
        }
    }
    for (U j = 0; j < 3; ++j) {
        const U slot = ::bof3::Img(kPreview.v) + 1 + j;
        BRest2e(slot) = record[0x13 + j];
        best = BRest2e(::bof3::Img(kArmourB.v) + 26u * BRest2e(slot));
        for (U k = 0; k < 0x80; ++k) {
            const UC id = AtRest2e(LRest2e(::bof3::Img(kIdLists.v) + 8))[k];
            const UC type = BRest2e(::bof3::Img(kArmourType.v) + 26u * id);
            const UC power = BRest2e(::bof3::Img(kArmourB.v) + 26u * id);
            if (type != 2 + j) continue;
            const U mask = SH_CALL(Item_EquipMask)(2, id);
            if ((bit & mask) == 0) continue;
            if (best < power || (best == power && BRest2e(slot) > id)) {
                best = BRest2e(::bof3::Img(kArmourB.v) + 26u * id);
                BRest2e(slot) = id;
            }
        }
    }
    BRest2e(::bof3::Img(kPreview.v) + 4) = record[0x16];
    BRest2e(::bof3::Img(kPreview.v) + 5) = record[0x17];
}

// original 0x58D2E0: the preview by the lowest +0x14 / +0x13 first, then the
// highest +0x16 / +0x14 (docs/rest_2e.md section 7 for the armour's two quirks).
extern "C" void __cdecl FieldEquip_BestByOrder(unsigned member) {
    const UC index = PartyRecord(static_cast<int>(member & 0xFF));
    const UC bit = Bit8(index);
    unsigned char* const record = Record(index);
    BRest2e(::bof3::Img(kPreview.v)) = record[0x12];
    const UC current = BRest2e(::bof3::Img(kPreview.v));
    U best_b = BRest2e(::bof3::Img(kWeaponB.v) + 28u * current);
    UC best_a = BRest2e(::bof3::Img(kWeaponA.v) + 28u * current);
    for (U k = 0; k < 0x80; ++k) {
        const UC id = AtRest2e(LRest2e(::bof3::Img(kIdLists.v) + 4))[k];
        const UC a = BRest2e(::bof3::Img(kWeaponA.v) + 28u * id);
        const UC b = BRest2e(::bof3::Img(kWeaponB.v) + 28u * id);
        const U mask = SH_CALL(Item_EquipMask)(1, id);
        if ((bit & mask) == 0) continue;
        const UC chosen = BRest2e(::bof3::Img(kPreview.v));
        bool take;
        if (chosen == 0)
            take = true;
        else if (best_a > a)
            take = true;
        else if (best_a != a)
            take = false;
        else if (best_b < b)
            take = true;
        else if (best_b != b)
            take = false;
        else
            take = chosen > id;
        if (!take) continue;
        best_b = BRest2e(::bof3::Img(kWeaponB.v) + 28u * id);
        BRest2e(::bof3::Img(kPreview.v)) = id;
        best_a = BRest2e(::bof3::Img(kWeaponA.v) + 28u * id);
    }
    for (U j = 0; j < 3; ++j) {
        const U slot = ::bof3::Img(kPreview.v) + 1 + j;
        BRest2e(slot) = record[0x13 + j];
        const UC now = BRest2e(slot);
        best_b = BRest2e(::bof3::Img(kArmourB.v) + 26u * now);
        best_a = BRest2e(::bof3::Img(kArmourA.v) + 26u * now);
        for (U k = 0; k < 0x80; ++k) {
            const UC id = AtRest2e(LRest2e(::bof3::Img(kIdLists.v) + 8))[k];
            const UC a = BRest2e(::bof3::Img(kArmourA.v) + 26u * id);
            const UC type = BRest2e(::bof3::Img(kArmourType.v) + 26u * id);
            const UC b = BRest2e(::bof3::Img(kArmourB.v) + 26u * id);
            if (static_cast<int>(type) - 2 != static_cast<int>(j)) continue;
            const U mask = SH_CALL(Item_EquipMask)(2, id);
            if ((bit & mask) == 0) continue;
            bool take;
            if (BRest2e(slot) == 0)
                take = true;
            else if (best_a > a)
                take = true;
            else if (best_a != a)
                take = false;
            else if (best_b < b)
                take = true;
            else if (best_b != b)
                take = false;
            else
                take = BRest2e(::bof3::Img(kPreview.v)) > id;   // the weapon's byte, not this slot's (Capcom's)
            if (!take) continue;
            const UC kept = BRest2e(::bof3::Img(kArmourA.v) + 26u * id);
            BRest2e(slot) = id;
            best_a = kept;
            best_b = kept;   // +0x13, not +0x14 (Capcom's)
        }
    }
    BRest2e(::bof3::Img(kPreview.v) + 4) = record[0x16];
    BRest2e(::bof3::Img(kPreview.v) + 5) = record[0x17];
}

// original 0x58D570: the preview swapped into the member's six slots through
// the inventory.
extern "C" void __cdecl FieldEquip_ApplyPreview(void) {
    unsigned char* const record = Record(PartyRecord(BRest2e(::bof3::Img(0x803340))));
    for (U i = 0; i < 6; ++i) {
        const UC item = BRest2e(::bof3::Img(kPreview.v) + i);
        if (item == 0) continue;
        unsigned char* const cell = record + 0x12 + i;
        if (item == *cell) continue;
        const UC category = BRest2e(AddrOf(FieldEquip_SlotCategories) + i);
        SH_CALL(Inventory_Remove)(category, item, 1);
        const UC old = *cell;
        const UC again = BRest2e(AddrOf(FieldEquip_SlotCategories) + i);
        SH_CALL(Inventory_Add)(again, old, 1);
        *cell = BRest2e(::bof3::Img(kPreview.v) + i);
    }
    SH_CALL(Char_RecalcStats)(record);
}

// original 0x58D640: the preview with the chosen item in the slot.
extern "C" void __cdecl FieldEquip_PreviewItem(void) {
    const UC item = BRest2e(::bof3::Img(0x803365));
    const UC index = PartyRecord(BRest2e(::bof3::Img(0x803340)));
    const UC category = BRest2e(::bof3::Img(0x803360));
    const U mask = SH_CALL(Item_EquipMask)(category, item);
    BRest2e(::bof3::Img(0x803341)) = (Bit8(index) & mask) == 0 ? 1 : 0;
    const unsigned char* const record = Record(index);
    BRest2e(::bof3::Img(kPreview.v) + 1) = record[0x13];
    BRest2e(::bof3::Img(kPreview.v)) = record[0x12];
    BRest2e(::bof3::Img(kPreview.v) + 3) = record[0x15];
    BRest2e(::bof3::Img(kPreview.v) + 2) = record[0x14];
    BRest2e(::bof3::Img(kPreview.v) + 4) = record[0x16];
    const U slot = BRest2e(::bof3::Img(0x80333E));
    BRest2e(::bof3::Img(kPreview.v) + 5) = record[0x17];
    BRest2e(::bof3::Img(kPreview.v) + slot) = item;
}

// original 0x58D700: the preview with the slot's item taken off; al 1 when
// there was one.
extern "C" unsigned char __cdecl FieldEquip_PreviewRemove(void) {
    const unsigned char* const record = Record(PartyRecord(BRest2e(::bof3::Img(0x803340))));
    BRest2e(::bof3::Img(kPreview.v)) = record[0x12];
    BRest2e(::bof3::Img(kPreview.v) + 1) = record[0x13];
    BRest2e(::bof3::Img(kPreview.v) + 2) = record[0x14];
    BRest2e(::bof3::Img(kPreview.v) + 3) = record[0x15];
    BRest2e(::bof3::Img(kPreview.v) + 5) = record[0x17];
    const U slot = BRest2e(::bof3::Img(0x80333E));
    BRest2e(::bof3::Img(kPreview.v) + 4) = record[0x16];
    if (BRest2e(::bof3::Img(kPreview.v) + slot) != 0) {
        BRest2e(::bof3::Img(kPreview.v) + slot) = 0;
        BRest2e(::bof3::Img(0x803341)) = 0;
        return 1;
    }
    BRest2e(::bof3::Img(0x803341)) = 1;
    return 0;
}

// original 0x58D7B0: two of the Equipment screen's windows closed.
extern "C" void __cdecl FieldEquip_CloseWindows(void) {
    BRest2e(::bof3::Img(0x803334)) = 0;
    BRest2e(::bof3::Img(0x803358)) = 0;
}

// original 0x58D7C0: FieldMenu_States[3], a tail jump through
// FieldAbility_States by the state byte, unchecked.
extern "C" void __cdecl FieldAbility_Run(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(Entry(AddrOf(FieldAbility_States), 10, FieldS_State, "FieldAbility_Run"))();
}

// original 0x58D7D0: FieldAbility_States[0].
extern "C" void __cdecl FieldAbility_Open(void) {
    Backdrop();
    SH_AT(Handler, rest_2e::at::kAbilityWindows.v)();
    Sound(0x102);
    const auto state = static_cast<UC>(FieldS_State + 1);
    const UC member = BRest2e(::bof3::Img(0x905BA1));
    BRest2e(::bof3::Img(kTimer.v)) = 5;
    BattleActorCopies_ItemActor = member;
    Rest2e_AbilityCursor = 0;
    FieldS_State = state;
}

namespace {

// DIV-0224 LevelSkillCatchUp: one id of a level row filed in record member's
// list, as the hack's leaf routine does it (US 0x801E34E4): an id of 0, or of
// type 3 (the ability's type byte & 3, the byte AbilityList_ForType reads),
// is skipped; an id already in the list is left; else the first 0 slot of the
// ten takes it. Not AbilityList_Add: that one files duplicates and type 3.
bool CatchUpSkill(unsigned member, unsigned char id) {
    if (id == 0) return false;
    const unsigned type = move_script::At(::bof3::Img(bof3::addr::Ability_Records) + id * 24u)[0x11] & 3u;
    if (type == 3) return false;
    unsigned char* const list = SH_CALL(AbilityList_ForType)(member, id, 0);
    unsigned char* free_slot = nullptr;
    for (unsigned i = 0; i < 10; ++i) {
        if (list[i] == id) return false;
        if (list[i] == 0 && free_slot == nullptr) free_slot = list + i;
    }
    if (free_slot == nullptr) return false;
    *free_slot = id;
    return true;
}

// DIV-0224 LevelSkillCatchUp: records 0..6 of CharacterRecords (not 7, joined
// or not), each one's level rows 0..level - 1 of Char_ExpTable (99 rows of 8
// a record, the rows Char_LevelUp reads, and row 0, which it never applies),
// the two ability bytes +6 and +7 of each row through CatchUpSkill (US
// 0x801E342C); the count filed.
unsigned CatchUpLevelSkills() {
    const unsigned char* const table = move_script::At(::bof3::Img(bof3::addr::Char_ExpTable));
    unsigned added = 0;
    for (unsigned member = 0; member < 7; ++member) {
        const unsigned level = CharacterRecords[member].level;
        const unsigned char* const rows = table + member * 99u * 8u;
        for (unsigned row = 0; row < level; ++row) {
            if (CatchUpSkill(member, rows[row * 8 + 6])) ++added;
            if (CatchUpSkill(member, rows[row * 8 + 7])) ++added;
        }
    }
    return added;
}

}  // namespace

// original 0x58D820: FieldAbility_States[2] - the four-entry menu.
//
// DIV-0224 LevelSkillCatchUp (off by default): every frame, after the cursor
// and before the confirm test, Select (Input_Pressed bit 0x100, a pad button,
// not a Config line, so no Sigma) files the level rows' skills each character
// is past (CatchUpLevelSkills) and plays 0x10C if any was filed, else 0x104;
// the menu then goes on to its confirm and cancel tests as before. The
// Definitive Edition romhack's change; evidence: its patched US disc, START.EMI
// 8 = STATUS.EMI 0, this function's twin 0x801D7314, 0x801D7444 made a jump to
// new code at 0x801E33E8 that joins 0x801D744C.
extern "C" void __cdecl FieldAbility_TopMenu(void) {
    Backdrop();
    const int c = static_cast<signed char>(Rest2e_AbilityCursor);
    const U help = WRest2e(static_cast<U>(static_cast<int>(AddrOf(FieldAbility_TopHelp)) + 4 * c));
    const U x = LRest2e(::bof3::Img(0x803314)) + static_cast<U>(48 * c);
    SetWRest2e(::bof3::Img(0x8032FC), help);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0xA000;
    BRest2e(::bof3::Img(0x8033A8)) = 0;
    const U y = WRest2e(::bof3::Img(0x803316)) + 4;
    WindowRecords[21].in_use = 1;
    SetWRest2e(::bof3::Img(0x803458), x);
    SetWRest2e(::bof3::Img(0x80345A), y);
    BRest2e(::bof3::Img(0x80331B)) = 0xFF;
    const U repeat = AutoRepeat(pressed);
    const UC was = Rest2e_AbilityCursor;
    UC cl = TurnFour(::bof3::Img(Rest2e_AbilityCursor_addr), repeat);
    if (was != cl) {
        Sound(0x101);
        cl = Rest2e_AbilityCursor;
    }
    if (bof3::BehaviourOn(bof3::behaviours::Id::LevelSkillCatchUp) && (static_cast<std::uint32_t>(Input_Pressed) & 0x100u) != 0) {
        Sound(CatchUpLevelSkills() != 0 ? 0x10C : 0x104);
    }
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        BRest2e(::bof3::Img(0x80331B)) = cl;
        Sound(0x104);
        const UC chosen = Rest2e_AbilityCursor;
        if (chosen == 1) {
            const auto state = static_cast<UC>(FieldS_State + 5);
            FieldS_Step = 0;
            FieldS_State = state;
            return;
        }
        if (chosen == 3) {
            const auto state = static_cast<UC>(FieldS_State + 7);
            WindowRecords[16].state = 1;
            BRest2e(::bof3::Img(kTimer.v)) = 7;
            FieldS_State = state;
            FieldS_Step = 0;
            return;
        }
        if (chosen == 2) Window16_Page = 3;
        FieldS_State = static_cast<UC>(FieldS_State + 1);
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x102);
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State + 6);
    BRest2e(::bof3::Img(0x8032EF)) = 1;
    BRest2e(::bof3::Img(0x803313)) = 1;
    WindowRecords[16].state = 1;
    WindowRecords[21].in_use = 0;
    FieldS_State = state;
    BRest2e(::bof3::Img(kTimer.v)) = 5;
}

// original 0x58D9C0: FieldAbility_States[3] - the member and the list type.
extern "C" void __cdecl FieldAbility_PickMember(void) {
    Backdrop();
    const UC cursor = Rest2e_AbilityCursor;
    const UC window = BRest2e(::bof3::Img(0x8031F3));
    WindowRecords[21].in_use = 0;
    BRest2e(::bof3::Img(0x8033A8)) = cursor != 0 ? 3 : 1;
    const UC member = Window16_Owner;
    if (window == 0) {
        const U wx = WRest2e(::bof3::Img(0x8031F4));
        WindowRecords[19].in_use = 1;
        BRest2e(::bof3::Img(0x803417)) = 1;
        SetWRest2e(::bof3::Img(0x803410), wx);
        WindowRecords[19].y = 54u * member + WRest2e(::bof3::Img(0x8031F6));
    } else {
        WindowRecords[19].in_use = 0;
    }
    SH_CALL(TextRecord_Set)(0, 8, Record(PartyRecord(member)));
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0xF000;
    SetWRest2e(::bof3::Img(0x8032FC), 0x2F);
    const U repeat = AutoRepeat(pressed);
    BRest2e(::bof3::Img(0x8033AC)) = 0xFF;
    BRest2e(::bof3::Img(0x8033AD)) = 0xFF;
    const UC n = PartyCountRest2e();
    const UC was = Window16_Owner;
    MoveAbilityMember(repeat, n);
    if (Rest2e_AbilityCursor == 2) {
        SetWRest2e(::bof3::Img(0x8033B0), 1);
    } else {
        SetWRest2e(::bof3::Img(0x8033B0), 0);
        UC cl;
        TurnType(repeat, cl);
    }
    if (Window16_Owner != S8Rest2e(was)) Sound(0x101);
    const U p = Input_Pressed;
    const UC now = Window16_Owner;
    BattleActorCopies_ItemActor = now;
    if (Confirmed(p)) {
        Sound(0x104);
        const int m = BattleActorCopies_ItemActor;
        const U type = Window16_Page;
        const UC kept = BRest2e(::bof3::Img(kAbilityCursors.v) + type + 4 * m);
        const auto state = static_cast<UC>(FieldS_State + 1);
        BRest2e(::bof3::Img(0x8033AC)) = kept;
        FieldS_State = state;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State - 1);
    WindowRecords[19].in_use = 0;
    FieldS_State = state;
}

// original 0x58DC40: FieldAbility_States[4] - the ability.
extern "C" void __cdecl FieldAbility_PickAbility(void) {
    Backdrop();
    const UC window = BRest2e(::bof3::Img(0x8031F3));
    const UC member = Window16_Owner;
    if (window == 0) {
        const U wx = WRest2e(::bof3::Img(0x8031F4));
        WindowRecords[19].in_use = 1;
        BRest2e(::bof3::Img(0x803417)) = 0;
        SetWRest2e(::bof3::Img(0x803410), wx);
        WindowRecords[19].y = 54u * member + WRest2e(::bof3::Img(0x8031F6));
    } else {
        WindowRecords[19].in_use = 0;
    }
    const UC type0 = Window16_Page;
    const unsigned char* const list = AbilityList(member, type0);
    const UC id = list[BRest2e(::bof3::Img(0x8033AC))];
    const U help = WRest2e(::bof3::Img(kAbilityHelp.v) + 24u * id);
    const UC busy = WindowRecords[16].state;
    SetWRest2e(::bof3::Img(0x8032FC), help);
    if (busy == 0) {
        const U cur = BRest2e(::bof3::Img(0x8033AC));
        const U x = LRest2e(::bof3::Img(0x8033A4)) + 7;
        WindowRecords[21].in_use = 1;
        SetWRest2e(::bof3::Img(0x803458), x);
        WindowRecords[21].y = 13u * (cur + 2) + static_cast<std::uint32_t>(WindowRecords[16].y);
    } else {
        WindowRecords[21].in_use = 0;
    }
    const U repeat = AutoRepeat(static_cast<std::uint32_t>(Input_Pressed) & 0xF000);
    UC al, cl;
    bool turned = false;
    if (Rest2e_AbilityCursor != 2) turned = TurnType(repeat, cl);
    if (turned) {
        const int m = BattleActorCopies_ItemActor;
        al = BRest2e(::bof3::Img(kAbilityCursors.v) + cl + 4 * m);
        BRest2e(::bof3::Img(0x8033AC)) = al;
    } else {
        al = BRest2e(::bof3::Img(0x8033AC));
        cl = Window16_Page;
    }
    const UC was = al;
    if (repeat & 0x1000) {
        if (al > 0) {
            --al;
            BRest2e(::bof3::Img(0x8033AC)) = al;
        }
    } else if (repeat & 0x4000) {
        if (al < 9) {
            ++al;
            BRest2e(::bof3::Img(0x8033AC)) = al;
        }
    }
    if (was != al) {
        Sound(0x100);
        al = BRest2e(::bof3::Img(0x8033AC));
        cl = Window16_Page;
    }
    {
        const int m = BattleActorCopies_ItemActor;
        BRest2e(::bof3::Img(kAbilityCursors.v) + cl + 4 * m) = al;
    }
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        const unsigned char* const chosen = AbilityList(Window16_Owner, cl);
        const UC ability = chosen[BRest2e(::bof3::Img(0x8033AC))];
        if (ability == 0) {
            Sound(0x107);
            return;
        }
        if (Rest2e_AbilityCursor != 0) {
            const UC mode = BRest2e(::bof3::Img(0x8033A8));
            const UC user = Window16_Owner;
            const UC can = SH_CALL(Skill_CanUse)(mode, user, ability);
            if (can == 0) {
                Sound(0x107);
                return;
            }
            const auto state = static_cast<UC>(FieldS_State + 2);
            const UC kept = BRest2e(::bof3::Img(0x8033AC));
            BRest2e(::bof3::Img(0x8033AD)) = kept;
            FieldS_Answer = 0;
            FieldS_State = state;
            return;
        }
        {
            const UC mode = BRest2e(::bof3::Img(0x8033A8));
            const UC user = Window16_Owner;
            const UC can = SH_CALL(Skill_CanUse)(mode, user, ability);
            if (can == 0) {
                Sound(0x107);
                return;
            }
        }
        if ((BRest2e(::bof3::Img(kAbilityFlags.v) + 24u * ability) & 0x10) != 0) {
            const unsigned char* const again = AbilityList(Window16_Owner, Window16_Page);
            const UC used = again[BRest2e(::bof3::Img(0x8033AC))];
            const UC user = PartyRecord(Window16_Owner);
            const UC answer = AbilityUse(user, 0, used);
            Rest2e_AbilityResult = answer;
            Sound(answer == 1 || answer == 5 ? 0x109 : 0x107);
            return;
        }
        Sound(0x103);
        UC k = 0;
        if (PartyCountRest2e() != 0) {
            do {
                const U o = 36u * k;
                BRest2e(::bof3::Img(0x80333E) + o) = k;
                const U y = 54u * k + 0x3E;
                ++k;
                BRest2e(::bof3::Img(0x803335) + o) = 6;
                BRest2e(::bof3::Img(0x803334) + o) = 1;
                BRest2e(::bof3::Img(0x803336) + o) = 0;
                BRest2e(::bof3::Img(0x803337) + o) = 0xA;
                BRest2e(::bof3::Img(0x80333F) + o) = 0;
                BRest2e(::bof3::Img(0x803340) + o) = 0;
                SetWRest2e(::bof3::Img(0x803338) + o, 0x140);
                SetWRest2e(::bof3::Img(0x80333A) + o, y);
                BRest2e(::bof3::Img(0x8031F3) + o) = 7;
            } while (k < PartyCountRest2e());
        }
        const UC kept = BRest2e(::bof3::Img(0x8033AC));
        BRest2e(::bof3::Img(0x8033AD)) = kept;
        const auto state = static_cast<UC>(FieldS_State + 1);
        WindowRecords[16].state = 4;
        WindowRecords[21].in_use = 1;
        WindowRecords[19].in_use = 0;
        FieldS_Column = 0;
        FieldS_State = state;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State - 1);
    BRest2e(::bof3::Img(0x8033AC)) = 0xFF;
    BRest2e(::bof3::Img(0x8033AD)) = 0xFF;
    FieldS_State = state;
}

// original 0x58E070: FieldAbility_States[5] - the target.
extern "C" void __cdecl FieldAbility_PickTarget(void) {
    Backdrop();
    const UC window = BRest2e(::bof3::Img(0x803337));
    WindowRecords[21].in_use = 0;
    if (window == 0) {
        const int t = FieldS_Column;
        const U wx = WRest2e(::bof3::Img(0x803338));
        WindowRecords[19].in_use = 1;
        BRest2e(::bof3::Img(0x803417)) = 1;
        SetWRest2e(::bof3::Img(0x803410), wx);
        WindowRecords[19].y = static_cast<U>(54 * t) + WRest2e(::bof3::Img(0x80333A));
    } else {
        WindowRecords[19].in_use = 0;
    }
    const U repeat = AutoRepeat(static_cast<std::uint32_t>(Input_Pressed) & 0xF000);
    const UC n = PartyCountRest2e();
    const UC was = static_cast<unsigned char>(FieldS_Column);
    const UC cl = MoveMemberS8(::bof3::Img(FieldS_Column_addr), repeat, n);
    if (was != cl) Sound(0x101);
    const unsigned char* const list = AbilityList(Window16_Owner, Window16_Page);
    const UC ability = list[BRest2e(::bof3::Img(0x8033AC))];
    SH_CALL(TextRecord_Set)(0, 0x10, AtRest2e(::bof3::Img(kAbilityRecords.v) + 24u * ability));
    const U p = Input_Pressed;
    SetWRest2e(::bof3::Img(0x8032FC), 0x30);
    if (Confirmed(p)) {
        const unsigned char* const again = AbilityList(Window16_Owner, Window16_Page);
        const UC used = again[BRest2e(::bof3::Img(0x8033AC))];
        const UC target = PartyRecord(FieldS_Column);
        const UC user = PartyRecord(Window16_Owner);
        const UC answer = AbilityUse(user, target, used);
        Rest2e_AbilityResult = answer;
        Sound(answer == 1 || answer == 5 ? 0x109 : 0x107);
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    Sound(0x102);
    UC k = 0;
    WindowRecords[19].in_use = 0;
    if (PartyCountRest2e() != 0) {
        do {
            const U o = 36u * k;
            ++k;
            BRest2e(::bof3::Img(0x803337) + o) = 9;
            BRest2e(::bof3::Img(0x8031F3) + o) = 8;
        } while (k < PartyCountRest2e());
    }
    const auto state = static_cast<UC>(FieldS_State - 1);
    WindowRecords[16].state = 3;
    BRest2e(::bof3::Img(0x8033AD)) = 0xFF;
    FieldS_State = state;
}

// original 0x58E2C0: FieldAbility_States[6] - the yes / no to add the ability
// to the shared list.
extern "C" void __cdecl FieldAbility_ShareConfirm(void) {
    Backdrop();
    const int answer = FieldS_Answer;
    const U x = LRest2e(::bof3::Img(0x8032F0)) - static_cast<U>(36 * answer) + 0xE8;
    WindowRecords[21].y = WRest2e(::bof3::Img(0x8032F2)) + 5;
    U pressed = LRest2e(::bof3::Img(kPressed.v));
    WindowRecords[21].in_use = 1;
    SetWRest2e(::bof3::Img(0x803458), x);
    SetWRest2e(::bof3::Img(0x8032FC), 0x31);
    if (pressed & 0xA000) {
        Sound(0x101);
        FieldS_Answer = static_cast<UC>(static_cast<unsigned char>(FieldS_Answer) ^ 1);
        pressed = LRest2e(::bof3::Img(kPressed.v));
    }
    if (Confirmed(pressed)) {
        if (static_cast<unsigned char>(FieldS_Answer) != 0) {
            Sound(0x104);
            const unsigned char* const list = AbilityList(Window16_Owner, Window16_Page);
            const UC ability = list[BRest2e(::bof3::Img(0x8033AC))];
            SH_CALL(AbilityList_Add)(ability, 0, 1, 0);
            unsigned char* const again = const_cast<unsigned char*>(AbilityList(Window16_Owner, Window16_Page));
            again[BRest2e(::bof3::Img(0x8033AC))] = 0;
        } else {
            Sound(0x106);
        }
        const auto state = static_cast<UC>(FieldS_State - 2);
        BRest2e(::bof3::Img(0x8033AD)) = 0xFF;
        FieldS_State = state;
        return;
    }
    if (!Cancelled(pressed)) return;
    const auto state = static_cast<UC>(FieldS_State - 2);
    BRest2e(::bof3::Img(0x8033AD)) = 0xFF;
    FieldS_State = state;
    Sound(0x106);
}

// original 0x58E410: FieldAbility_States[7], a tail jump through
// FieldAbility_ArrangeSteps by the step byte, unchecked.
extern "C" void __cdecl FieldAbility_ArrangeRun(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(
        Entry(AddrOf(FieldAbility_ArrangeSteps), 3, FieldS_Step, "FieldAbility_ArrangeRun"))();
}

// original 0x58E420: FieldAbility_ArrangeSteps[0] - the member and the type.
extern "C" void __cdecl FieldAbility_ArrangeMember(void) {
    Backdrop();
    const U member = Window16_Owner;
    const U wx = WRest2e(::bof3::Img(0x8031F4));
    SetWRest2e(::bof3::Img(0x8032FC), 0x17);
    SetWRest2e(::bof3::Img(0x803410), wx);
    const U hand_y = WindowRecords[16].y;
    WindowRecords[19].in_use = 1;
    const U x = LRest2e(::bof3::Img(0x8033A4)) + 0x2A;
    BRest2e(::bof3::Img(0x803417)) = 1;
    WindowRecords[19].y = 54u * member + WRest2e(::bof3::Img(0x8031F6));
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0xF00C);
    WindowRecords[21].in_use = 1;
    SetWRest2e(::bof3::Img(0x803458), x);
    WindowRecords[21].y = hand_y + 7;
    const U repeat = AutoRepeat(pressed);
    BRest2e(::bof3::Img(0x8033AD)) = 0xFF;
    const UC n = PartyCountRest2e();
    MoveAbilityMember(repeat, n);
    UC cl;
    TurnType(repeat, cl);
    BattleActorCopies_ItemActor = Window16_Owner;
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        Sound(0x103);
        const UC three = Window16_Page == 3 ? 1 : 0;
        const auto step = static_cast<UC>(FieldS_Step + 1);
        WindowRecords[19].in_use = 0;
        BRest2e(::bof3::Img(0x8033E9)) = 6;
        BRest2e(::bof3::Img(0x8033EA)) = 0xF;
        WindowRecords[18].in_use = 1;
        SetWRest2e(::bof3::Img(0x8033EC), 0x28);
        SetWRest2e(::bof3::Img(0x8033EE), 0x3C);
        BRest2e(::bof3::Img(0x8033F2)) = static_cast<UC>(three + 5);
        BRest2e(::bof3::Img(0x8033F3)) = 0;
        FieldS_Step = step;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State - 5);
    WindowRecords[19].in_use = 0;
    FieldS_State = state;
}

// original 0x58E640: FieldAbility_ArrangeSteps[1] - the sort, a row of
// FieldAbility_ArrangeRows.
extern "C" void __cdecl FieldAbility_ArrangeHow(void) {
    Backdrop();
    const U row = BRest2e(::bof3::Img(0x8033F3));
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    const U x = LRest2e(::bof3::Img(0x8033EC)) + 5;
    const U y = (row << 4) + WRest2e(::bof3::Img(0x8033EE)) + 0x1A;
    SetWRest2e(::bof3::Img(0x8032FC), 0x18);
    BRest2e(::bof3::Img(0x8033F4)) = 0xFF;
    WindowRecords[21].in_use = 1;
    SetWRest2e(::bof3::Img(0x803458), x);
    SetWRest2e(::bof3::Img(0x80345A), y);
    BRest2e(::bof3::Img(0x8033AC)) = 0xFF;
    const U repeat = AutoRepeat(pressed);
    const U which = Window16_Page == 3 ? 1 : 0;
    const U rows = AddrOf(FieldAbility_ArrangeRows) + 6 * which;
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto c = static_cast<UC>(BRest2e(::bof3::Img(0x8033F3)) - 1);
        BRest2e(::bof3::Img(0x8033F3)) = c;
        if (S8Rest2e(c) < 0) BRest2e(::bof3::Img(0x8033F3)) = static_cast<UC>(BRest2e(rows) - 1);
    } else if (repeat & 0x4000) {
        Sound(0x100);
        const auto c = static_cast<UC>(BRest2e(::bof3::Img(0x8033F3)) + 1);
        BRest2e(::bof3::Img(0x8033F3)) = c;
        if (static_cast<int>(c) > static_cast<int>(BRest2e(rows)) - 1) BRest2e(::bof3::Img(0x8033F3)) = 0;
    }
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        Sound(0x103);
        const UC how = BRest2e(rows + 1 + BRest2e(::bof3::Img(0x8033F3)));
        if (how != 0) {
            SH_AT(void (__cdecl*)(unsigned), rest_2e::at::kAbilitySort.v)(how);
            return;
        }
        const int m = BattleActorCopies_ItemActor;
        BRest2e(::bof3::Img(0x8033AD)) = 0xFF;
        const U type = Window16_Page;
        const UC kept = BRest2e(::bof3::Img(kAbilityCursors.v) + type + 4 * m);
        const auto step = static_cast<UC>(FieldS_Step + 1);
        BRest2e(::bof3::Img(0x8033AC)) = kept;
        FieldS_Step = step;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto step = static_cast<UC>(FieldS_Step - 1);
    WindowRecords[18].in_use = 0;
    FieldS_Step = step;
}

// original 0x58E7E0: FieldAbility_ArrangeSteps[2] - an ability picked and
// swapped with another.
extern "C" void __cdecl FieldAbility_ArrangeMove(void) {
    Backdrop();
    const U cur = BRest2e(::bof3::Img(0x8033AC));
    const U x = LRest2e(::bof3::Img(0x8033A4)) + 7;
    SetWRest2e(::bof3::Img(0x803458), x);
    const UC type = Window16_Page;
    BRest2e(::bof3::Img(0x8033F4)) = 0;
    const UC member = Window16_Owner;
    WindowRecords[21].in_use = 1;
    WindowRecords[21].y = 13u * (cur + 2) + static_cast<std::uint32_t>(WindowRecords[16].y);
    const unsigned char* const list = AbilityList(member, type);
    const UC ability = list[BRest2e(::bof3::Img(0x8033AC))];
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0x500C);
    SetWRest2e(::bof3::Img(0x8032FC), WRest2e(::bof3::Img(kAbilityHelp.v) + 24u * ability));
    const U repeat = AutoRepeat(pressed);
    UC cl = BRest2e(::bof3::Img(0x8033AC));
    const UC was = cl;
    if (repeat & 0x1000) {
        if (cl > 0) {
            --cl;
            BRest2e(::bof3::Img(0x8033AC)) = cl;
        }
    } else if (repeat & 0x4000) {
        if (cl < 9) {
            ++cl;
            BRest2e(::bof3::Img(0x8033AC)) = cl;
        }
    }
    {
        const int m = BattleActorCopies_ItemActor;
        const U t = Window16_Page;
        BRest2e(::bof3::Img(kAbilityCursors.v) + t + 4 * m) = cl;
    }
    if (was != cl) Sound(0x100);
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        if (BRest2e(::bof3::Img(0x8033AD)) == 0xFF) {
            const unsigned char* const again = AbilityList(Window16_Owner, Window16_Page);
            if (again[BRest2e(::bof3::Img(0x8033AC))] != 0) {
                Sound(0x103);
                BRest2e(::bof3::Img(0x8033AD)) = BRest2e(::bof3::Img(0x8033AC));
                return;
            }
            Sound(0x107);
            return;
        }
        Sound(0x103);
        const UC type1 = Window16_Page;
        const UC member1 = Window16_Owner;
        const unsigned char* const at_cursor = AbilityList(member1, type1);
        unsigned char* const first = const_cast<unsigned char*>(at_cursor) + BRest2e(::bof3::Img(0x8033AC));
        const UC member2 = Window16_Owner;
        const UC type2 = Window16_Page;
        const unsigned char* const at_pick = AbilityList(member2, type2);
        unsigned char* const second = const_cast<unsigned char*>(at_pick) + BRest2e(::bof3::Img(0x8033AD));
        Swap(second, first);
        BRest2e(::bof3::Img(0x8033AD)) = 0xFF;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    if (BRest2e(::bof3::Img(0x8033AD)) == 0xFF) {
        FieldS_Step = static_cast<UC>(FieldS_Step - 1);
        return;
    }
    BRest2e(::bof3::Img(0x8033AD)) = 0xFF;
}

// original 0x58E9D0: FieldAbility_States[8] - the screen closed.
extern "C" void __cdecl FieldAbility_Close(void) {
    Backdrop();
    const auto t = static_cast<UC>(BRest2e(::bof3::Img(kTimer.v)) - 1);
    BRest2e(::bof3::Img(kTimer.v)) = t;
    if (t != 0) return;
    SH_AT(Handler, rest_2e::at::kAbilityCloseWindows.v)();
    U k = 0;
    if (PartyCountRest2e() != 0) {
        do {
            BRest2e(::bof3::Img(0x8031F3) + 0x24u * k) = 4;
            ++k;
        } while (static_cast<int>(k) < static_cast<int>(PartyCountRest2e()));
    }
    const UC m = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    BRest2e(::bof3::Img(0x80325F)) = 2;
    BRest2e(::bof3::Img(0x803283)) = 2;
    BRest2e(::bof3::Img(0x8032A7)) = 2;
    BRest2e(::bof3::Img(0x8032CB)) = 2;
    BRest2e(::bof3::Img(0x905BA1)) = m;
    BRest2e(::bof3::Img(kTimer.v)) = 0;
    BattleE1_Mode = 1;
    FieldS_State = 0;
}

// original 0x58EA60: FieldAbility_States[9], a tail jump through
// FieldAbility_ViewSteps by the step byte, unchecked.
extern "C" void __cdecl FieldAbility_ViewRun(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(
        Entry(AddrOf(FieldAbility_ViewSteps), 5, FieldS_Step, "FieldAbility_ViewRun"))();
}

// original 0x58EA70: FieldAbility_ViewSteps[0].
extern "C" void __cdecl FieldAbility_ViewOpen(void) {
    Backdrop();
    const auto t = static_cast<UC>(BRest2e(::bof3::Img(kTimer.v)) - 1);
    BRest2e(::bof3::Img(kTimer.v)) = t;
    if (t != 0) return;
    SH_AT(Handler, rest_2e::at::kAbilityViewWindow.v)();
    const UC cursor = Rest2e_AbilityViewCursor;
    const UC top = Rest2e_AbilityViewTop;
    BRest2e(::bof3::Img(0x8033CF)) = cursor;
    BRest2e(::bof3::Img(0x8033CE)) = top;
    Sound(0x102);
    const auto step = static_cast<UC>(FieldS_Step + 1);
    BRest2e(::bof3::Img(kTimer.v)) = 7;
    FieldS_Step = step;
}

// original 0x58EAD0: FieldAbility_ViewSteps[1].
extern "C" void __cdecl FieldAbility_ViewWait(void) {
    Backdrop();
    const auto t = static_cast<UC>(BRest2e(::bof3::Img(kTimer.v)) - 1);
    BRest2e(::bof3::Img(kTimer.v)) = t;
    if (t == 0) FieldS_Step = static_cast<UC>(FieldS_Step + 1);
}

// original 0x58EB00: FieldAbility_ViewSteps[2] - the list scrolled.
extern "C" void __cdecl FieldAbility_ViewBrowse(void) {
    Backdrop();
    const UC h = BRest2e(::bof3::Img(0x8033D0));
    SetWRest2e(::bof3::Img(0x8032FC), h != 0 ? h + 0x4183u : 0);
    const U top = BRest2e(::bof3::Img(0x8033CE));
    const U cur = BRest2e(::bof3::Img(0x8033CF));
    const U x = LRest2e(::bof3::Img(0x8033C8)) + 7;
    SetWRest2e(::bof3::Img(0x803458), x);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0x500C);
    WindowRecords[21].in_use = 1;
    WindowRecords[21].y = 13u * (cur - top + 2) + WRest2e(::bof3::Img(0x8033CA));
    const U repeat = AutoRepeat(pressed);
    const UC was = BRest2e(::bof3::Img(0x8033CF));
    UC cl, al;
    Scroll(repeat, kViewScroll, cl, al);
    Rest2e_AbilityViewCursor = cl;
    Rest2e_AbilityViewTop = al;
    if (was != cl) Sound(0x100);
    if (!Cancelled(static_cast<std::uint32_t>(Input_Pressed))) return;
    Sound(0x106);
    const auto step = static_cast<UC>(FieldS_Step + 1);
    WindowRecords[21].in_use = 0;
    BRest2e(::bof3::Img(0x8033C7)) = 1;
    BRest2e(::bof3::Img(kTimer.v)) = 7;
    FieldS_Step = step;
}

// original 0x58ECC0: FieldAbility_ViewSteps[3].
extern "C" void __cdecl FieldAbility_ViewLeave(void) {
    Backdrop();
    const auto t = static_cast<UC>(BRest2e(::bof3::Img(kTimer.v)) - 1);
    BRest2e(::bof3::Img(kTimer.v)) = t;
    if (t != 0) return;
    WindowRecords[16].state = 2;
    Sound(0x102);
    const auto step = static_cast<UC>(FieldS_Step + 1);
    BRest2e(::bof3::Img(kTimer.v)) = 7;
    FieldS_Step = step;
}

// original 0x58ED10: FieldAbility_ViewSteps[4].
extern "C" void __cdecl FieldAbility_ViewEnd(void) {
    Backdrop();
    const auto t = static_cast<UC>(BRest2e(::bof3::Img(kTimer.v)) - 1);
    BRest2e(::bof3::Img(kTimer.v)) = t;
    if (t != 0) return;
    const UC state = FieldS_State;
    FieldS_Step = 0;
    FieldS_State = static_cast<UC>(state - 7);
}

// original 0x58ED40: called by 0x58D7D0 (the Ability screen's step 0, R2E):
// window records 11 (kind 9), 12 (kind 0xD), 16 (kind 0x11, slid in from x
// 0x140) set up and opened; 19 and 21 set up, closed.
extern "C" void __cdecl AbilityMenu_InitRecords(void) {
    BRest2f(Rec(11, 3)) = 2;
    BRest2f(Rec(12, 3)) = 2;
    BRest2f(Rec(12, 0xA)) = 2;
    BRest2f(Rec(16, 3)) = 2;
    BRest2f(Rec(11, 1)) = 6;
    BRest2f(Rec(11, 2)) = 9;
    BRest2f(Rec(11, 0)) = 1;
    SetWRest2f(Rec(11, 4), 0x14);
    SetWRest2f(Rec(11, 6), 0xFFEC);
    SetWRest2f(Rec(11, 0x10), 0);
    BRest2f(Rec(12, 1)) = 6;
    BRest2f(Rec(12, 2)) = 0xD;
    BRest2f(Rec(12, 0)) = 1;
    SetWRest2f(Rec(12, 4), 0x40);
    SetWRest2f(Rec(12, 6), 0xFFEC);
    BRest2f(Rec(12, 0xB)) = 0xFF;
    BRest2f(Rec(16, 1)) = 6;
    BRest2f(Rec(16, 2)) = 0x11;
    BRest2f(Rec(16, 0)) = 1;
    SetWRest2f(Rec(16, 4), 0x140);
    SetWRest2f(Rec(16, 6), 0x3E);
    BRest2f(Rec(16, 0xB)) = 0;
    BRest2f(Rec(16, 0xA)) = 0;
    BRest2f(Rec(16, 0xC)) = 0xFF;
    BRest2f(Rec(16, 0xD)) = 0xFF;
    BRest2f(Rec(16, 8)) = 0;
    BRest2f(Rec(16, 9)) = 0;
    SetWRest2f(Rec(16, 0x10), 0);
    BRest2f(Rec(19, 1)) = 6;
    BRest2f(Rec(19, 2)) = 0xA;
    BRest2f(Rec(19, 0)) = 0;
    BRest2f(Rec(19, 0xA)) = 1;
    BRest2f(Rec(19, 0xB)) = 1;
    BRest2f(Rec(21, 1)) = 6;
    BRest2f(Rec(21, 2)) = 0xE;
    BRest2f(Rec(21, 3)) = 0;
    BRest2f(Rec(21, 0)) = 0;
}

// original 0x58EE40: called by 0x58E640 (R2E) with a mode byte: a tail jump
// through AbilityList_SortModes by it, unchecked (ours aborts past its 3).
extern "C" void __cdecl AbilityList_SortBy(unsigned mode) {
    Run("AbilityList_SortBy (0x58EE40)", AbilityList_SortModes, AbilityList_SortModes_count, mode & 0xFF, "the mode");
}

// original 0x58EE50: AbilityList_SortModes[0] - the empty slots moved to the
// end: a pair swaps when its first byte is 0 and its second is not.
extern "C" void __cdecl AbilityList_Compact(void) {
    Bubble([](const unsigned char* p) { return p[0] == 0 && p[1] != 0; });
}

// original 0x58EEC0: AbilityList_SortModes[1] - compacted (AbilityList_Compact,
// called), then by the AP cost byte (Ability_Records + 0x12), highest first: a
// pair of two abilities swaps when the first costs less (unsigned).
extern "C" void __cdecl AbilityList_SortByApDown(void) {
    SH_CALL(AbilityList_Compact)();
    Bubble([](const unsigned char* p) {
        const unsigned char a = p[0];
        if (a == 0) return false;
        const unsigned char b = p[1];
        if (b == 0) return false;
        return BRest2f(::bof3::Img(kApCost.v) + 24u * a) < BRest2f(::bof3::Img(kApCost.v) + 24u * b);
    });
}

// original 0x58EF60: AbilityList_SortModes[2] - as AbilityList_SortByApDown,
// lowest first: the pair swaps when the first costs more.
extern "C" void __cdecl AbilityList_SortByApUp(void) {
    SH_CALL(AbilityList_Compact)();
    Bubble([](const unsigned char* p) {
        const unsigned char a = p[0];
        if (a == 0) return false;
        const unsigned char b = p[1];
        if (b == 0) return false;
        return BRest2f(::bof3::Img(kApCost.v) + 24u * a) > BRest2f(::bof3::Img(kApCost.v) + 24u * b);
    });
}

// original 0x58F000: called by 0x58EA70 (R2E): window record 17 (kind 0x14)
// set up at x 0x140, y 0x3E and opened, its +8, +0xA..+0xC cleared.
extern "C" void __cdecl AbilityMenu_InitRecord17(void) {
    BRest2f(Rec(17, 1)) = 6;
    BRest2f(Rec(17, 2)) = 0x14;
    BRest2f(Rec(17, 3)) = 2;
    BRest2f(Rec(17, 0)) = 1;
    SetWRest2f(Rec(17, 4), 0x140);
    SetWRest2f(Rec(17, 6), 0x3E);
    BRest2f(Rec(17, 0xB)) = 0;
    BRest2f(Rec(17, 0xA)) = 0;
    BRest2f(Rec(17, 0xC)) = 0;
    BRest2f(Rec(17, 8)) = 0;
}

// original 0x58F050: called by 0x58E9D0 (R2E): window records 13..18 freed
// (+0 = 0).
extern "C" void __cdecl FieldMenu_FreeRecords13To18(void) {
    for (unsigned n = 13; n <= 18; ++n) BRest2f(Rec(n, 0)) = 0;
}

// original 0x58F080: FieldMenu_States[5] - jmp through TacticsMenu_Steps by the
// step byte 0x929F01, unchecked (ours aborts past its 7: steps 0..6, of which 6
// is TacticsFormation_Steps[0] by the tables' layout).
extern "C" void __cdecl Tactics_Run(void) {
    Run("Tactics_Run (0x58F080)", TacticsMenu_Steps, TacticsMenu_Steps_count, FieldS_State, "0x929F01");
}

// original 0x58F090: TacticsMenu_Steps[0] - window records 11 (kind 9), 19, 20
// (kind 0xA, closed), 18 (kind 0xD: the top's two choices at x 0x70) and 21
// (kind 0xE, closed) set up; the backdrop; the top's choice 0; sound 0x102;
// the step on, the sub-step 0, the countdown 5.
extern "C" void __cdecl Tactics_Open(void) {
    BRest2f(Rec(11, 1)) = 6;
    BRest2f(Rec(11, 2)) = 9;
    BRest2f(Rec(11, 3)) = 2;
    BRest2f(Rec(11, 0)) = 1;
    SetWRest2f(Rec(11, 4), 0x14);
    SetWRest2f(Rec(11, 6), 0xFFEC);
    SetWRest2f(Rec(11, 0x10), 0);
    for (unsigned n = 19; n <= 20; ++n) {
        BRest2f(Rec(n, 1)) = 6;
        BRest2f(Rec(n, 2)) = 0xA;
        BRest2f(Rec(n, 0)) = 0;
        BRest2f(Rec(n, 0xA)) = 0;
    }
    const unsigned char kind = Config_Background;
    BRest2f(Rec(18, 1)) = 6;
    BRest2f(Rec(18, 2)) = 0xD;
    BRest2f(Rec(18, 3)) = 2;
    BRest2f(Rec(18, 0)) = 1;
    BRest2f(Rec(18, 0xA)) = 7;
    BRest2f(Rec(18, 0xB)) = 0xFF;
    SetWRest2f(Rec(18, 4), 0x70);
    SetWRest2f(Rec(18, 6), 0xFFEC);
    BRest2f(Rec(21, 1)) = 6;
    BRest2f(Rec(21, 2)) = 0xE;
    BRest2f(Rec(21, 0)) = 0;
    SH_CALL(Menu_DrawBackdrop)(kind);
    Rest2f_TopChoice = 0;
    SoundRest2f(0x102);
    const unsigned char step = FieldS_State;
    FieldS_Step = 0;
    BRest2f(::bof3::Img(kTimer.v)) = 5;
    FieldS_State = static_cast<unsigned char>(step + 1);
}

// original 0x58F170: TacticsMenu_Steps[1] - the top. Record 11's word +0x10 is
// 0x1F + the choice; while record 18 has arrived (+3 0) record 21 (the hand)
// is shown at 18's x + 0x30 * choice, y + 4. Left / right (auto-repeated, 0xA000)
// toggle the choice with sound 0x101. Confirm: choice 0 builds the formation
// grid (TacticsFormation_Build), choice 1 the reserve (TacticsMembers_Build)
// when Field_InputFlags bit 0 allows, else sound 0x107 and nothing; then
// sounds 0x102 and 0x104, record 18's +0xB the choice, the hand closed, the
// step on by the choice + 1 (2 formation, 3 members). Cancel: sounds 0x102,
// 0x106, records 11 and 18 to state 1 (out), 19..21 closed, the step on by 3
// (4, the close).
//
// TacticsClose is the cancel's stores after its sounds, as the original has
// them; DIV-0217's refused member change shares them (PSX 0x801D9A88).
namespace {
void TacticsClose() {
    const unsigned char step = FieldS_State;
    BRest2f(Rec(11, 3)) = 1;
    BRest2f(Rec(18, 3)) = 1;
    BRest2f(Rec(21, 0)) = 0;
    BRest2f(Rec(19, 0)) = 0;
    BRest2f(Rec(20, 0)) = 0;
    FieldS_State = static_cast<unsigned char>(step + 3);
    FieldS_Step = 0;
    BRest2f(::bof3::Img(kTimer.v)) = 5;
}
}  // namespace

extern "C" void __cdecl Tactics_Top(void) {
    Backdrop();
    const unsigned choice = Rest2f_TopChoice;
    SetWRest2f(Rec(11, 0x10), choice + 0x1F);
    if (BRest2f(Rec(18, 3)) == 0) {
        const U x = (choice * 0x30u + static_cast<U>(Long(move_script::At(Rec(18, 4))))) & 0xFFFF;
        const U y = (WRest2f(Rec(18, 6)) + 4u) & 0xFFFF;
        BRest2f(Rec(21, 0)) = 1;
        SetWRest2f(Rec(21, 4), x);
        SetWRest2f(Rec(21, 6), y);
    } else {
        BRest2f(Rec(21, 0)) = 0;
    }
    const U pressed = Input_Pressed;
    BRest2f(Rec(18, 0xB)) = 0xFF;
    if (Repeat(pressed & 0xA000) & 0xA000) {
        SoundRest2f(0x101);
        Rest2f_TopChoice = static_cast<unsigned char>(Rest2f_TopChoice ^ 1);
    }
    const U now = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_ConfirmButtons) & now) {
        if (Rest2f_TopChoice == 0) {
            SH_CALL(TacticsFormation_Build)();
        } else if (Field_InputFlags & 1) {
            SH_CALL(TacticsMembers_Build)();
        } else if (bof3::BehaviourOn(bof3::behaviours::Id::AllyAnywhere)) {
            // DIV-0217 AllyAnywhere. The Definitive Edition romhack (evidence:
            // the patched US disc, START.EMI 8 = STATUS.EMI 0, Tactics_Top's
            // twin 0x801D98BC, the refusal's call at 0x801D99F8 turned into a
            // jump to 0x801E3600): the refused member change sets its two
            // flags (field_idle.h), plays 0x104, and closes Tactics by the
            // cancel branch's stores without the cancel's two sounds. The top
            // bar then closes the menu, and the idle field frame raises the
            // party swap (FieldIdle_Tick).
            field_idle::g_ally_pending = 1;
            field_idle::g_ally_close_menu = 1;
            SoundRest2f(0x104);
            TacticsClose();
            return;
        } else {
            SoundRest2f(0x107);
            return;
        }
        SoundRest2f(0x102);
        SoundRest2f(0x104);
        const unsigned char c = Rest2f_TopChoice;
        const unsigned char step = FieldS_State;
        BRest2f(Rec(18, 0xB)) = c;
        BRest2f(Rec(21, 0)) = 0;
        FieldS_Step = 0;
        FieldS_State = static_cast<unsigned char>(step + c + 1);
        BRest2f(::bof3::Img(kTimer.v)) = 5;
        return;
    }
    if (!(static_cast<std::uint32_t>(Field_CancelButtons) & now)) return;
    SoundRest2f(0x102);
    SoundRest2f(0x106);
    TacticsClose();
}

// original 0x58F2F0: TacticsMenu_Steps[2] - jmp through TacticsFormation_Steps
// by the sub-step 0x929F02, unchecked (ours aborts past its 3).
extern "C" void __cdecl Tactics_Formation(void) {
    Run("Tactics_Formation (0x58F2F0)", TacticsFormation_Steps, TacticsFormation_Steps_count, FieldS_Step, "0x929F02");
}

// original 0x58F300: TacticsFormation_Steps[0] (and TacticsMenu_Steps[6]) -
// the backdrop; at the countdown's end the party's records 4.. get +0xB = 1
// and the sub-step goes on.
extern "C" void __cdecl TacticsFormation_Enter(void) {
    Backdrop();
    if (!CountedDown()) return;
    ForParty([](unsigned char i) { BRest2f(Rec(4u + i, 0xB)) = 1; });
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x58F370: TacticsFormation_Steps[1] - the formation grid. Record 19
// (the picked cell's mark) at kFormCells[3 row + col] of the cell picked, or
// closed; record 20 (the cursor) at the cursor's cell; records 12..17's +0xC
// cleared, then the set cell's record (11 + col + 2 row) +0xC = 1 << row for a
// cursor in column 0, else 7; record 11's word +0x10 0x3B, or 0x76 + col +
// 2 row. Confirm: on column 1..2 the cursor's cell becomes the formation set
// (sound 0x104, the pick dropped, 0x6BDFD2 / D3 the cell); on column 0 with no
// pick the cursor's cell is picked (0x103); with a pick, the two party bytes
// 0x904065 + row swapped (0x58BD50, sound 0x104) and the rows noted when they
// differ, the pick dropped. Cancel (0x106): a pick dropped; else entered
// directly (0x929F10 1) Game_Step on and the step 6; else 0x904060 written,
// the sound 0x102, records 12..17 of kind 0xB in use to state 1, the sub-step
// on and the countdown 5. Then up / down (0x5000) move the cursor's row with a
// wrap, skipping cells the grid has 0 for; left / right (0xA000) its column
// likewise, but only while 0x6BDFC6 is not 0 - with no pick (0x7F); a pick is
// taken only on column 0 here, so a pick holds the column; sound 0x101 when
// the cell moved; 0x904060 = the set's col + 2 row - 1 again. The grid loops
// never end on a column of three empty cells, as the original's never would
// (section 7): the grid is read through a volatile, so the compiler may not
// assume they end.
extern "C" void __cdecl TacticsFormation_Pick(void) {
    unsigned char vdelta = 0, hdelta = 0;
    const unsigned char zero = 0;
    Backdrop();
    {
        const unsigned char pick = Rest2f_PickCol;
        BRest2f(Rec(19, 0)) = 0;
        if (pick != 0x7F) {
            const U e = ::bof3::Img(kFormCells.v) + 8u * static_cast<U>(Rest2f_PickRow * 3 + static_cast<signed char>(pick));
            BRest2f(Rec(19, 0)) = 1;
            BRest2f(Rec(19, 0xB)) = 0;
            SetWRest2f(Rec(19, 4), WRest2f(e));
            SetWRest2f(Rec(19, 6), WRest2f(e + 2));
            BRest2f(Rec(19, 0xA)) = static_cast<unsigned char>(BRest2f(e + 4) + 1);
        }
    }
    unsigned char col = Rest2f_CursorCol;
    {
        const int row = Rest2f_CursorRow;
        const int c = static_cast<signed char>(col);
        BRest2f(Rec(20, 0)) = 1;
        BRest2f(Rec(20, 0xB)) = 1;
        const U e = ::bof3::Img(kFormCells.v) + 8u * static_cast<U>(3 * row + c);
        SetWRest2f(Rec(20, 4), WRest2f(e));
        SetWRest2f(Rec(20, 6), WRest2f(e + 2));
        BRest2f(Rec(20, 0xA)) = static_cast<unsigned char>(BRest2f(e + 4) + 1);
        for (unsigned n = 12; n <= 17; ++n) BRest2f(Rec(n, 0xC)) = 0;
        const unsigned char mark = col == 0 ? static_cast<unsigned char>(1u << (static_cast<U>(row) & 31)) : 7;
        const int set = Rest2f_SetCol + 2 * Rest2f_SetRow;
        BRest2f(Rec(11, 0xC) + kWindowStride * static_cast<U>(set)) = mark;
        SetWRest2f(Rec(11, 0x10), col == 0 ? 0x3Bu : static_cast<U>(c + 2 * row + 0x76));
    }
    const U pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        BRest2f(Rec(19, 0)) = 0;
        if (col != 0) {
            SoundRest2f(0x104);
            const unsigned char cc = Rest2f_CursorCol, cr = static_cast<unsigned char>(Rest2f_CursorRow);
            Rest2f_SetCol = cc;
            Rest2f_SetRow = cr;
            Rest2f_PickCol = 0x7F;
            Rest2f_PickRow = 0x7F;
            Rest2f_SwappedA = cc;
            Rest2f_SwappedB = cr;
        } else if (Rest2f_PickCol == 0x7F) {
            SoundRest2f(0x103);
            Rest2f_PickCol = Rest2f_CursorCol;
            Rest2f_PickRow = static_cast<unsigned char>(Rest2f_CursorRow);
        } else {
            SoundRest2f(0x104);
            unsigned char* const picked = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd5 + static_cast<std::uint32_t>(Rest2f_PickRow));
            unsigned char* const cursor = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd5 + static_cast<std::uint32_t>(Rest2f_CursorRow));
            SwapRest2f(cursor, picked);
            const unsigned char a = static_cast<unsigned char>(Rest2f_CursorRow), b = static_cast<unsigned char>(Rest2f_PickRow);
            if (a != b) {
                Rest2f_SwappedA = a;
                Rest2f_SwappedB = b;
            }
            Rest2f_PickCol = 0x7F;
            Rest2f_PickRow = 0x7F;
        }
    } else if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        SoundRest2f(0x106);
        const unsigned char pick = Rest2f_PickCol;
        BRest2f(Rec(19, 0)) = 0;
        BRest2f(Rec(20, 0)) = 0;
        if (pick != 0x7F) {
            Rest2f_PickCol = 0x7F;
            Rest2f_PickRow = 0x7F;
        } else if (ScenaSc6_Load10 == 1) {
            Game_Step = static_cast<std::uint32_t>(Game_Step) + 1;
            FieldS_State = 6;
            FieldS_Step = 0;
            ScenaSc6_Load10 = 0;
        } else {
            const unsigned char set = static_cast<unsigned char>(static_cast<unsigned char>(Rest2f_SetRow) * 2 + static_cast<unsigned char>(Rest2f_SetCol) - 1);
            BRest2f(::bof3::Img(kTimer.v)) = 0;
            Cond_Flags[0xd0] = set;
            SoundRest2f(0x102);
            for (unsigned n = 12; n <= 17; ++n)
                if (BRest2f(Rec(n, 0)) != 0 && BRest2f(Rec(n, 2)) == 0xB) BRest2f(Rec(n, 3)) = 1;
            const unsigned char sub = FieldS_Step;
            BRest2f(::bof3::Img(kTimer.v)) = 5;
            FieldS_Step = static_cast<unsigned char>(sub + 1);
        }
    }
    // the cursor: up / down, then (while 0x6BDFC6 is not 0) left / right
    unsigned char step;
    const U v = Repeat(static_cast<std::uint32_t>(Input_Pressed) & 0x5000);
    if (v & 0x1000) step = 0xFF;
    else step = vdelta;
    if (v & 0x4000) {
        vdelta = 1;
        step = vdelta;
    }
    unsigned char row = static_cast<unsigned char>(Rest2f_CursorRow);
    unsigned char c = Rest2f_CursorCol;
    const unsigned char old_row = row;
    const int ccol = static_cast<signed char>(c);
    do {
        row = static_cast<unsigned char>(row + step);
        if (static_cast<signed char>(row) < 0) row = 2;
        else if (static_cast<signed char>(row) > 2) row = 0;
    } while (*static_cast<volatile unsigned char*>(move_script::At(::bof3::Img(kGrid.v) + static_cast<U>(3 * static_cast<signed char>(row) + ccol))) == 0);
    Rest2f_CursorRow = row;
    unsigned char before;
    if (Rest2f_PickCol == 0) {
        before = zero;
    } else {
        const U h = Repeat(static_cast<std::uint32_t>(Input_Pressed) & 0xA000);
        if (h & 0x8000) hdelta = 0xFF;
        if (h & 0x2000) hdelta = 1;
        row = static_cast<unsigned char>(Rest2f_CursorRow);
        c = Rest2f_CursorCol;
        const int base = 3 * static_cast<signed char>(row);
        before = c;
        do {
            c = static_cast<unsigned char>(c + hdelta);
            if (static_cast<signed char>(c) < 0) c = 2;
            else if (static_cast<signed char>(c) > 2) c = 0;
        } while (*static_cast<volatile unsigned char*>(move_script::At(::bof3::Img(kGrid.v) + static_cast<U>(base + static_cast<signed char>(c)))) == 0);
        Rest2f_CursorCol = c;
    }
    if (before != c || old_row != row) SoundRest2f(0x101);
    Cond_Flags[0xd0] = static_cast<unsigned char>(static_cast<unsigned char>(Rest2f_SetRow) * 2 + static_cast<unsigned char>(Rest2f_SetCol) - 1);
}

// original 0x58F760: TacticsFormation_Steps[2] - the backdrop; at the
// countdown's end the party's records 4.. get +0xB = 0 and the step goes back
// one (to the top).
extern "C" void __cdecl TacticsFormation_Leave(void) {
    Backdrop();
    if (!CountedDown()) return;
    ForParty([](unsigned char i) { BRest2f(Rec(4u + i, 0xB)) = 0; });
    FieldS_State = static_cast<unsigned char>(FieldS_State - 1);
}

// original 0x58F7D0: TacticsMenu_Steps[3] - jmp through TacticsMembers_Steps by
// the sub-step 0x929F02, unchecked (ours aborts past its 3).
extern "C" void __cdecl Tactics_Members(void) {
    Run("Tactics_Members (0x58F7D0)", TacticsMembers_Steps, TacticsMembers_Steps_count, FieldS_Step, "0x929F02");
}

// original 0x58F7E0: TacticsMembers_Steps[0] - the backdrop; at the countdown's
// end each party record 4.. whose member (the party byte 0x904062 + 3 rec[+0xB]
// + rec[+0xA]) has CharacterRecords +0xB bit 1 gets +0xC = 1; the sub-step on.
extern "C" void __cdecl TacticsMembers_Enter(void) {
    Backdrop();
    if (!CountedDown()) return;
    ForParty([](unsigned char i) {
        const U r = Rec(4u + i, 0);
        const unsigned char id = BRest2f(::bof3::Img(Cond_Flags_addr) + 0xd2 + 3u * BRest2f(r + 0xB) + BRest2f(r + 0xA));
        if (Character(id)[0xB] & 2) BRest2f(r + 0xC) = 1;
    });
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// The solo-character mode's members step (group MS; src/game/solo/
// solo_screens.h, docs/solo-character-screens.md). While a Tactics screen
// built with solo_screens::Wide() is open (g_tactics.wide, latched by
// TacticsMembers_Build), Pick and the reserve half of Swap run these instead
// of the original's bodies: the reserve is solo_screens::g_tactics, every
// joined member not in the party, of which the screen shows three rows from
// g_tactics.top and scrolls; 0x6BDFD4.. holds the rows shown and record 12's
// +0xA their number, so the reserve's window (MenuList_ReserveWinDraw) runs
// unchanged. In the reserve column the cursor's row (0x929F09) and the
// pick's (0x929F0D) are rows of the whole list; in the party column they are
// the original's. Everything else is the original's, step for step.
namespace {

namespace ss = solo_screens;

// The members screen's place for column `col` and a row (a whole-list row in
// column 1): kMemberCells[col + 2 * shown row], or 0 when the row is not shown.
U WidePlace(int col, int row) {
    const int shown = col == 0 ? row : ss::ShownRow(ss::g_tactics, row);
    if (shown < 0 || shown >= static_cast<int>(ss::kRows)) return 0;
    return ::bof3::Img(kMemberCells.v) + 8u * static_cast<U>(col + 2 * shown);
}

// The rows shown into 0x6BDFD4.. and their number into record 12's +0xA.
void WideWindow() {
    ss::Window(ss::g_tactics, move_script::At(::bof3::Img(kReserve.v)));
    BRest2f(Rec(12, 0xA)) = static_cast<unsigned char>(ss::Shown(ss::g_tactics));
}

// The champion's mark (solo_screens::kChampionMark). Its place: the
// champion's cursor place in the party column or on a reserve row shown, 0
// when the champion is not on the screen.
U WideChampionPlace(unsigned char count) {
    for (unsigned k = 0; k < count && k < ss::kRows; ++k)
        if (ss::IsChampion(BRest2f(::bof3::Img(kParty.v) + k))) return WidePlace(0, static_cast<int>(k));
    const unsigned shown = ss::Shown(ss::g_tactics);
    for (unsigned s = 0; s < shown; ++s) {
        const int whole = ss::Whole(ss::g_tactics, static_cast<int>(s));
        if (ss::IsChampion(ss::g_tactics.ids[whole])) return WidePlace(1, whole);
    }
    return 0;
}

// kHand: window record 21 - Tactics_Open's pointing hand (handler 6, kind
// 0xE, MenuWin_Hand), closed in the members step and set afresh by
// Tactics_Top every frame there - open at the place plus the hand's offset,
// closed when the champion is not shown. kCleared: record 21 closed, and the
// cleared slot's look (solo_screens::DrawCleared) on the champion's panel, in
// front (InFront): the panels are window records drawn after this step, so a
// draw of this step's own in slot 1 lies under them (measured 2026-11-12).
// The panel's origin: the party panel's is its place (MenuList_DrawMemberPanel
// draws Menu_DrawMemberStatus at the record's x, y, the places' x, y); a
// reserve row's box is at its place, three left of and above a member
// panel's box. The cursor (record 19) and the pick (record 20) are drawn in
// front again over the champion's panel, as the game draws them over every
// panel. The mode only: IsChampion is false while it is not active.
void WideChampionMark(unsigned char count) {
    const U at = WideChampionPlace(count);
    if (ss::kChampionMark == ss::ChampionMark::kHand && at != 0) {
        BRest2f(Rec(21, 0)) = 1;
        SetWRest2f(Rec(21, 4), (WRest2f(at) + static_cast<U>(ss::kChampionHandDx)) & 0xFFFF);
        SetWRest2f(Rec(21, 6), (WRest2f(at + 2) + static_cast<U>(ss::kChampionHandDy)) & 0xFFFF);
        return;
    }
    BRest2f(Rec(21, 0)) = 0;
    if (at == 0) return;
    const bool reserve = at != WidePlace(0, 0) && at != WidePlace(0, 1) && at != WidePlace(0, 2);
    const int x = static_cast<short>(WRest2f(at)) - (reserve ? 3 : 0);
    const int y = static_cast<short>(WRest2f(at + 2)) - (reserve ? 3 : 0);
    ss::InFront([&] {
        ss::DrawCleared(x, y, static_cast<unsigned>(solo::Solo_ChampionRecord()), reserve);
        for (unsigned n = 19; n <= 20; ++n) {
            if (BRest2f(Rec(n, 0)) == 0) continue;
            if (WRest2f(Rec(n, 4)) != WRest2f(at) || WRest2f(Rec(n, 6)) != WRest2f(at + 2)) continue;
            const U pair = ::bof3::Img(MenuList_CursorBoxSizes_addr) + 4u * BRest2f(Rec(n, 0xA));
            SH_CALL(Menu_DrawCursorBox)(static_cast<short>(WRest2f(Rec(n, 4))), static_cast<short>(WRest2f(Rec(n, 6))),
                                        static_cast<int>(WRest2f(pair)), static_cast<int>(WRest2f(pair + 2)),
                                        BRest2f(Rec(n, 0xB)), 6);
        }
    });
}

// TacticsMembers_Pick with the long reserve: the original's body, the reserve
// column's rows read through g_tactics, and the champion's mark.
void TacticsMembersPickWide() {
    Backdrop();
    {
        const U e = WidePlace(FieldS_Column, BattleE1_ModeByte9);
        if (e == 0)
            bof3::Fatal("solo members screen: the cursor's row %d in column %d is not shown (top %u of %u)",
                        static_cast<int>(BattleE1_ModeByte9), static_cast<int>(FieldS_Column),
                        static_cast<unsigned>(ss::g_tactics.top), static_cast<unsigned>(ss::g_tactics.count));
        BRest2f(Rec(20, 0)) = 0;
        BRest2f(Rec(19, 0)) = 1;
        BRest2f(Rec(19, 0xB)) = 1;
        SetWRest2f(Rec(19, 4), WRest2f(e));
        BRest2f(Rec(19, 0xA)) = BRest2f(e + 4);
        SetWRest2f(Rec(19, 6), WRest2f(e + 2));
        const unsigned char held = static_cast<unsigned char>(FieldS_PickRow);
        if (held != 0x7F) {
            // A picked reserve row scrolled out of sight is held, unmarked.
            const U f = WidePlace(ShopStates_Clear0A, static_cast<signed char>(held));
            if (f != 0) {
                BRest2f(Rec(20, 0)) = 1;
                BRest2f(Rec(20, 0xB)) = 0;
                SetWRest2f(Rec(20, 4), WRest2f(f));
                SetWRest2f(Rec(20, 6), WRest2f(f + 2));
                BRest2f(Rec(20, 0xA)) = BRest2f(f + 4);
            }
        }
    }
    {
        const unsigned char refused = Rest2f_Refused;
        if (refused == 0xFF) {
            SetWRest2f(Rec(11, 0x10), 0x60);
        } else {
            SH_CALL(TextRecord_Set)(0, 8, Character(refused));
            SetWRest2f(Rec(11, 0x10), 0x61);
        }
    }
    WideChampionMark(PartyCountRest2f());
    const U keys = Repeat(static_cast<std::uint32_t>(Input_Pressed) & 0xF000);
    const unsigned char count = PartyCountRest2f();
    const int old_col = static_cast<unsigned char>(FieldS_Column);
    int col = old_col;
    const int old_row = BattleE1_ModeByte9;
    int row = old_row;
    if ((keys & 0xA000) && ss::g_tactics.count != 0) {
        // The row shown is kept across the switch, as the original keeps the row.
        row = col == 0 ? ss::Whole(ss::g_tactics, row) : ss::ShownRow(ss::g_tactics, row);
        col ^= 1;
        FieldS_Column = static_cast<signed char>(col);
    }
    if (col == 0) {
        if (row > static_cast<int>(count) - 1) row = count - 1;
    } else {
        row = ss::Hold(ss::g_tactics, row);
    }
    if (keys & 0x1000) {
        if (col == 0) row = row - 1 < 0 ? count - 1 : row - 1;
        else row = ss::Move(ss::g_tactics, row, -1);
    } else if (keys & 0x4000) {
        if (col == 0) row = row + 1 > static_cast<int>(count) - 1 ? 0 : row + 1;
        else row = ss::Move(ss::g_tactics, row, +1);
    }
    BattleE1_ModeByte9 = static_cast<signed char>(row);
    WideWindow();
    if (old_col != col || old_row != row) SoundRest2f(0x100);
    const U pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        if (static_cast<unsigned char>(ShopStates_Clear0A) == 0x7F) {
            SoundRest2f(0x103);
            ShopStates_Clear0A = static_cast<signed char>(FieldS_Column);
            FieldS_PickRow = static_cast<signed char>(BattleE1_ModeByte9);
            return;
        }
        const unsigned char refused = SH_CALL(TacticsMembers_Swap)();
        Rest2f_Refused = refused;
        SoundRest2f(refused == 0xFF ? 0x103 : 0x107);
        ShopStates_Clear0A = 0x7F;
        FieldS_PickRow = 0x7F;
        return;
    }
    if (!(static_cast<std::uint32_t>(Field_CancelButtons) & pressed)) return;
    SoundRest2f(0x106);
    if (static_cast<unsigned char>(ShopStates_Clear0A) != 0x7F) {
        ShopStates_Clear0A = 0x7F;
        FieldS_PickRow = 0x7F;
        return;
    }
    const unsigned char sub = FieldS_Step;
    BRest2f(::bof3::Img(kTimer.v)) = 5;
    BRest2f(Rec(12, 3)) = 1;
    FieldS_Step = static_cast<unsigned char>(sub + 1);
    BRest2f(Rec(19, 0)) = 0;
    BRest2f(Rec(20, 0)) = 0;
    BRest2f(Rec(21, 0)) = 0;   // the champion's hand (kHand), with the two cursors
}

// TacticsMembers_Swap's reserve cases with the long reserve (two party rows
// stay the original's). Two reserve rows: the two ids exchanged. Across: the
// party's member A and the reserve's B, refused by +0xB bit 1 as the original
// (A asked first: the hero's permanent mark keeps him, a scene's mark keeps
// its member); else sound 0x103, A becomes B in both party lists, B becomes A
// in the reserve, A's row's portrait cells cleared - the original's stores,
// the reserve's over the whole list.
unsigned char TacticsMembersSwapWide() {
    ss::Reserve& r = ss::g_tactics;
    const int col = static_cast<unsigned char>(FieldS_Column);
    const int row = BattleE1_ModeByte9, held = FieldS_PickRow;
    auto whole = [&](int w) -> unsigned char& {
        if (w < 0 || w >= r.count)
            bof3::Fatal("solo members screen: reserve row %d of %u", w, static_cast<unsigned>(r.count));
        return r.ids[w];
    };
    if (col == static_cast<unsigned char>(ShopStates_Clear0A)) {
        unsigned char& x = whole(row);
        unsigned char& y = whole(held);
        const unsigned char t = x;
        x = y;
        y = t;
        WideWindow();
        return 0xFF;
    }
    const int party_row = col == 0 ? row : held;
    const unsigned char a = BRest2f(::bof3::Img(kParty.v) + static_cast<U>(party_row));
    const unsigned char b = whole(col == 0 ? held : row);
    if (Character(a)[0xB] & 2) return a;
    if (Character(b)[0xB] & 2) return b;
    SoundRest2f(0x103);
    unsigned char i = 0;
    if (PartyCountRest2f() != 0) {
        do {
            if (BRest2f(::bof3::Img(kParty.v) + i) == a) BRest2f(::bof3::Img(kParty.v) + i) = b;
            if (BRest2f(::bof3::Img(kPartyCopy.v) + i) == a) BRest2f(::bof3::Img(kPartyCopy.v) + i) = b;
            ++i;
        } while (i < PartyCountRest2f());
    }
    for (unsigned j = 0; j < r.count; ++j)
        if (r.ids[j] == b) r.ids[j] = a;
    SetLong(move_script::At(::bof3::Img(kPortrait4.v) + 4u * static_cast<U>(party_row)), 0);
    SetLong(move_script::At(::bof3::Img(kPortraitA.v) + 6u * static_cast<U>(party_row)), 0);
    SetWRest2f(::bof3::Img(kPortraitA.v) + 6u * static_cast<U>(party_row) + 4, 0);
    SetLong(move_script::At(::bof3::Img(kPortraitB.v) + 6u * static_cast<U>(party_row)), 0);
    SetWRest2f(::bof3::Img(kPortraitB.v) + 6u * static_cast<U>(party_row) + 4, 0);
    WideWindow();
    return 0xFF;
}

// TacticsMembers_Build with the long reserve: the reserve filled into
// g_tactics, the rows shown into 0x6BDFD4.., record 12 as the original's with
// +0xA the rows shown, the party's flags as the original's and the reserve's
// for the rows shown only (the original writes them from 0x6BDFCD two apart
// for every member, past their three pairs from the fourth on; nothing reads
// them - pe_xref.py over 0x6BDFCC..0x6BDFD3 finds stores only); the cursor at
// 0, 0, nothing picked, nobody refused.
void TacticsMembersBuildWide() {
    const unsigned char count = PartyCountRest2f();
    ss::Fill(ss::g_tactics, move_script::At(::bof3::Img(kParty.v)), count);
    ss::g_tactics.wide = true;
    BRest2f(Rec(12, 0xA)) = 0;
    WideWindow();
    const unsigned n = ss::Shown(ss::g_tactics);
    BRest2f(Rec(12, 0)) = 1;
    BRest2f(Rec(12, 1)) = 6;
    BRest2f(Rec(12, 2)) = 0x13;
    BRest2f(Rec(12, 3)) = 2;
    SetWRest2f(Rec(12, 4), 0x140);
    SetWRest2f(Rec(12, 6), 0x3B);
    SetLong(move_script::At(Rec(12, 0x20)), static_cast<std::int32_t>(::bof3::Img(kReserve.v)));
    for (unsigned i = 0; i < 3; ++i) BRest2f(::bof3::Img(kMemberFlags.v) + 2 * i) = i < count ? 1 : 0;
    for (unsigned j = 0; j < 3; ++j) BRest2f(::bof3::Img(kMemberFlags.v) + 1 + 2 * j) = j < n ? 1 : 0;
    FieldS_Column = 0;
    BattleE1_ModeByte9 = 0;
    ShopStates_Clear0A = 0x7F;
    FieldS_PickRow = 0x7F;
    Rest2f_Refused = 0xFF;
    bof3::Log("solo members screen (Tactics): %u in the party, %u in reserve, %u shown", static_cast<unsigned>(count),
              static_cast<unsigned>(ss::g_tactics.count), n);
}

}  // namespace

// original 0x58F890: TacticsMembers_Steps[1] - the members screen. Record 19
// (the cursor) at kMemberCells[col + 2 row]; record 20 (the cell picked) there
// too, or closed; with a member refused (0x6BDFD7) its name to text record 0
// (TextRecord_Set, 8 bytes) and record 11's word +0x10 0x61, else 0x60. The
// cursor: up / down / left / right auto-repeated (0xF000); left / right switch
// the column while the reserve has members (record 12's +0xA); the row is held
// inside the column's count, then up and down wrap; sound 0x100 when it moved.
// Confirm: with nothing picked the cursor's cell is picked (0x103); else
// TacticsMembers_Swap, its answer to 0x6BDFD7 and sound 0x103 (done, 0xFF) or
// 0x107 (refused), the pick dropped. Cancel (0x106): a pick dropped; else the
// sub-step on, the countdown 5, record 12 to state 1, 19 and 20 closed.
extern "C" void __cdecl TacticsMembers_Pick(void) {
    if (solo_screens::g_tactics.wide) return TacticsMembersPickWide();   // the solo-character mode (MS)
    Backdrop();
    {
        const U e = ::bof3::Img(kMemberCells.v) + 8u * static_cast<U>(FieldS_Column + 2 * BattleE1_ModeByte9);
        BRest2f(Rec(20, 0)) = 0;
        BRest2f(Rec(19, 0)) = 1;
        BRest2f(Rec(19, 0xB)) = 1;
        SetWRest2f(Rec(19, 4), WRest2f(e));
        BRest2f(Rec(19, 0xA)) = BRest2f(e + 4);
        SetWRest2f(Rec(19, 6), WRest2f(e + 2));
        const unsigned char held = static_cast<unsigned char>(FieldS_PickRow);
        if (held != 0x7F) {
            const U f = ::bof3::Img(kMemberCells.v) + 8u * static_cast<U>(ShopStates_Clear0A + 2 * static_cast<signed char>(held));
            BRest2f(Rec(20, 0)) = 1;
            BRest2f(Rec(20, 0xB)) = 0;
            SetWRest2f(Rec(20, 4), WRest2f(f));
            SetWRest2f(Rec(20, 6), WRest2f(f + 2));
            BRest2f(Rec(20, 0xA)) = BRest2f(f + 4);
        }
    }
    {
        const unsigned char refused = Rest2f_Refused;
        if (refused == 0xFF) {
            SetWRest2f(Rec(11, 0x10), 0x60);
        } else {
            SH_CALL(TextRecord_Set)(0, 8, Character(refused));
            SetWRest2f(Rec(11, 0x10), 0x61);
        }
    }
    const U keys = Repeat(static_cast<std::uint32_t>(Input_Pressed) & 0xF000);
    const unsigned char count = PartyCountRest2f();
    unsigned char col = static_cast<unsigned char>(FieldS_Column);
    const unsigned char reserve = BRest2f(Rec(12, 0xA));
    const unsigned char old_col = col;
    if ((keys & 0xA000) && reserve != 0) {
        col = static_cast<unsigned char>(col ^ 1);
        FieldS_Column = col;
    }
    unsigned char row = static_cast<unsigned char>(BattleE1_ModeByte9);
    const int old_row = static_cast<signed char>(row);
    if (col == 0) {
        if (old_row > static_cast<int>(count) - 1) {
            row = static_cast<unsigned char>(count - 1);
            BattleE1_ModeByte9 = row;
        }
    } else {
        if (old_row > static_cast<int>(reserve) - 1) {
            row = static_cast<unsigned char>(reserve - 1);
            BattleE1_ModeByte9 = row;
        }
    }
    if (keys & 0x1000) {
        row = static_cast<unsigned char>(row - 1);
        BattleE1_ModeByte9 = row;
        if (static_cast<signed char>(row) < 0) {
            row = static_cast<unsigned char>(FieldS_Column) == 0 ? static_cast<unsigned char>(count - 1) : static_cast<unsigned char>(reserve - 1);
            BattleE1_ModeByte9 = row;
        }
    } else if (keys & 0x4000) {
        const unsigned char now = static_cast<unsigned char>(FieldS_Column);
        row = static_cast<unsigned char>(row + 1);
        BattleE1_ModeByte9 = row;
        const int last = now == 0 ? static_cast<int>(count) - 1 : static_cast<int>(reserve) - 1;
        if (static_cast<signed char>(row) > last) {
            row = 0;
            BattleE1_ModeByte9 = row;
        }
    }
    if (static_cast<int>(old_col) != FieldS_Column || old_row != static_cast<signed char>(row)) SoundRest2f(0x100);
    const U pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        if (static_cast<unsigned char>(ShopStates_Clear0A) == 0x7F) {
            SoundRest2f(0x103);
            ShopStates_Clear0A = static_cast<unsigned char>(FieldS_Column);
            FieldS_PickRow = static_cast<unsigned char>(BattleE1_ModeByte9);
            return;
        }
        const unsigned char refused = SH_CALL(TacticsMembers_Swap)();
        Rest2f_Refused = refused;
        SoundRest2f(refused == 0xFF ? 0x103 : 0x107);
        ShopStates_Clear0A = 0x7F;
        FieldS_PickRow = 0x7F;
        return;
    }
    if (!(static_cast<std::uint32_t>(Field_CancelButtons) & pressed)) return;
    SoundRest2f(0x106);
    if (static_cast<unsigned char>(ShopStates_Clear0A) != 0x7F) {
        ShopStates_Clear0A = 0x7F;
        FieldS_PickRow = 0x7F;
        return;
    }
    const unsigned char sub = FieldS_Step;
    BRest2f(::bof3::Img(kTimer.v)) = 5;
    BRest2f(Rec(12, 3)) = 1;
    FieldS_Step = static_cast<unsigned char>(sub + 1);
    BRest2f(Rec(19, 0)) = 0;
    BRest2f(Rec(20, 0)) = 0;
}

// original 0x58FB60: TacticsMembers_Steps[2] - the backdrop; at the
// countdown's end the party's records 4.. get +0xC = 0 and the step goes back
// two (to the top).
extern "C" void __cdecl TacticsMembers_Leave(void) {
    Backdrop();
    if (!CountedDown()) return;
    ForParty([](unsigned char i) { BRest2f(Rec(4u + i, 0xC)) = 0; });
    FieldS_State = static_cast<unsigned char>(FieldS_State - 2);
}

// original 0x58FBD0: TacticsMenu_Steps[4] - the backdrop; at the countdown's
// end records 12..18 freed (FieldMenu_FreeRecords12To18), the party's records
// 4.. to state 4, records 7..10 to state 2, the countdown 5, and the field
// menu back to its top bar (state 1, step 0).
extern "C" void __cdecl Tactics_Close(void) {
    Backdrop();
    if (!CountedDown()) return;
    SH_CALL(FieldMenu_FreeRecords12To18)();
    ForParty([](unsigned char i) { BRest2f(Rec(4u + i, 3)) = 4; });
    BRest2f(::bof3::Img(kTimer.v)) = 5;
    for (unsigned n = 7; n <= 10; ++n) BRest2f(Rec(n, 3)) = 2;
    BattleE1_Mode = 1;
    FieldS_State = 0;
}

// original 0x58FC60: TacticsMenu_Steps[5] - Tactics entered at the formation:
// record 11 (kind 9) at x 0x14, y 0x10; records 19, 20 set up, closed; the
// party's records 4.. (kind 0 of handler 6, +0xA the slot) at x 0x11, y 0x3E +
// 0x36 slot, +0xB 1; the backdrop; the grid built (TacticsFormation_Build); the
// step 2, sub-step 0, countdown 5.
extern "C" void __cdecl Tactics_OpenFormation(void) {
    BRest2f(Rec(11, 1)) = 6;
    BRest2f(Rec(11, 2)) = 9;
    BRest2f(Rec(11, 3)) = 2;
    BRest2f(Rec(11, 0)) = 1;
    SetWRest2f(Rec(11, 4), 0x14);
    SetWRest2f(Rec(11, 6), 0x10);
    SetWRest2f(Rec(11, 0x10), 0);
    for (unsigned n = 19; n <= 20; ++n) {
        BRest2f(Rec(n, 1)) = 6;
        BRest2f(Rec(n, 2)) = 0xA;
        BRest2f(Rec(n, 0)) = 0;
        BRest2f(Rec(n, 0xA)) = 0;
    }
    ForParty([](unsigned char i) {
        const U r = Rec(4u + i, 0);
        BRest2f(r + 0xA) = i;
        rv::As<records::WindowRecord>(move_script::At(r))->handler = 6;
        rv::As<records::WindowRecord>(move_script::At(r))->kind = 0;
        rv::As<records::WindowRecord>(move_script::At(r))->state = 0;
        rv::As<records::WindowRecord>(move_script::At(r))->in_use = 1;
        rv::As<records::WindowRecord>(move_script::At(r))->x = 0x11;
        rv::As<records::WindowRecord>(move_script::At(r))->y = 0x36u * i + 0x3E;
        BRest2f(r + 0xB) = 1;
        BRest2f(r + 0xC) = 0;
    });
    Backdrop();
    SH_CALL(TacticsFormation_Build)();
    FieldS_State = 2;
    FieldS_Step = 0;
    BRest2f(::bof3::Img(kTimer.v)) = 5;
}

// original 0x58FD60: called by Tactics_Top and Tactics_OpenFormation - the
// formation grid. By Party_Count(0): 1 member 1 cell from record 12 (+0xA from
// 0), 2 members 3 (from 1), else 6 (from 4). Rows 0..2: column 0 is open for a
// row below the count; columns 1..2 are six cells n = 0..5 in order, open when
// 0x904061 bit n is set and n is below the limit, each such cell's window
// record 12 + n (handler 6, kind 0xB, state 2) placed at x 0xF5 + 0x4B col, y
// 0x3E + 0x36 row, +0xA n + the base, +0xB n, +0x10 0xE00. Then the set's cell
// from 0x904060 (column (v & 1) + 1, row v >> 1), the pick dropped, the cursor
// at column 0 row 0.
extern "C" void __cdecl TacticsFormation_Build(void) {
    const unsigned char count = PartyCountRest2f();
    unsigned char limit, base;
    if (count == 1) {
        base = 0;
        limit = 1;
    } else if (count == 2) {
        base = 1;
        limit = 3;
    } else {
        base = 4;
        limit = 6;
    }
    unsigned char mask = 1, n = 0;
    for (unsigned char row = 0; row < 3; ++row) {
        for (unsigned char col = 0; col < 3; ++col) {
            unsigned char* const cell = move_script::At(::bof3::Img(kGrid.v) + 3u * row + col);
            if (col == 0 && row < count) {
                *cell = 1;
                continue;
            }
            *cell = 0;
            if (col == 0) continue;
            if ((Cond_Flags[0xd1] & mask) && n < limit) {
                *cell = 1;
                const U r = Rec(12u + n, 0);
                rv::As<records::WindowRecord>(move_script::At(r))->x = 0x4Bu * col + 0xF5;
                rv::As<records::WindowRecord>(move_script::At(r))->handler = 6;
                rv::As<records::WindowRecord>(move_script::At(r))->kind = 0xB;
                BRest2f(r + 3) = 2;
                rv::As<records::WindowRecord>(move_script::At(r))->in_use = 1;
                BRest2f(r + 0xB) = n;
                BRest2f(r + 0xC) = 0;
                SetWRest2f(r + 0x10, 0xE00);
                rv::As<records::WindowRecord>(move_script::At(r))->y = 0x36u * row + 0x3E;
                BRest2f(r + 0xA) = static_cast<unsigned char>(n + base);
            } else {
                *cell = 0;
            }
            ++n;
            mask = static_cast<unsigned char>(mask << 1);
        }
    }
    const unsigned char set = Cond_Flags[0xd0];
    Rest2f_CursorCol = 0;
    Rest2f_SetRow = static_cast<unsigned char>(set >> 1);
    Rest2f_SetCol = static_cast<unsigned char>((set & 1) + 1);
    Rest2f_PickCol = 0x7F;
    Rest2f_PickRow = 0x7F;
    Rest2f_CursorRow = 0;
}

// original 0x58FEF0: called by Tactics_Top - the reserve: each of the eight
// CharacterRecords with +0xB bit 0 whose id (+9) is not among the party's
// Party_Count(0) bytes goes into 0x6BDFD4.. in record order; record 12 (handler
// 6, kind 0x13, at x 0x140, y 0x3B, +0x20 the list) holds their count at +0xA.
// The flags 0x6BDFCC + 2 row (party) are 1 below the count and 0 to row 2, and
// 0x6BDFCD + 2 row (reserve) 1 below the reserve's count - written after the
// list, which they overlap from the fourth row on (section 7). The cursor at
// 0, 0, nothing picked, nobody refused.
extern "C" void __cdecl TacticsMembers_Build(void) {
    if (solo_screens::Wide()) return TacticsMembersBuildWide();   // the solo-character mode (MS)
    solo_screens::g_tactics.wide = false;
    const unsigned char count = PartyCountRest2f();
    unsigned char n = 0;
    BRest2f(Rec(12, 0xA)) = 0;
    for (U r = ::bof3::Img(CharacterRecords_addr); r < ::bof3::Img(CharacterRecords_addr) + 8 * kCharacterStride; r += kCharacterStride) {
        if (!(BRest2f(r + 0xB) & 1)) continue;
        unsigned j = 0;
        while (j < count && BRest2f(r + 9) != BRest2f(::bof3::Img(kParty.v) + j)) ++j;
        if (j == count) BRest2f(::bof3::Img(kReserve.v) + n++) = BRest2f(r + 9);
    }
    BRest2f(Rec(12, 0xA)) = n;
    BRest2f(Rec(12, 0)) = 1;
    BRest2f(Rec(12, 1)) = 6;
    BRest2f(Rec(12, 2)) = 0x13;
    BRest2f(Rec(12, 3)) = 2;
    SetWRest2f(Rec(12, 4), 0x140);
    SetWRest2f(Rec(12, 6), 0x3B);
    SetLong(move_script::At(Rec(12, 0x20)), static_cast<std::int32_t>(::bof3::Img(kReserve.v)));
    unsigned from = 0;
    if (count > 0) {
        for (unsigned i = 0; i < count; ++i) BRest2f(::bof3::Img(kMemberFlags.v) + 2 * i) = 1;
        from = count;
    }
    if (count == 0 || count < 3)
        for (U p = ::bof3::Img(kMemberFlags.v) + 2 * from; p < ::bof3::Img(kMemberFlags.v) + 6; p += 2) BRest2f(p) = 0;
    for (unsigned j = 0; j < n; ++j) BRest2f(::bof3::Img(kMemberFlags.v) + 1 + 2 * j) = 1;
    FieldS_Column = 0;
    BattleE1_ModeByte9 = 0;
    ShopStates_Clear0A = 0x7F;
    FieldS_PickRow = 0x7F;
    Rest2f_Refused = 0xFF;
}

// original 0x590020: called by TacticsMembers_Pick - the swap of the cell
// picked with the cursor's, answering al 0xFF when done, or the member that
// refused it. Both in the party column: the two party records' +0xC, the party
// bytes (0x58BD50) and the rows' portrait cells (0x9398C0 + 4 row, and 0x939880
// / 0x9398A0 + 6 row, byte by byte) exchanged. Both in the reserve column: the
// reserve bytes exchanged (0x58BD50). Across: the party member A (row of the
// party column) and reserve member B; A's or B's CharacterRecords +0xB bit 1
// refuses (A first) and is the answer; else sound 0x103, every party byte
// (0x904062.. and the copy 0x904065..) that is A becomes B, every reserve byte
// that is B becomes A, and A's row's portrait cells cleared.
extern "C" unsigned char __cdecl TacticsMembers_Swap(void) {
    const unsigned char col = static_cast<unsigned char>(FieldS_Column);
    // The solo-character mode (MS): a reserve row is in it.
    if (solo_screens::g_tactics.wide && !(col == 0 && static_cast<unsigned char>(ShopStates_Clear0A) == 0))
        return TacticsMembersSwapWide();
    if (col == static_cast<unsigned char>(ShopStates_Clear0A)) {
        if (col != 0) {
            SwapRest2f(move_script::At(::bof3::Img(kReserve.v) + static_cast<std::uint32_t>(BattleE1_ModeByte9)),
                 move_script::At(::bof3::Img(kReserve.v) + static_cast<std::uint32_t>(FieldS_PickRow)));
            return 0xFF;
        }
        {
            const int held = FieldS_PickRow, row = BattleE1_ModeByte9;
            const U h = Rec(4, 0xC) + kWindowStride * static_cast<U>(held);
            const U r = Rec(4, 0xC) + kWindowStride * static_cast<U>(row);
            const unsigned char a = BRest2f(h), b = BRest2f(r);
            BRest2f(r) = a;
            BRest2f(h) = b;
            SwapRest2f(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2 + static_cast<U>(row)), move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2 + static_cast<U>(held)));
        }
        const int held = FieldS_PickRow, row = BattleE1_ModeByte9;
        U p = ::bof3::Img(kPortrait4.v) + 4u * static_cast<U>(held), q = ::bof3::Img(kPortrait4.v) + 4u * static_cast<U>(row);
        for (unsigned k = 0; k < 4; ++k, ++p, ++q) {
            const unsigned char x = BRest2f(p), y = BRest2f(q);
            BRest2f(q) = x;
            BRest2f(p) = y;
        }
        const U c = 6u * static_cast<U>(held), e = 6u * static_cast<U>(row);
        for (unsigned k = 0; k < 6; ++k) {
            unsigned char x = BRest2f(::bof3::Img(kPortraitB.v) + c + k);
            unsigned char y = BRest2f(::bof3::Img(kPortraitB.v) + e + k);
            BRest2f(::bof3::Img(kPortraitB.v) + e + k) = x;
            x = BRest2f(::bof3::Img(kPortraitA.v) + c + k);
            BRest2f(::bof3::Img(kPortraitB.v) + c + k) = y;
            y = BRest2f(::bof3::Img(kPortraitA.v) + e + k);
            BRest2f(::bof3::Img(kPortraitA.v) + e + k) = x;
            BRest2f(::bof3::Img(kPortraitA.v) + c + k) = y;
        }
        return 0xFF;
    }
    int row;
    unsigned char a, b;
    if (col == 0) {
        row = BattleE1_ModeByte9;
        a = BRest2f(::bof3::Img(kParty.v) + static_cast<U>(row));
        b = BRest2f(::bof3::Img(kReserve.v) + static_cast<std::uint32_t>(FieldS_PickRow));
    } else {
        row = FieldS_PickRow;
        a = BRest2f(::bof3::Img(kParty.v) + static_cast<U>(row));
        b = BRest2f(::bof3::Img(kReserve.v) + static_cast<std::uint32_t>(BattleE1_ModeByte9));
    }
    if (Character(a)[0xB] & 2) return a;
    if (Character(b)[0xB] & 2) return b;
    SoundRest2f(0x103);
    unsigned char i = 0;
    if (PartyCountRest2f() != 0) {
        do {
            if (BRest2f(::bof3::Img(kParty.v) + i) == a) BRest2f(::bof3::Img(kParty.v) + i) = b;
            if (BRest2f(::bof3::Img(kPartyCopy.v) + i) == a) BRest2f(::bof3::Img(kPartyCopy.v) + i) = b;
            ++i;
        } while (i < PartyCountRest2f());
    }
    const unsigned char reserve = BRest2f(Rec(12, 0xA));
    for (unsigned j = 0; j < reserve; ++j)
        if (BRest2f(::bof3::Img(kReserve.v) + j) == b) BRest2f(::bof3::Img(kReserve.v) + j) = a;
    SetLong(move_script::At(::bof3::Img(kPortrait4.v) + 4u * static_cast<U>(row)), 0);
    SetLong(move_script::At(::bof3::Img(kPortraitA.v) + 6u * static_cast<U>(row)), 0);
    SetWRest2f(::bof3::Img(kPortraitA.v) + 6u * static_cast<U>(row) + 4, 0);
    SetLong(move_script::At(::bof3::Img(kPortraitB.v) + 6u * static_cast<U>(row)), 0);
    SetWRest2f(::bof3::Img(kPortraitB.v) + 6u * static_cast<U>(row) + 4, 0);
    return 0xFF;
}

// original 0x5902A0: called by Tactics_Close - window records 12..18 freed.
extern "C" void __cdecl FieldMenu_FreeRecords12To18(void) {
    for (unsigned n = 12; n <= 18; ++n) BRest2f(Rec(n, 0)) = 0;
}

// original 0x5902D0: FieldMenu_States[8] - jmp through FieldMenu_State8Steps
// (four bare rets) by 0x929F01, unchecked (ours aborts past its 4).
extern "C" void __cdecl FieldMenu_State8Run(void) {
    Run("FieldMenu_State8Run (0x5902D0)", FieldMenu_State8Steps, FieldMenu_State8Steps_count, FieldS_State, "0x929F01");
}

void FieldAbility2_Inject() {
    // The solo-character members screens' reserve model (group MS), as solo.cpp runs Solo_SelfTest.
    if (bof3::IsExeHost() ? bof3::os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : bof3::WantsShadow("solo_screens"))
        solo_screens::SelfTest();
    BOF3_INJECT(FieldMenuItems_ByState);
    BOF3_INJECT(FieldMenuItems_Open);
    BOF3_INJECT(FieldMenuItems_SlideIn);
    BOF3_INJECT(FieldMenuItems_Category);
    BOF3_INJECT(FieldMenuItems_List);
    BOF3_INJECT(FieldMenuItems_Close);
    BOF3_INJECT(FieldMenuItems_State5ByStep);
    BOF3_INJECT(FieldItems_ArrangeCategory);
    BOF3_INJECT(FieldItems_ArrangeHow);
    BOF3_INJECT(FieldItems_ArrangeMove);
    BOF3_INJECT(FieldItems_DiscardConfirm);
    BOF3_INJECT(FieldItems_UseOnMember);
    BOF3_INJECT(FieldItems_ViewList32);
    BOF3_INJECT(FieldItems_InitWindows);
    BOF3_INJECT(FieldItems_Sort);
    BOF3_INJECT(FieldMenu_SwapBytes);
    BOF3_INJECT(FieldItemSort_Compact);
    BOF3_INJECT(FieldItemSort_ConsumableFlag1);
    BOF3_INJECT(FieldItemSort_ConsumableFlag2);
    BOF3_INJECT(FieldItemSort_WeaponsByPower);
    BOF3_INJECT(FieldItemSort_ArmourByPower);
    BOF3_INJECT(FieldItemSort_ByIconKind);
    BOF3_INJECT(FieldItemSort_EquipableFirst);
    BOF3_INJECT(FieldItems_CloseWindows);
    BOF3_INJECT(FieldEquip_Run);
    BOF3_INJECT(FieldEquip_Open);
    BOF3_INJECT(FieldMenu_CountdownState);
    BOF3_INJECT(FieldEquip_TopMenu);
    BOF3_INJECT(FieldEquip_PickMember);
    BOF3_INJECT(Equip_ChooseSlot);
    BOF3_INJECT(Equip_ChooseItem);
    BOF3_INJECT(FieldEquip_Close);
    BOF3_INJECT(FieldEquip_RemoveSlot);
    BOF3_INJECT(FieldEquip_InitWindows);
    BOF3_INJECT(FieldEquip_BestByPower);
    BOF3_INJECT(FieldEquip_BestByOrder);
    BOF3_INJECT(FieldEquip_ApplyPreview);
    BOF3_INJECT(FieldEquip_PreviewItem);
    BOF3_INJECT(FieldEquip_PreviewRemove);
    BOF3_INJECT(FieldEquip_CloseWindows);
    BOF3_INJECT(FieldAbility_Run);
    BOF3_INJECT(FieldAbility_Open);
    BOF3_INJECT(FieldAbility_TopMenu);
    BOF3_INJECT(FieldAbility_PickMember);
    BOF3_INJECT(FieldAbility_PickAbility);
    BOF3_INJECT(FieldAbility_PickTarget);
    BOF3_INJECT(FieldAbility_ShareConfirm);
    BOF3_INJECT(FieldAbility_ArrangeRun);
    BOF3_INJECT(FieldAbility_ArrangeMember);
    BOF3_INJECT(FieldAbility_ArrangeHow);
    BOF3_INJECT(FieldAbility_ArrangeMove);
    BOF3_INJECT(FieldAbility_Close);
    BOF3_INJECT(FieldAbility_ViewRun);
    BOF3_INJECT(FieldAbility_ViewOpen);
    BOF3_INJECT(FieldAbility_ViewWait);
    BOF3_INJECT(FieldAbility_ViewBrowse);
    BOF3_INJECT(FieldAbility_ViewLeave);
    BOF3_INJECT(FieldAbility_ViewEnd);
    BOF3_INJECT(AbilityMenu_InitRecords);
    BOF3_INJECT(AbilityList_SortBy);
    BOF3_INJECT(AbilityList_Compact);
    BOF3_INJECT(AbilityList_SortByApDown);
    BOF3_INJECT(AbilityList_SortByApUp);
    BOF3_INJECT(AbilityMenu_InitRecord17);
    BOF3_INJECT(FieldMenu_FreeRecords13To18);
    BOF3_INJECT(Tactics_Run);
    BOF3_INJECT(Tactics_Open);
    BOF3_INJECT(Tactics_Top);
    BOF3_INJECT(Tactics_Formation);
    BOF3_INJECT(TacticsFormation_Enter);
    BOF3_INJECT(TacticsFormation_Pick);
    BOF3_INJECT(TacticsFormation_Leave);
    BOF3_INJECT(Tactics_Members);
    BOF3_INJECT(TacticsMembers_Enter);
    BOF3_INJECT(TacticsMembers_Pick);
    BOF3_INJECT(TacticsMembers_Leave);
    BOF3_INJECT(Tactics_Close);
    BOF3_INJECT(Tactics_OpenFormation);
    BOF3_INJECT(TacticsFormation_Build);
    BOF3_INJECT(TacticsMembers_Build);
    BOF3_INJECT(TacticsMembers_Swap);
    BOF3_INJECT(FieldMenu_FreeRecords12To18);
    BOF3_INJECT(FieldMenu_State8Run);
}

void FieldAbility2FromRest2e_Inject() {
    {
        // DIV-0189: armed after the self-test, which compares the two
        // consumable sorts with Capcom's partition.
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("ItemSortById", Low32(&rest_2e::g_sort_by_id), &was, &is, 1);
        bof3::Log("DIV-0189    the Items screen's NormalItem / CombatItem sorts order each half by item id: %s",
                  rest_2e::g_sort_by_id ? "on" : "OFF (BOF3X_ORIGINAL=ItemSortById)");
    }
}
