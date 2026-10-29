// Group R2E of round fourteen (wave two): three of the field menu's screens,
// 49 functions at 0x58B1D0..0x58ED3F - the cut's 48 rows (analysis/round14_cut.tsv)
// and the start in their span no list had (0x58CFC0), each read to its last
// instruction with capstone (2026-10-04) and fuzzed through the scenario
// harness's field mode (rest_2e_fuzz.cpp). docs/rest_2e.md has them one row
// each.
//
//   - The Items screen (FieldMenu_States[2], whose dispatcher 0x58AAE0 and
//     state table 0x667328 are R2D's): the three arrange steps R2D's 0x58B1C0
//     jumps to through FieldMenuItems_State5Steps, the discard prompt (state 6),
//     the use-on-a-member state (7), a 32-entry list view (9), two window
//     helpers, and FieldItems_Sort with its seven sorts.
//   - The Equipment screen (FieldMenu_States[4]): FieldEquip_Run and its
//     states but FS's two choosers, the "best equipment" previews and their
//     apply, the remove-slot state.
//   - The Ability screen (FieldMenu_States[3]): FieldAbility_Run, its states,
//     and the arrange and view step machines under states 7 and 9.
//
// The menu block 0x929F00 (mode, state +1, step +2, timer +4, cursor bytes) and
// WindowRecords 0x803160 (22 of 0x24) are the cells; the names say what the
// code does, not what the player sees (no gameplay fact from memory).
//
// Every one is a faithful replacement. Where the original indexes a .data
// table by a byte it never bounds (the dispatchers' state and step bytes,
// FieldItems_Sort's argument) ours aborts with a message past the table's own
// length (round9 doc section 6); where it reads an image table in place by a
// byte (help words, the name tables, Character_IndexById) ours reads the
// same bytes in place. The inventory's list pointers (Inventory_IdLists /
// Inventory_CountLists by a category byte) are read in place for the five
// categories; past them, or category 4's null count list, ours aborts.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for the callees; each call's answer is taken into a
// local before the next memory read, in the original's order.
#include "game/rest_2e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

namespace at = rest_2e::at;
using U = std::uint32_t;
using UC = unsigned char;
using move_script::At;
using Handler = void (__cdecl*)();

UC& B(U address) { return *At(address); }
U W(U address) { return move_script::Word(At(address)); }
void SetW(U address, U v) { move_script::SetWord(At(address), v); }
U L(U address) { return static_cast<U>(move_script::Long(At(address))); }
int S8(U v) { return static_cast<signed char>(static_cast<UC>(v)); }
// A .data table symbols.gen.h declares as a pointer: its address.
U AddrOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the cells --------------------------------------------------------------------------

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
// kEquipCursor: now Rest2e_EquipCursor (symbols.toml). FieldEquip_TopMenu's cursor
// kAbilityResult: now Rest2e_AbilityResult (symbols.toml). the last ability's answer
// kAbilityCursor: now Rest2e_AbilityCursor (symbols.toml). FieldAbility_TopMenu's cursor

// --- the helpers ------------------------------------------------------------------------

void Backdrop() { SH_CALL(Menu_DrawBackdrop)(Config_Background); }
void Sound(U id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
U AutoRepeat(U pressed) { return SH_CALL(Input_AutoRepeat)(pressed); }
UC PartyCount() { return static_cast<UC>(SH_CALL(Party_Count)(0)); }
bool Confirmed(U pressed) { return (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed & 0xFFFF) != 0; }
bool Cancelled(U pressed) { return (static_cast<std::uint32_t>(Field_CancelButtons) & pressed & 0xFFFF) != 0; }
// A party member's record index: Character_IndexById[the member's id].
UC RecordOf(U id) { return Character_IndexById[id & 0xFF]; }
UC PartyRecord(int member) { return RecordOf(B(::bof3::Img(kParty.v) + member)); }
unsigned char* Record(U index) { return At(::bof3::Img(CharacterRecords_addr) + (index & 0xFF) * kRecordStride); }
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
    return At(L(::bof3::Img(kIdLists.v) + 4 * category));
}
// The count list's address as the original reads it (0 for category 4).
U CountListAt(U category, const char* who) {
    if (category > 4) bof3::Fatal("%s: category %u reads past Inventory_CountLists (the original reads on)", who, (unsigned)category);
    return L(::bof3::Img(kCountLists.v) + 4 * category);
}
// Entry `index` of a count list at its access: category 4's null list aborts
// here, where the original faults - not where the list is taken.
unsigned char* CountAt(U list, U index, U category, const char* who) {
    if (list == 0) bof3::Fatal("%s: category %u's count list is a null pointer (the original faults)", who, (unsigned)category);
    return At(list + index);
}
unsigned char* CountList(U category, const char* who) { return CountAt(CountListAt(category, who), 0, category, who); }

// A .data table's entry: past its count the original jumps through whatever
// follows; ours aborts.
U Entry(U table, unsigned count, U index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: index %u past its %u-entry table 0x%X (the original jumps through the next word)", who,
                    (unsigned)index, count, (unsigned)table);
    return L(table + 4 * index);
}

void Swap(unsigned char* a, unsigned char* b) { SH_CALL(FieldMenu_SwapBytes)(a, b); }

const unsigned char* AbilityList(U member, U type) { return SH_CALL(Char_AbilityList)(member & 0xFF, type & 0xFF, 0); }
UC AbilityUse(U user, U target, U ability) {
    return static_cast<UC>(SH_AT(UC (__cdecl*)(unsigned, unsigned, unsigned, unsigned), at::kAbilityUse.v)(user, target, ability, 0));
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
        B(s.scroll) = static_cast<UC>(v);
    else
        SetW(s.scroll, v);
}
void Scroll(U repeat, const ListScroll& s, UC& cl, UC& al) {
    cl = B(s.cursor);
    if (repeat & 0x1000) {
        if (cl > 0) {
            --cl;
            B(s.cursor) = cl;
        }
        al = B(s.top);
        if (cl < al) SetScroll(s, 0xF0);
    } else if (repeat & 0x4000) {
        if (cl < s.max) {
            ++cl;
            B(s.cursor) = cl;
        }
        al = B(s.top);
        if (static_cast<int>(cl) >= static_cast<int>(al) + 9) SetScroll(s, 0x10);
    } else if (repeat & Sigma(4)) {
        al = B(s.top);
        if (al == 0) {
            cl = 0;
            B(s.cursor) = 0;
        } else if (al < 9) {
            cl = static_cast<UC>(cl - al);
            al = 0;
            B(s.cursor) = cl;
            B(s.top) = al;
        } else {
            cl = static_cast<UC>(cl - 9);
            al = static_cast<UC>(al - 9);
            B(s.cursor) = cl;
            B(s.top) = al;
        }
    } else {
        al = B(s.top);
        if (repeat & Sigma(8)) {
            if (al == s.last) {
                cl = s.max;
                B(s.cursor) = cl;
            } else if (al > static_cast<UC>(s.last - 9)) {
                cl = static_cast<UC>(cl + static_cast<UC>(s.last - al));
                al = s.last;
                B(s.cursor) = cl;
                B(s.top) = al;
            } else {
                cl = static_cast<UC>(cl + 9);
                al = static_cast<UC>(al + 9);
                B(s.cursor) = cl;
                B(s.top) = al;
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
    UC cl = B(cell);
    if (repeat & 0x1000) {
        --cl;
        B(cell) = cl;
        if (S8(cl) < 0) {
            cl = static_cast<UC>(count - 1);
            B(cell) = cl;
        }
    } else if (repeat & 0x4000) {
        ++cl;
        B(cell) = cl;
        if (S8(cl) > static_cast<int>(count) - 1) {
            cl = 0;
            B(cell) = 0;
        }
    }
    return cl;
}

// The four-entry cursor of FieldEquip_TopMenu and FieldAbility_TopMenu: 0x8000
// down then 0x2000 up (both tested), s8 wrapping through 0..3; cl as left.
UC TurnFour(U cell, U repeat) {
    UC cl = B(cell);
    if (repeat & 0x8000) {
        --cl;
        B(cell) = cl;
        if (S8(cl) < 0) {
            cl = 3;
            B(cell) = cl;
        }
    }
    if (repeat & 0x2000) {
        ++cl;
        B(cell) = cl;
        if (S8(cl) > 3) {
            cl = 0;
            B(cell) = cl;
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
        if (S8(cl) < 0) {
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
        if (S8(m) < 0) Window16_Owner = static_cast<UC>(count - 1);
    } else if (repeat & 0x4000) {
        Sound(0x100);
        const auto m = static_cast<UC>(Window16_Owner + 1);
        Window16_Owner = m;
        if (static_cast<int>(m) > static_cast<int>(count) - 1) Window16_Owner = 0;
    }
}

}  // namespace

// ===========================================================================
// The Items screen
// ===========================================================================

// original 0x58B1D0: FieldMenuItems_State5Steps[0] - the category.
extern "C" void __cdecl FieldItems_ArrangeCategory(void) {
    Backdrop();
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0xF00C);
    const U x = L(::bof3::Img(0x803338)) + 0x2A;
    const U y = W(::bof3::Img(0x80333A)) + 7;
    SetW(::bof3::Img(0x8032FC), 0x17);
    WindowRecords[21].in_use = 1;
    SetW(::bof3::Img(0x803458), x);
    SetW(::bof3::Img(0x80345A), y);
    const U repeat = AutoRepeat(pressed);
    B(::bof3::Img(0x803340)) = 0xFF;
    bool turned = false;
    U category = 0;
    if (repeat & 0x8000) {
        Sound(0x101);
        const auto c = static_cast<UC>(B(::bof3::Img(0x80333E)) - 1);
        SetW(::bof3::Img(0x803344), 0x32);
        B(::bof3::Img(0x80333E)) = c;
        if (S8(c) < 0) B(::bof3::Img(0x80333E)) = 3;
        category = B(::bof3::Img(0x80333E));
        turned = true;
    } else if (repeat & 0x2000) {
        Sound(0x101);
        auto c = static_cast<UC>(B(::bof3::Img(0x80333E)) + 1);
        SetW(::bof3::Img(0x803344), 0x31);
        B(::bof3::Img(0x80333E)) = c;
        if (c > 3) {
            c = 0;
            B(::bof3::Img(0x80333E)) = c;
        }
        category = c;
        turned = true;
    }
    if (turned) B(::bof3::Img(0x80333F)) = B(::bof3::Img(kItemTops.v) + category);
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        Sound(0x103);
        B(::bof3::Img(0x803362)) = B(::bof3::Img(0x80333E));
        const auto step = static_cast<UC>(FieldS_Step + 1);
        B(::bof3::Img(0x803359)) = 6;
        B(::bof3::Img(0x80335A)) = 0xF;
        B(::bof3::Img(0x803358)) = 1;
        SetW(::bof3::Img(0x80335C), 0xB4);
        SetW(::bof3::Img(0x80335E), 0x3C);
        B(::bof3::Img(0x803363)) = 0;
        B(::bof3::Img(0x803364)) = 0xFF;
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
    const U row = B(::bof3::Img(0x803363));
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    const U x = L(::bof3::Img(0x80335C)) + 5;
    const U y = (row << 4) + W(::bof3::Img(0x80335E)) + 0x1A;
    SetW(::bof3::Img(0x8032FC), 0x18);
    WindowRecords[21].in_use = 1;
    SetW(::bof3::Img(0x803458), x);
    SetW(::bof3::Img(0x80345A), y);
    B(::bof3::Img(0x803340)) = 0xFF;
    B(::bof3::Img(0x803364)) = 0xFF;
    const U repeat = AutoRepeat(pressed);
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto c = static_cast<UC>(B(::bof3::Img(0x803363)) - 1);
        B(::bof3::Img(0x803363)) = c;
        if (S8(c) < 0) B(::bof3::Img(0x803363)) = static_cast<UC>(B(AddrOf(FieldItems_ArrangeRows) + 6u * B(::bof3::Img(0x803362))) - 1);
    } else if (repeat & 0x4000) {
        Sound(0x100);
        const auto c = static_cast<UC>(B(::bof3::Img(0x803363)) + 1);
        const int last = static_cast<int>(B(AddrOf(FieldItems_ArrangeRows) + 6u * B(::bof3::Img(0x803362)))) - 1;
        B(::bof3::Img(0x803363)) = c;
        if (static_cast<int>(c) > last) B(::bof3::Img(0x803363)) = 0;
    }
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        Sound(0x103);
        const UC how = B(AddrOf(FieldItems_ArrangeRows) + 1 + 6u * B(::bof3::Img(0x803362)) + B(::bof3::Img(0x803363)));
        if (how != 0) {
            SH_CALL(FieldItems_Sort)(how);
            return;
        }
        B(::bof3::Img(0x803341)) = 0xFF;
        const UC cursor = B(::bof3::Img(kItemCursors.v) + B(::bof3::Img(0x80333E)));
        const auto step = static_cast<UC>(FieldS_Step + 1);
        B(::bof3::Img(0x803340)) = cursor;
        FieldS_Step = step;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto step = static_cast<UC>(FieldS_Step - 1);
    B(::bof3::Img(0x803358)) = 0;
    FieldS_Step = step;
}

// original 0x58B4B0: FieldMenuItems_State5Steps[2] - an entry picked and swapped
// with another, the list scrolled.
extern "C" void __cdecl FieldItems_ArrangeMove(void) {
    Backdrop();
    const U category = B(::bof3::Img(0x80333E));
    const UC item = IdList(category, "FieldItems_ArrangeMove")[B(::bof3::Img(0x803340))];
    const U help = SH_CALL(Item_HelpMessage)(category, item);
    const U top = B(::bof3::Img(0x80333F));
    SetW(::bof3::Img(0x8032FC), help);
    const U x = L(::bof3::Img(0x803338)) + 7;
    B(::bof3::Img(0x803364)) = 0;
    SetW(::bof3::Img(0x803458), x);
    WindowRecords[21].in_use = 1;
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0x500C);
    WindowRecords[21].y = 13u * (B(::bof3::Img(0x803340)) - top + 2) + W(::bof3::Img(0x80333A));
    const U repeat = AutoRepeat(pressed);
    const UC was = B(::bof3::Img(0x803340));
    UC cl, al;
    Scroll(repeat, kItemScroll, cl, al);
    const U now = B(::bof3::Img(0x80333E));
    B(::bof3::Img(kItemCursors.v) + now) = cl;
    B(::bof3::Img(kItemTops.v) + now) = al;
    if (was != cl) Sound(0x100);
    if (W(::bof3::Img(0x803346)) != 0) return;
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        if (B(::bof3::Img(0x803341)) == 0xFF) {
            Sound(0x103);
            if (IdList(B(::bof3::Img(0x80333E)), "FieldItems_ArrangeMove")[B(::bof3::Img(0x803340))] != 0) {
                B(::bof3::Img(0x803341)) = B(::bof3::Img(0x803340));
                return;
            }
            Sound(0x107);
            return;
        }
        Sound(0x103);
        {
            unsigned char* const ids = IdList(B(::bof3::Img(0x80333E)), "FieldItems_ArrangeMove");
            const U cursor = B(::bof3::Img(0x803340));
            Swap(ids + B(::bof3::Img(0x803341)), ids + cursor);
        }
        {
            const U cursor = B(::bof3::Img(0x803340));
            unsigned char* const counts = CountList(B(::bof3::Img(0x80333E)), "FieldItems_ArrangeMove");
            Swap(counts + B(::bof3::Img(0x803341)), counts + cursor);
        }
        B(::bof3::Img(0x803341)) = 0xFF;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    if (B(::bof3::Img(0x803341)) != 0xFF) {
        B(::bof3::Img(0x803341)) = 0xFF;
        return;
    }
    FieldS_Step = static_cast<UC>(FieldS_Step - 1);
}

// original 0x58B750: the Items screen's state 6 - the yes / no to discard.
extern "C" void __cdecl FieldItems_DiscardConfirm(void) {
    Backdrop();
    const U category = B(::bof3::Img(0x80333E));
    const UC item = IdList(category, "FieldItems_DiscardConfirm")[B(::bof3::Img(0x803340))];
    const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
    SH_CALL(TextRecord_Set)(0, 0x10, name);
    const int answer = FieldS_Answer;
    const U x = L(::bof3::Img(0x8032F0)) - static_cast<U>(36 * answer) + 0xE8;
    const U y = W(::bof3::Img(0x8032F2)) + 5;
    U pressed = L(::bof3::Img(kPressed.v));
    SetW(::bof3::Img(0x8032FC), 0x1A);
    WindowRecords[21].in_use = 1;
    SetW(::bof3::Img(0x803458), x);
    SetW(::bof3::Img(0x80345A), y);
    if (pressed & 0xA000) {
        Sound(0x101);
        FieldS_Answer = static_cast<UC>(static_cast<unsigned char>(FieldS_Answer) ^ 1);
        pressed = L(::bof3::Img(kPressed.v));
    }
    if (Confirmed(pressed)) {
        if (static_cast<unsigned char>(FieldS_Answer) != 0) {
            Sound(0x104);
            IdList(B(::bof3::Img(0x80333E)), "FieldItems_DiscardConfirm")[B(::bof3::Img(0x803340))] = 0;
            CountList(B(::bof3::Img(0x80333E)), "FieldItems_DiscardConfirm")[B(::bof3::Img(0x803340))] = 0;
            const auto state = static_cast<UC>(FieldS_State - 3);
            B(::bof3::Img(0x803341)) = 0xFF;
            FieldS_State = state;
            return;
        }
    } else if (!Cancelled(pressed)) {
        return;
    }
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State - 3);
    B(::bof3::Img(0x803341)) = 0xFF;
    FieldS_State = state;
}

// original 0x58B8A0: the Items screen's state 7 - a consumable used on a member.
extern "C" void __cdecl FieldItems_UseOnMember(void) {
    Backdrop();
    const U category = B(::bof3::Img(0x80333E));
    const U index = B(::bof3::Img(0x803340));
    unsigned char* const id = IdList(category, "FieldItems_UseOnMember") + index;
    const U counts = CountListAt(category, "FieldItems_UseOnMember");
    SH_CALL(TextRecord_Set)(0, 0x10, At(::bof3::Img(kConsumables.v) + 22u * *id));
    const int member = BattleActorCopies_ItemActor;
    SetW(::bof3::Img(0x8032FC), 0x19);
    WindowRecords[19].in_use = 1;
    SetW(::bof3::Img(0x803410), 0xB6);
    WindowRecords[19].y = static_cast<U>(54 * member + 0x3E);
    const UC n = PartyCount();
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
        const UC record = PartyRecord(S8(cl));
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
    B(::bof3::Img(0x803341)) = 0xFF;
    FieldS_State = state;
}

// original 0x58BA40: the Items screen's state 9 - a 32-entry list scrolled.
extern "C" void __cdecl FieldItems_ViewList32(void) {
    Backdrop();
    const U category = B(::bof3::Img(0x80333E));
    const UC item = IdList(category, "FieldItems_ViewList32")[B(::bof3::Img(0x803340))];
    const U help = SH_CALL(Item_HelpMessage)(category, item);
    const U top = B(::bof3::Img(0x80333F));
    SetW(::bof3::Img(0x8032FC), help);
    const U x = L(::bof3::Img(0x803338)) + 7;
    WindowRecords[21].in_use = 1;
    SetW(::bof3::Img(0x803458), x);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0x500C);
    WindowRecords[21].y = 13u * (B(::bof3::Img(0x803340)) - top + 2) + W(::bof3::Img(0x80333A));
    const U repeat = AutoRepeat(pressed);
    const UC was = B(::bof3::Img(0x803340));
    UC cl, al;
    Scroll(repeat, kShortScroll, cl, al);
    const U now = B(::bof3::Img(0x80333E));
    B(::bof3::Img(kItemCursors.v) + now) = cl;
    B(::bof3::Img(kItemTops.v) + now) = al;
    if (was != cl) Sound(0x100);
    if (W(::bof3::Img(0x803346)) != 0) return;
    if (!Cancelled(static_cast<std::uint32_t>(Input_Pressed))) return;
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State - 7);
    const UC t = B(::bof3::Img(kItemTops.v));
    FieldS_State = state;
    B(::bof3::Img(0x80333E)) = 0;
    B(::bof3::Img(0x80333F)) = t;
    B(::bof3::Img(0x803340)) = 0xFF;
    B(::bof3::Img(0x803341)) = 0xFF;
}

// original 0x58BC30: the Items screen's windows.
extern "C" void __cdecl FieldItems_InitWindows(void) {
    B(::bof3::Img(0x8032EF)) = 2;
    B(::bof3::Img(0x803313)) = 2;
    B(::bof3::Img(0x803337)) = 2;
    B(::bof3::Img(0x80333F)) = B(::bof3::Img(kItemTops.v));
    B(::bof3::Img(0x803340)) = 0xFF;
    B(::bof3::Img(0x803341)) = 0xFF;
    B(::bof3::Img(0x8032ED)) = 6;
    B(::bof3::Img(0x8032EE)) = 9;
    B(::bof3::Img(0x8032EC)) = 1;
    SetW(::bof3::Img(0x8032F0), 0x14);
    SetW(::bof3::Img(0x8032F2), 0xFFEC);
    SetW(::bof3::Img(0x8032FC), 0);
    B(::bof3::Img(0x803311)) = 6;
    B(::bof3::Img(0x803312)) = 0xD;
    B(::bof3::Img(0x803310)) = 1;
    SetW(::bof3::Img(0x803314), 0x40);
    SetW(::bof3::Img(0x803316), 0xFFEC);
    B(::bof3::Img(0x80331A)) = 0;
    B(::bof3::Img(0x80331B)) = 0xFF;
    B(::bof3::Img(0x803335)) = 6;
    B(::bof3::Img(0x803336)) = 0xC;
    B(::bof3::Img(0x803334)) = 1;
    SetW(::bof3::Img(0x803338), 0xFF38);
    SetW(::bof3::Img(0x80333A), 0x3E);
    B(::bof3::Img(0x80333E)) = 0;
    B(::bof3::Img(0x80333C)) = 0;
    B(::bof3::Img(0x80333D)) = 0;
    SetW(::bof3::Img(0x803344), 0);
    SetW(::bof3::Img(0x803346), 0);
    B(::bof3::Img(0x80340D)) = 6;
    B(::bof3::Img(0x80340E)) = 0xA;
    WindowRecords[19].in_use = 0;
    B(::bof3::Img(0x803416)) = 1;
    B(::bof3::Img(0x803417)) = 1;
    B(::bof3::Img(0x803455)) = 6;
    B(::bof3::Img(0x803456)) = 0xE;
    B(::bof3::Img(0x803457)) = 0;
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
        const U category = B(::bof3::Img(0x80333E));
        unsigned char* const ids = IdList(category, "FieldItemSort_Compact");
        const U counts = L(::bof3::Img(kCountLists.v) + 4 * category);
        for (U i = 0; i < bound; ++i) {
            if (ids[i] != 0 || ids[i + 1] == 0) continue;
            Swap(ids + i, ids + i + 1);
            if (counts == 0) bof3::Fatal("FieldItemSort_Compact: category %u's count list is a null pointer (the original faults)", (unsigned)category);
            Swap(At(counts + i), At(counts + i + 1));
        }
    }
}

namespace {

// The bubble pass of the five fixed-list sorts: 127 passes over the list at
// `ids` (its counts 0x200 on), a pair (x, y) of non-empty entries exchanged
// when `before(x, y)` says y goes first.
template <typename Before> void BubbleFixed(U ids, Before before) {
    for (U bound = 0x7F; bound != 0; --bound)
        for (U i = 0; i < bound; ++i) {
            const UC x = B(ids + i);
            if (x == 0) continue;
            const UC y = B(ids + i + 1);
            if (y == 0) continue;
            if (!before(x, y)) continue;
            Swap(At(ids + i), At(ids + i + 1));
            Swap(At(ids + i + 0x200), At(ids + i + 0x201));
        }
}
constexpr ::bof3::image::Va kConsumableIds{0x904154};   // Inventory_IdLists[0]; the counts 0x904354
constexpr ::bof3::image::Va kWeaponIds{0x9041D4};       // [1]
constexpr ::bof3::image::Va kArmourIds{0x904254};       // [2]

}  // namespace

// original 0x58BE00: FieldItems_Sorts[1] - consumables with flag bit 0 first.
extern "C" void __cdecl FieldItemSort_ConsumableFlag1(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(::bof3::Img(kConsumableIds.v), [](UC x, UC y) {
        return (B(::bof3::Img(kConsumableFlags.v) + 22u * x) & 1) == 0 && (B(::bof3::Img(kConsumableFlags.v) + 22u * y) & 1) != 0;
    });
}

// original 0x58BEB0: FieldItems_Sorts[2] - consumables with flag bit 1 first.
extern "C" void __cdecl FieldItemSort_ConsumableFlag2(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(::bof3::Img(kConsumableIds.v), [](UC x, UC y) {
        return (B(::bof3::Img(kConsumableFlags.v) + 22u * x) & 2) == 0 && (B(::bof3::Img(kConsumableFlags.v) + 22u * y) & 2) != 0;
    });
}

// original 0x58BF60: FieldItems_Sorts[3] - weapons by +0x16, the highest first.
extern "C" void __cdecl FieldItemSort_WeaponsByPower(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(::bof3::Img(kWeaponIds.v), [](UC x, UC y) { return B(::bof3::Img(kWeaponB.v) + 28u * x) < B(::bof3::Img(kWeaponB.v) + 28u * y); });
}

// original 0x58C010: FieldItems_Sorts[4] - armour by +0x14, the highest first.
extern "C" void __cdecl FieldItemSort_ArmourByPower(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(::bof3::Img(kArmourIds.v), [](UC x, UC y) { return B(::bof3::Img(kArmourB.v) + 26u * x) < B(::bof3::Img(kArmourB.v) + 26u * y); });
}

// original 0x58C0B0: FieldItems_Sorts[5] - the category's list by Item_IconKind.
extern "C" void __cdecl FieldItemSort_ByIconKind(void) {
    SH_CALL(FieldItemSort_Compact)();
    const UC category = B(::bof3::Img(0x80333E));
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
            Swap(CountAt(counts, i, category, "FieldItemSort_ByIconKind"), At(counts + i + 1));
        }
    }
}

// original 0x58C1A0: FieldItems_Sorts[6] - the entries member s8 0x929F06 can
// equip first.
extern "C" void __cdecl FieldItemSort_EquipableFirst(void) {
    SH_CALL(FieldItemSort_Compact)();
    const UC record = PartyRecord(BattleActorCopies_ItemActor);
    const UC category = B(::bof3::Img(0x80333E));
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
            Swap(CountAt(counts, i, category, "FieldItemSort_EquipableFirst"), At(counts + i + 1));
        }
    }
}

// original 0x58C2A0: three of the Items screen's windows closed.
extern "C" void __cdecl FieldItems_CloseWindows(void) {
    B(::bof3::Img(0x803310)) = 0;
    B(::bof3::Img(0x803334)) = 0;
    B(::bof3::Img(0x803358)) = 0;
}

// ===========================================================================
// The Equipment screen
// ===========================================================================

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
    B(::bof3::Img(kTimer.v)) = 5;
    FieldS_State = state;
}

// original 0x58C310: FieldEquip_States[1] and [4], FieldAbility_States[1].
extern "C" void __cdecl FieldMenu_CountdownState(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(::bof3::Img(kTimer.v)) - 1);
    B(::bof3::Img(kTimer.v)) = t;
    if (t == 0) FieldS_State = static_cast<UC>(FieldS_State + 1);
}

// original 0x58C340: FieldEquip_States[2] - the four-entry menu.
extern "C" void __cdecl FieldEquip_TopMenu(void) {
    Backdrop();
    const int c = Rest2e_EquipCursor;
    const U help = W(static_cast<U>(static_cast<int>(AddrOf(FieldEquip_TopHelp)) + 2 * c));
    const U x = L(::bof3::Img(0x803314)) + static_cast<U>(48 * c);
    SetW(::bof3::Img(0x8032FC), help);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0xA000;
    WindowRecords[21].in_use = 1;
    const U y = W(::bof3::Img(0x803316)) + 4;
    SetW(::bof3::Img(0x803458), x);
    SetW(::bof3::Img(0x80345A), y);
    B(::bof3::Img(0x80331B)) = 0xFF;
    B(::bof3::Img(0x803341)) = 1;
    const U repeat = AutoRepeat(pressed);
    const UC was = static_cast<unsigned char>(Rest2e_EquipCursor);
    const UC cl = TurnFour(::bof3::Img(Rest2e_EquipCursor_addr), repeat);
    if (was != cl) Sound(0x101);
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        Sound(0x104);
        B(::bof3::Img(0x80331B)) = static_cast<unsigned char>(Rest2e_EquipCursor);
        const auto state = static_cast<UC>(FieldS_State + 1);
        WindowRecords[21].in_use = 0;
        FieldS_State = state;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x102);
    Sound(0x106);
    const auto state = static_cast<UC>(FieldS_State + 5);
    B(::bof3::Img(0x8032EF)) = 1;
    B(::bof3::Img(0x803313)) = 1;
    B(::bof3::Img(0x803337)) = 1;
    WindowRecords[19].in_use = 0;
    WindowRecords[21].in_use = 0;
    FieldS_State = state;
    B(::bof3::Img(kTimer.v)) = 0;
}

// original 0x58C4A0: FieldEquip_States[3] - the member.
extern "C" void __cdecl FieldEquip_PickMember(void) {
    Backdrop();
    const U m0 = B(::bof3::Img(0x803340));
    const U wx = W(::bof3::Img(0x8031F4));
    WindowRecords[19].in_use = 1;
    SetW(::bof3::Img(0x803410), wx);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    WindowRecords[19].y = 54 * m0 + 0x3E;
    const U repeat = AutoRepeat(pressed);
    const UC was = B(::bof3::Img(0x803340));
    const UC n = PartyCount();
    if (repeat & 0x1000) {
        const auto m = static_cast<UC>(B(::bof3::Img(0x803340)) - 1);
        B(::bof3::Img(0x803340)) = m;
        if (S8(m) < 0) B(::bof3::Img(0x803340)) = static_cast<UC>(n - 1);
    } else if (repeat & 0x4000) {
        const auto m = static_cast<UC>(B(::bof3::Img(0x803340)) + 1);
        B(::bof3::Img(0x803340)) = m;
        if (static_cast<int>(B(::bof3::Img(0x803340))) > static_cast<int>(n) - 1) B(::bof3::Img(0x803340)) = 0;
    }
    if (S8(was) != static_cast<int>(B(::bof3::Img(0x803340)))) Sound(0x101);
    if (static_cast<unsigned char>(Rest2e_EquipCursor) == 1) SH_CALL(FieldEquip_BestByPower)(B(::bof3::Img(0x803340)));
    if (static_cast<unsigned char>(Rest2e_EquipCursor) == 2) SH_CALL(FieldEquip_BestByOrder)(B(::bof3::Img(0x803340)));
    SH_CALL(TextRecord_Set)(0, 8, Record(PartyRecord(B(::bof3::Img(0x803340)))));
    const int c = Rest2e_EquipCursor;
    const U help = W(static_cast<U>(static_cast<int>(AddrOf(FieldEquip_MemberHelp)) + 2 * c));
    const UC flag = B(static_cast<U>(static_cast<int>(AddrOf(FieldEquip_MemberFlags)) + 2 * c));
    const U p = Input_Pressed;
    SetW(::bof3::Img(0x8032FC), help);
    B(::bof3::Img(0x803341)) = flag;
    if (Confirmed(p)) {
        Sound(0x103);
        const UC cursor = static_cast<unsigned char>(Rest2e_EquipCursor);
        if (cursor == 0) {
            Sound(0x102);
            UC k = 0;
            if (PartyCount() != 0) {
                do {
                    B(::bof3::Img(0x8031F3) + 0x24u * k) = 7;
                    ++k;
                } while (k < PartyCount());
            }
            const UC m = B(::bof3::Img(0x803340));
            WindowRecords[19].in_use = 0;
            B(::bof3::Img(0x803337)) = 3;
            B(::bof3::Img(0x80333E)) = 0;
            B(::bof3::Img(0x80333F)) = 0xFF;
            B(::bof3::Img(0x803359)) = 6;
            B(::bof3::Img(0x803362)) = B(::bof3::Img(kEquipTops.v) + 6u * m);
            const auto state = static_cast<UC>(FieldS_State + 1);
            B(::bof3::Img(0x80335A)) = 0x12;
            B(::bof3::Img(0x80335B)) = 4;
            B(::bof3::Img(0x803358)) = 1;
            SetW(::bof3::Img(0x80335C), 0x140);
            SetW(::bof3::Img(0x80335E), 0x3E);
            B(::bof3::Img(0x803364)) = m;
            B(::bof3::Img(0x803363)) = 0xFF;
            B(::bof3::Img(0x803360)) = 0;
            B(::bof3::Img(0x803365)) = 0;
            B(::bof3::Img(0x803361)) = 0;
            SetW(::bof3::Img(0x803368), 0);
            BattleActorCopies_ItemActor = m;
            B(::bof3::Img(kTimer.v)) = 5;
            FieldS_State = state;
            return;
        }
        if (cursor == 3) {
            const auto state = static_cast<UC>(FieldS_State + 5);
            B(::bof3::Img(0x803417)) = 0;
            B(::bof3::Img(0x80333E)) = 1;
            B(::bof3::Img(0x80333F)) = 0xFF;
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
    B(::bof3::Img(0x937F8E)) = m;
    B(::bof3::Img(kTimer.v)) = 0;
    FieldS_State = state;
}

// original 0x58CD40: FieldEquip_States[7] - the screen closed after 4 frames.
extern "C" void __cdecl FieldEquip_Close(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(::bof3::Img(kTimer.v)) + 1);
    B(::bof3::Img(kTimer.v)) = t;
    if (t != 4) return;
    SH_CALL(FieldEquip_CloseWindows)();
    U k = 0;
    if (PartyCount() != 0) {
        do {
            B(::bof3::Img(0x8031F3) + 0x24u * k) = 4;
            ++k;
        } while (static_cast<int>(k) < static_cast<int>(PartyCount()));
    }
    const UC m = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    B(::bof3::Img(0x80325F)) = 2;
    B(::bof3::Img(0x803283)) = 2;
    B(::bof3::Img(0x8032A7)) = 2;
    B(::bof3::Img(0x8032CB)) = 2;
    B(::bof3::Img(0x937F8E)) = m;
    B(::bof3::Img(kTimer.v)) = 0;
    BattleE1_Mode = 1;
    FieldS_State = 0;
}

// original 0x58CDD0: FieldEquip_States[8] - a slot's item taken off.
extern "C" void __cdecl FieldEquip_RemoveSlot(void) {
    Backdrop();
    unsigned char* const record = Record(PartyRecord(B(::bof3::Img(0x803340))));
    U category;
    unsigned char* cell;
    switch (B(::bof3::Img(0x80333E))) {
    case 1: category = 2; cell = record + 0x13; break;
    case 2: category = 2; cell = record + 0x14; break;
    case 3: category = 2; cell = record + 0x15; break;
    case 4: category = 3; cell = record + 0x16; break;
    case 5: category = 3; cell = record + 0x17; break;
    default: category = 1; cell = record + 0x12; break;
    }
    const U help = SH_CALL(Item_HelpMessage)(category, *cell);
    const U x = L(::bof3::Img(0x803338)) + 6;
    SetW(::bof3::Img(0x8032FC), help);
    const U y = 13u * B(::bof3::Img(0x80333E)) + W(::bof3::Img(0x80333A)) + 0x57;
    WindowRecords[21].in_use = 1;
    SetW(::bof3::Img(0x803458), x);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    SetW(::bof3::Img(0x80345A), y);
    const U repeat = AutoRepeat(pressed);
    if (repeat & 0x4000) {
        Sound(0x101);
        const auto s = static_cast<UC>(B(::bof3::Img(0x80333E)) + 1);
        B(::bof3::Img(0x80333E)) = s;
        if (s > 5) B(::bof3::Img(0x80333E)) = 1;
    } else if (repeat & 0x1000) {
        Sound(0x101);
        const auto s = static_cast<UC>(B(::bof3::Img(0x80333E)) - 1);
        B(::bof3::Img(0x80333E)) = s;
        if (s < 1) B(::bof3::Img(0x80333E)) = 5;
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
    B(::bof3::Img(0x803417)) = 1;
    WindowRecords[21].in_use = 0;
    B(::bof3::Img(0x80333E)) = 0xFF;
    B(::bof3::Img(kTimer.v)) = 0;
    FieldS_State = state;
}

// original 0x58CFC0: the Equipment screen's windows.
extern "C" void __cdecl FieldEquip_InitWindows(void) {
    B(::bof3::Img(0x8032ED)) = 6;
    B(::bof3::Img(0x8032EE)) = 9;
    B(::bof3::Img(0x8032EF)) = 2;
    B(::bof3::Img(0x8032EC)) = 1;
    SetW(::bof3::Img(0x8032F0), 0x14);
    SetW(::bof3::Img(0x8032F2), 0xFFEC);
    SetW(::bof3::Img(0x8032FC), 0);
    B(::bof3::Img(0x803311)) = 6;
    B(::bof3::Img(0x803312)) = 0xD;
    B(::bof3::Img(0x803313)) = 2;
    B(::bof3::Img(0x803310)) = 1;
    SetW(::bof3::Img(0x803314), 0x40);
    SetW(::bof3::Img(0x803316), 0xFFEC);
    B(::bof3::Img(0x80331A)) = 1;
    B(::bof3::Img(0x80331B)) = 0xFF;
    B(::bof3::Img(0x803335)) = 6;
    B(::bof3::Img(0x803336)) = 0x10;
    B(::bof3::Img(0x803337)) = 2;
    B(::bof3::Img(0x803334)) = 1;
    SetW(::bof3::Img(0x803338), 0x140);
    SetW(::bof3::Img(0x80333A), 0x3E);
    B(::bof3::Img(0x803340)) = 0;
    B(::bof3::Img(0x80333E)) = 0xFF;
    B(::bof3::Img(0x80333F)) = 0xFF;
    B(::bof3::Img(0x803341)) = 1;
    move_script::SetLong(At(::bof3::Img(0x803354)), static_cast<std::int32_t>(::bof3::Img(kPreview.v)));
    B(::bof3::Img(0x80340D)) = 6;
    B(::bof3::Img(0x80340E)) = 0xA;
    WindowRecords[19].in_use = 0;
    B(::bof3::Img(0x803416)) = 1;
    B(::bof3::Img(0x803417)) = 1;
    B(::bof3::Img(0x803455)) = 6;
    B(::bof3::Img(0x803456)) = 0xE;
    B(::bof3::Img(0x803457)) = 0;
    WindowRecords[21].in_use = 0;
}

// original 0x58D0C0: the preview of the highest +0x16 weapon and +0x14 armour
// the member can equip.
extern "C" void __cdecl FieldEquip_BestByPower(unsigned member) {
    const UC index = PartyRecord(static_cast<int>(member & 0xFF));
    const UC bit = Bit8(index);
    unsigned char* const record = Record(index);
    B(::bof3::Img(kPreview.v)) = record[0x12];
    U best = B(::bof3::Img(kWeaponB.v) + 28u * B(::bof3::Img(kPreview.v)));
    for (U k = 0; k < 0x80; ++k) {
        const UC id = At(L(::bof3::Img(kIdLists.v) + 4))[k];
        const UC power = B(::bof3::Img(kWeaponB.v) + 28u * id);
        const U mask = SH_CALL(Item_EquipMask)(1, id);
        if ((bit & mask) == 0) continue;
        if (best < power || (best == power && B(::bof3::Img(kPreview.v)) > id)) {
            best = B(::bof3::Img(kWeaponB.v) + 28u * id);
            B(::bof3::Img(kPreview.v)) = id;
        }
    }
    for (U j = 0; j < 3; ++j) {
        const U slot = ::bof3::Img(kPreview.v) + 1 + j;
        B(slot) = record[0x13 + j];
        best = B(::bof3::Img(kArmourB.v) + 26u * B(slot));
        for (U k = 0; k < 0x80; ++k) {
            const UC id = At(L(::bof3::Img(kIdLists.v) + 8))[k];
            const UC type = B(::bof3::Img(kArmourType.v) + 26u * id);
            const UC power = B(::bof3::Img(kArmourB.v) + 26u * id);
            if (type != 2 + j) continue;
            const U mask = SH_CALL(Item_EquipMask)(2, id);
            if ((bit & mask) == 0) continue;
            if (best < power || (best == power && B(slot) > id)) {
                best = B(::bof3::Img(kArmourB.v) + 26u * id);
                B(slot) = id;
            }
        }
    }
    B(::bof3::Img(kPreview.v) + 4) = record[0x16];
    B(::bof3::Img(kPreview.v) + 5) = record[0x17];
}

// original 0x58D2E0: the preview by the lowest +0x14 / +0x13 first, then the
// highest +0x16 / +0x14 (docs/rest_2e.md section 7 for the armour's two quirks).
extern "C" void __cdecl FieldEquip_BestByOrder(unsigned member) {
    const UC index = PartyRecord(static_cast<int>(member & 0xFF));
    const UC bit = Bit8(index);
    unsigned char* const record = Record(index);
    B(::bof3::Img(kPreview.v)) = record[0x12];
    const UC current = B(::bof3::Img(kPreview.v));
    U best_b = B(::bof3::Img(kWeaponB.v) + 28u * current);
    UC best_a = B(::bof3::Img(kWeaponA.v) + 28u * current);
    for (U k = 0; k < 0x80; ++k) {
        const UC id = At(L(::bof3::Img(kIdLists.v) + 4))[k];
        const UC a = B(::bof3::Img(kWeaponA.v) + 28u * id);
        const UC b = B(::bof3::Img(kWeaponB.v) + 28u * id);
        const U mask = SH_CALL(Item_EquipMask)(1, id);
        if ((bit & mask) == 0) continue;
        const UC chosen = B(::bof3::Img(kPreview.v));
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
        best_b = B(::bof3::Img(kWeaponB.v) + 28u * id);
        B(::bof3::Img(kPreview.v)) = id;
        best_a = B(::bof3::Img(kWeaponA.v) + 28u * id);
    }
    for (U j = 0; j < 3; ++j) {
        const U slot = ::bof3::Img(kPreview.v) + 1 + j;
        B(slot) = record[0x13 + j];
        const UC now = B(slot);
        best_b = B(::bof3::Img(kArmourB.v) + 26u * now);
        best_a = B(::bof3::Img(kArmourA.v) + 26u * now);
        for (U k = 0; k < 0x80; ++k) {
            const UC id = At(L(::bof3::Img(kIdLists.v) + 8))[k];
            const UC a = B(::bof3::Img(kArmourA.v) + 26u * id);
            const UC type = B(::bof3::Img(kArmourType.v) + 26u * id);
            const UC b = B(::bof3::Img(kArmourB.v) + 26u * id);
            if (static_cast<int>(type) - 2 != static_cast<int>(j)) continue;
            const U mask = SH_CALL(Item_EquipMask)(2, id);
            if ((bit & mask) == 0) continue;
            bool take;
            if (B(slot) == 0)
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
                take = B(::bof3::Img(kPreview.v)) > id;   // the weapon's byte, not this slot's (Capcom's)
            if (!take) continue;
            const UC kept = B(::bof3::Img(kArmourA.v) + 26u * id);
            B(slot) = id;
            best_a = kept;
            best_b = kept;   // +0x13, not +0x14 (Capcom's)
        }
    }
    B(::bof3::Img(kPreview.v) + 4) = record[0x16];
    B(::bof3::Img(kPreview.v) + 5) = record[0x17];
}

// original 0x58D570: the preview swapped into the member's six slots through
// the inventory.
extern "C" void __cdecl FieldEquip_ApplyPreview(void) {
    unsigned char* const record = Record(PartyRecord(B(::bof3::Img(0x803340))));
    for (U i = 0; i < 6; ++i) {
        const UC item = B(::bof3::Img(kPreview.v) + i);
        if (item == 0) continue;
        unsigned char* const cell = record + 0x12 + i;
        if (item == *cell) continue;
        const UC category = B(AddrOf(FieldEquip_SlotCategories) + i);
        SH_CALL(Inventory_Remove)(category, item, 1);
        const UC old = *cell;
        const UC again = B(AddrOf(FieldEquip_SlotCategories) + i);
        SH_CALL(Inventory_Add)(again, old, 1);
        *cell = B(::bof3::Img(kPreview.v) + i);
    }
    SH_CALL(Char_RecalcStats)(record);
}

// original 0x58D640: the preview with the chosen item in the slot.
extern "C" void __cdecl FieldEquip_PreviewItem(void) {
    const UC item = B(::bof3::Img(0x803365));
    const UC index = PartyRecord(B(::bof3::Img(0x803340)));
    const UC category = B(::bof3::Img(0x803360));
    const U mask = SH_CALL(Item_EquipMask)(category, item);
    B(::bof3::Img(0x803341)) = (Bit8(index) & mask) == 0 ? 1 : 0;
    const unsigned char* const record = Record(index);
    B(::bof3::Img(kPreview.v) + 1) = record[0x13];
    B(::bof3::Img(kPreview.v)) = record[0x12];
    B(::bof3::Img(kPreview.v) + 3) = record[0x15];
    B(::bof3::Img(kPreview.v) + 2) = record[0x14];
    B(::bof3::Img(kPreview.v) + 4) = record[0x16];
    const U slot = B(::bof3::Img(0x80333E));
    B(::bof3::Img(kPreview.v) + 5) = record[0x17];
    B(::bof3::Img(kPreview.v) + slot) = item;
}

// original 0x58D700: the preview with the slot's item taken off; al 1 when
// there was one.
extern "C" unsigned char __cdecl FieldEquip_PreviewRemove(void) {
    const unsigned char* const record = Record(PartyRecord(B(::bof3::Img(0x803340))));
    B(::bof3::Img(kPreview.v)) = record[0x12];
    B(::bof3::Img(kPreview.v) + 1) = record[0x13];
    B(::bof3::Img(kPreview.v) + 2) = record[0x14];
    B(::bof3::Img(kPreview.v) + 3) = record[0x15];
    B(::bof3::Img(kPreview.v) + 5) = record[0x17];
    const U slot = B(::bof3::Img(0x80333E));
    B(::bof3::Img(kPreview.v) + 4) = record[0x16];
    if (B(::bof3::Img(kPreview.v) + slot) != 0) {
        B(::bof3::Img(kPreview.v) + slot) = 0;
        B(::bof3::Img(0x803341)) = 0;
        return 1;
    }
    B(::bof3::Img(0x803341)) = 1;
    return 0;
}

// original 0x58D7B0: two of the Equipment screen's windows closed.
extern "C" void __cdecl FieldEquip_CloseWindows(void) {
    B(::bof3::Img(0x803334)) = 0;
    B(::bof3::Img(0x803358)) = 0;
}

// ===========================================================================
// The Ability screen
// ===========================================================================

// original 0x58D7C0: FieldMenu_States[3], a tail jump through
// FieldAbility_States by the state byte, unchecked.
extern "C" void __cdecl FieldAbility_Run(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(Entry(AddrOf(FieldAbility_States), 10, FieldS_State, "FieldAbility_Run"))();
}

// original 0x58D7D0: FieldAbility_States[0].
extern "C" void __cdecl FieldAbility_Open(void) {
    Backdrop();
    SH_AT(Handler, at::kAbilityWindows.v)();
    Sound(0x102);
    const auto state = static_cast<UC>(FieldS_State + 1);
    const UC member = B(::bof3::Img(0x905BA1));
    B(::bof3::Img(kTimer.v)) = 5;
    BattleActorCopies_ItemActor = member;
    Rest2e_AbilityCursor = 0;
    FieldS_State = state;
}

// original 0x58D820: FieldAbility_States[2] - the four-entry menu.
extern "C" void __cdecl FieldAbility_TopMenu(void) {
    Backdrop();
    const int c = static_cast<signed char>(Rest2e_AbilityCursor);
    const U help = W(static_cast<U>(static_cast<int>(AddrOf(FieldAbility_TopHelp)) + 4 * c));
    const U x = L(::bof3::Img(0x803314)) + static_cast<U>(48 * c);
    SetW(::bof3::Img(0x8032FC), help);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0xA000;
    B(::bof3::Img(0x8033A8)) = 0;
    const U y = W(::bof3::Img(0x803316)) + 4;
    WindowRecords[21].in_use = 1;
    SetW(::bof3::Img(0x803458), x);
    SetW(::bof3::Img(0x80345A), y);
    B(::bof3::Img(0x80331B)) = 0xFF;
    const U repeat = AutoRepeat(pressed);
    const UC was = Rest2e_AbilityCursor;
    UC cl = TurnFour(::bof3::Img(Rest2e_AbilityCursor_addr), repeat);
    if (was != cl) {
        Sound(0x101);
        cl = Rest2e_AbilityCursor;
    }
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        B(::bof3::Img(0x80331B)) = cl;
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
            B(::bof3::Img(kTimer.v)) = 7;
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
    B(::bof3::Img(0x8032EF)) = 1;
    B(::bof3::Img(0x803313)) = 1;
    WindowRecords[16].state = 1;
    WindowRecords[21].in_use = 0;
    FieldS_State = state;
    B(::bof3::Img(kTimer.v)) = 5;
}

// original 0x58D9C0: FieldAbility_States[3] - the member and the list type.
extern "C" void __cdecl FieldAbility_PickMember(void) {
    Backdrop();
    const UC cursor = Rest2e_AbilityCursor;
    const UC window = B(::bof3::Img(0x8031F3));
    WindowRecords[21].in_use = 0;
    B(::bof3::Img(0x8033A8)) = cursor != 0 ? 3 : 1;
    const UC member = Window16_Owner;
    if (window == 0) {
        const U wx = W(::bof3::Img(0x8031F4));
        WindowRecords[19].in_use = 1;
        B(::bof3::Img(0x803417)) = 1;
        SetW(::bof3::Img(0x803410), wx);
        WindowRecords[19].y = 54u * member + W(::bof3::Img(0x8031F6));
    } else {
        WindowRecords[19].in_use = 0;
    }
    SH_CALL(TextRecord_Set)(0, 8, Record(PartyRecord(member)));
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0xF000;
    SetW(::bof3::Img(0x8032FC), 0x2F);
    const U repeat = AutoRepeat(pressed);
    B(::bof3::Img(0x8033AC)) = 0xFF;
    B(::bof3::Img(0x8033AD)) = 0xFF;
    const UC n = PartyCount();
    const UC was = Window16_Owner;
    MoveAbilityMember(repeat, n);
    if (Rest2e_AbilityCursor == 2) {
        SetW(::bof3::Img(0x8033B0), 1);
    } else {
        SetW(::bof3::Img(0x8033B0), 0);
        UC cl;
        TurnType(repeat, cl);
    }
    if (Window16_Owner != S8(was)) Sound(0x101);
    const U p = Input_Pressed;
    const UC now = Window16_Owner;
    BattleActorCopies_ItemActor = now;
    if (Confirmed(p)) {
        Sound(0x104);
        const int m = BattleActorCopies_ItemActor;
        const U type = Window16_Page;
        const UC kept = B(::bof3::Img(kAbilityCursors.v) + type + 4 * m);
        const auto state = static_cast<UC>(FieldS_State + 1);
        B(::bof3::Img(0x8033AC)) = kept;
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
    const UC window = B(::bof3::Img(0x8031F3));
    const UC member = Window16_Owner;
    if (window == 0) {
        const U wx = W(::bof3::Img(0x8031F4));
        WindowRecords[19].in_use = 1;
        B(::bof3::Img(0x803417)) = 0;
        SetW(::bof3::Img(0x803410), wx);
        WindowRecords[19].y = 54u * member + W(::bof3::Img(0x8031F6));
    } else {
        WindowRecords[19].in_use = 0;
    }
    const UC type0 = Window16_Page;
    const unsigned char* const list = AbilityList(member, type0);
    const UC id = list[B(::bof3::Img(0x8033AC))];
    const U help = W(::bof3::Img(kAbilityHelp.v) + 24u * id);
    const UC busy = WindowRecords[16].state;
    SetW(::bof3::Img(0x8032FC), help);
    if (busy == 0) {
        const U cur = B(::bof3::Img(0x8033AC));
        const U x = L(::bof3::Img(0x8033A4)) + 7;
        WindowRecords[21].in_use = 1;
        SetW(::bof3::Img(0x803458), x);
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
        al = B(::bof3::Img(kAbilityCursors.v) + cl + 4 * m);
        B(::bof3::Img(0x8033AC)) = al;
    } else {
        al = B(::bof3::Img(0x8033AC));
        cl = Window16_Page;
    }
    const UC was = al;
    if (repeat & 0x1000) {
        if (al > 0) {
            --al;
            B(::bof3::Img(0x8033AC)) = al;
        }
    } else if (repeat & 0x4000) {
        if (al < 9) {
            ++al;
            B(::bof3::Img(0x8033AC)) = al;
        }
    }
    if (was != al) {
        Sound(0x100);
        al = B(::bof3::Img(0x8033AC));
        cl = Window16_Page;
    }
    {
        const int m = BattleActorCopies_ItemActor;
        B(::bof3::Img(kAbilityCursors.v) + cl + 4 * m) = al;
    }
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        const unsigned char* const chosen = AbilityList(Window16_Owner, cl);
        const UC ability = chosen[B(::bof3::Img(0x8033AC))];
        if (ability == 0) {
            Sound(0x107);
            return;
        }
        if (Rest2e_AbilityCursor != 0) {
            const UC mode = B(::bof3::Img(0x8033A8));
            const UC user = Window16_Owner;
            const UC can = SH_CALL(Skill_CanUse)(mode, user, ability);
            if (can == 0) {
                Sound(0x107);
                return;
            }
            const auto state = static_cast<UC>(FieldS_State + 2);
            const UC kept = B(::bof3::Img(0x8033AC));
            B(::bof3::Img(0x8033AD)) = kept;
            FieldS_Answer = 0;
            FieldS_State = state;
            return;
        }
        {
            const UC mode = B(::bof3::Img(0x8033A8));
            const UC user = Window16_Owner;
            const UC can = SH_CALL(Skill_CanUse)(mode, user, ability);
            if (can == 0) {
                Sound(0x107);
                return;
            }
        }
        if ((B(::bof3::Img(kAbilityFlags.v) + 24u * ability) & 0x10) != 0) {
            const unsigned char* const again = AbilityList(Window16_Owner, Window16_Page);
            const UC used = again[B(::bof3::Img(0x8033AC))];
            const UC user = PartyRecord(Window16_Owner);
            const UC answer = AbilityUse(user, 0, used);
            Rest2e_AbilityResult = answer;
            Sound(answer == 1 || answer == 5 ? 0x109 : 0x107);
            return;
        }
        Sound(0x103);
        UC k = 0;
        if (PartyCount() != 0) {
            do {
                const U o = 36u * k;
                B(::bof3::Img(0x80333E) + o) = k;
                const U y = 54u * k + 0x3E;
                ++k;
                B(::bof3::Img(0x803335) + o) = 6;
                B(::bof3::Img(0x803334) + o) = 1;
                B(::bof3::Img(0x803336) + o) = 0;
                B(::bof3::Img(0x803337) + o) = 0xA;
                B(::bof3::Img(0x80333F) + o) = 0;
                B(::bof3::Img(0x803340) + o) = 0;
                SetW(::bof3::Img(0x803338) + o, 0x140);
                SetW(::bof3::Img(0x80333A) + o, y);
                B(::bof3::Img(0x8031F3) + o) = 7;
            } while (k < PartyCount());
        }
        const UC kept = B(::bof3::Img(0x8033AC));
        B(::bof3::Img(0x8033AD)) = kept;
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
    B(::bof3::Img(0x8033AC)) = 0xFF;
    B(::bof3::Img(0x8033AD)) = 0xFF;
    FieldS_State = state;
}

// original 0x58E070: FieldAbility_States[5] - the target.
extern "C" void __cdecl FieldAbility_PickTarget(void) {
    Backdrop();
    const UC window = B(::bof3::Img(0x803337));
    WindowRecords[21].in_use = 0;
    if (window == 0) {
        const int t = FieldS_Column;
        const U wx = W(::bof3::Img(0x803338));
        WindowRecords[19].in_use = 1;
        B(::bof3::Img(0x803417)) = 1;
        SetW(::bof3::Img(0x803410), wx);
        WindowRecords[19].y = static_cast<U>(54 * t) + W(::bof3::Img(0x80333A));
    } else {
        WindowRecords[19].in_use = 0;
    }
    const U repeat = AutoRepeat(static_cast<std::uint32_t>(Input_Pressed) & 0xF000);
    const UC n = PartyCount();
    const UC was = static_cast<unsigned char>(FieldS_Column);
    const UC cl = MoveMemberS8(::bof3::Img(FieldS_Column_addr), repeat, n);
    if (was != cl) Sound(0x101);
    const unsigned char* const list = AbilityList(Window16_Owner, Window16_Page);
    const UC ability = list[B(::bof3::Img(0x8033AC))];
    SH_CALL(TextRecord_Set)(0, 0x10, At(::bof3::Img(kAbilityRecords.v) + 24u * ability));
    const U p = Input_Pressed;
    SetW(::bof3::Img(0x8032FC), 0x30);
    if (Confirmed(p)) {
        const unsigned char* const again = AbilityList(Window16_Owner, Window16_Page);
        const UC used = again[B(::bof3::Img(0x8033AC))];
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
    if (PartyCount() != 0) {
        do {
            const U o = 36u * k;
            ++k;
            B(::bof3::Img(0x803337) + o) = 9;
            B(::bof3::Img(0x8031F3) + o) = 8;
        } while (k < PartyCount());
    }
    const auto state = static_cast<UC>(FieldS_State - 1);
    WindowRecords[16].state = 3;
    B(::bof3::Img(0x8033AD)) = 0xFF;
    FieldS_State = state;
}

// original 0x58E2C0: FieldAbility_States[6] - the yes / no to add the ability
// to the shared list.
extern "C" void __cdecl FieldAbility_ShareConfirm(void) {
    Backdrop();
    const int answer = FieldS_Answer;
    const U x = L(::bof3::Img(0x8032F0)) - static_cast<U>(36 * answer) + 0xE8;
    WindowRecords[21].y = W(::bof3::Img(0x8032F2)) + 5;
    U pressed = L(::bof3::Img(kPressed.v));
    WindowRecords[21].in_use = 1;
    SetW(::bof3::Img(0x803458), x);
    SetW(::bof3::Img(0x8032FC), 0x31);
    if (pressed & 0xA000) {
        Sound(0x101);
        FieldS_Answer = static_cast<UC>(static_cast<unsigned char>(FieldS_Answer) ^ 1);
        pressed = L(::bof3::Img(kPressed.v));
    }
    if (Confirmed(pressed)) {
        if (static_cast<unsigned char>(FieldS_Answer) != 0) {
            Sound(0x104);
            const unsigned char* const list = AbilityList(Window16_Owner, Window16_Page);
            const UC ability = list[B(::bof3::Img(0x8033AC))];
            SH_CALL(AbilityList_Add)(ability, 0, 1, 0);
            unsigned char* const again = const_cast<unsigned char*>(AbilityList(Window16_Owner, Window16_Page));
            again[B(::bof3::Img(0x8033AC))] = 0;
        } else {
            Sound(0x106);
        }
        const auto state = static_cast<UC>(FieldS_State - 2);
        B(::bof3::Img(0x8033AD)) = 0xFF;
        FieldS_State = state;
        return;
    }
    if (!Cancelled(pressed)) return;
    const auto state = static_cast<UC>(FieldS_State - 2);
    B(::bof3::Img(0x8033AD)) = 0xFF;
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
    const U wx = W(::bof3::Img(0x8031F4));
    SetW(::bof3::Img(0x8032FC), 0x17);
    SetW(::bof3::Img(0x803410), wx);
    const U hand_y = WindowRecords[16].y;
    WindowRecords[19].in_use = 1;
    const U x = L(::bof3::Img(0x8033A4)) + 0x2A;
    B(::bof3::Img(0x803417)) = 1;
    WindowRecords[19].y = 54u * member + W(::bof3::Img(0x8031F6));
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0xF00C);
    WindowRecords[21].in_use = 1;
    SetW(::bof3::Img(0x803458), x);
    WindowRecords[21].y = hand_y + 7;
    const U repeat = AutoRepeat(pressed);
    B(::bof3::Img(0x8033AD)) = 0xFF;
    const UC n = PartyCount();
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
        B(::bof3::Img(0x8033E9)) = 6;
        B(::bof3::Img(0x8033EA)) = 0xF;
        WindowRecords[18].in_use = 1;
        SetW(::bof3::Img(0x8033EC), 0x28);
        SetW(::bof3::Img(0x8033EE), 0x3C);
        B(::bof3::Img(0x8033F2)) = static_cast<UC>(three + 5);
        B(::bof3::Img(0x8033F3)) = 0;
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
    const U row = B(::bof3::Img(0x8033F3));
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    const U x = L(::bof3::Img(0x8033EC)) + 5;
    const U y = (row << 4) + W(::bof3::Img(0x8033EE)) + 0x1A;
    SetW(::bof3::Img(0x8032FC), 0x18);
    B(::bof3::Img(0x8033F4)) = 0xFF;
    WindowRecords[21].in_use = 1;
    SetW(::bof3::Img(0x803458), x);
    SetW(::bof3::Img(0x80345A), y);
    B(::bof3::Img(0x8033AC)) = 0xFF;
    const U repeat = AutoRepeat(pressed);
    const U which = Window16_Page == 3 ? 1 : 0;
    const U rows = AddrOf(FieldAbility_ArrangeRows) + 6 * which;
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto c = static_cast<UC>(B(::bof3::Img(0x8033F3)) - 1);
        B(::bof3::Img(0x8033F3)) = c;
        if (S8(c) < 0) B(::bof3::Img(0x8033F3)) = static_cast<UC>(B(rows) - 1);
    } else if (repeat & 0x4000) {
        Sound(0x100);
        const auto c = static_cast<UC>(B(::bof3::Img(0x8033F3)) + 1);
        B(::bof3::Img(0x8033F3)) = c;
        if (static_cast<int>(c) > static_cast<int>(B(rows)) - 1) B(::bof3::Img(0x8033F3)) = 0;
    }
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        Sound(0x103);
        const UC how = B(rows + 1 + B(::bof3::Img(0x8033F3)));
        if (how != 0) {
            SH_AT(void (__cdecl*)(unsigned), at::kAbilitySort.v)(how);
            return;
        }
        const int m = BattleActorCopies_ItemActor;
        B(::bof3::Img(0x8033AD)) = 0xFF;
        const U type = Window16_Page;
        const UC kept = B(::bof3::Img(kAbilityCursors.v) + type + 4 * m);
        const auto step = static_cast<UC>(FieldS_Step + 1);
        B(::bof3::Img(0x8033AC)) = kept;
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
    const U cur = B(::bof3::Img(0x8033AC));
    const U x = L(::bof3::Img(0x8033A4)) + 7;
    SetW(::bof3::Img(0x803458), x);
    const UC type = Window16_Page;
    B(::bof3::Img(0x8033F4)) = 0;
    const UC member = Window16_Owner;
    WindowRecords[21].in_use = 1;
    WindowRecords[21].y = 13u * (cur + 2) + static_cast<std::uint32_t>(WindowRecords[16].y);
    const unsigned char* const list = AbilityList(member, type);
    const UC ability = list[B(::bof3::Img(0x8033AC))];
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0x500C);
    SetW(::bof3::Img(0x8032FC), W(::bof3::Img(kAbilityHelp.v) + 24u * ability));
    const U repeat = AutoRepeat(pressed);
    UC cl = B(::bof3::Img(0x8033AC));
    const UC was = cl;
    if (repeat & 0x1000) {
        if (cl > 0) {
            --cl;
            B(::bof3::Img(0x8033AC)) = cl;
        }
    } else if (repeat & 0x4000) {
        if (cl < 9) {
            ++cl;
            B(::bof3::Img(0x8033AC)) = cl;
        }
    }
    {
        const int m = BattleActorCopies_ItemActor;
        const U t = Window16_Page;
        B(::bof3::Img(kAbilityCursors.v) + t + 4 * m) = cl;
    }
    if (was != cl) Sound(0x100);
    const U p = Input_Pressed;
    if (Confirmed(p)) {
        if (B(::bof3::Img(0x8033AD)) == 0xFF) {
            const unsigned char* const again = AbilityList(Window16_Owner, Window16_Page);
            if (again[B(::bof3::Img(0x8033AC))] != 0) {
                Sound(0x103);
                B(::bof3::Img(0x8033AD)) = B(::bof3::Img(0x8033AC));
                return;
            }
            Sound(0x107);
            return;
        }
        Sound(0x103);
        const UC type1 = Window16_Page;
        const UC member1 = Window16_Owner;
        const unsigned char* const at_cursor = AbilityList(member1, type1);
        unsigned char* const first = const_cast<unsigned char*>(at_cursor) + B(::bof3::Img(0x8033AC));
        const UC member2 = Window16_Owner;
        const UC type2 = Window16_Page;
        const unsigned char* const at_pick = AbilityList(member2, type2);
        unsigned char* const second = const_cast<unsigned char*>(at_pick) + B(::bof3::Img(0x8033AD));
        Swap(second, first);
        B(::bof3::Img(0x8033AD)) = 0xFF;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    if (B(::bof3::Img(0x8033AD)) == 0xFF) {
        FieldS_Step = static_cast<UC>(FieldS_Step - 1);
        return;
    }
    B(::bof3::Img(0x8033AD)) = 0xFF;
}

// original 0x58E9D0: FieldAbility_States[8] - the screen closed.
extern "C" void __cdecl FieldAbility_Close(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(::bof3::Img(kTimer.v)) - 1);
    B(::bof3::Img(kTimer.v)) = t;
    if (t != 0) return;
    SH_AT(Handler, at::kAbilityCloseWindows.v)();
    U k = 0;
    if (PartyCount() != 0) {
        do {
            B(::bof3::Img(0x8031F3) + 0x24u * k) = 4;
            ++k;
        } while (static_cast<int>(k) < static_cast<int>(PartyCount()));
    }
    const UC m = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    B(::bof3::Img(0x80325F)) = 2;
    B(::bof3::Img(0x803283)) = 2;
    B(::bof3::Img(0x8032A7)) = 2;
    B(::bof3::Img(0x8032CB)) = 2;
    B(::bof3::Img(0x905BA1)) = m;
    B(::bof3::Img(kTimer.v)) = 0;
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
    const auto t = static_cast<UC>(B(::bof3::Img(kTimer.v)) - 1);
    B(::bof3::Img(kTimer.v)) = t;
    if (t != 0) return;
    SH_AT(Handler, at::kAbilityViewWindow.v)();
    const UC cursor = Rest2e_AbilityViewCursor;
    const UC top = Rest2e_AbilityViewTop;
    B(::bof3::Img(0x8033CF)) = cursor;
    B(::bof3::Img(0x8033CE)) = top;
    Sound(0x102);
    const auto step = static_cast<UC>(FieldS_Step + 1);
    B(::bof3::Img(kTimer.v)) = 7;
    FieldS_Step = step;
}

// original 0x58EAD0: FieldAbility_ViewSteps[1].
extern "C" void __cdecl FieldAbility_ViewWait(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(::bof3::Img(kTimer.v)) - 1);
    B(::bof3::Img(kTimer.v)) = t;
    if (t == 0) FieldS_Step = static_cast<UC>(FieldS_Step + 1);
}

// original 0x58EB00: FieldAbility_ViewSteps[2] - the list scrolled.
extern "C" void __cdecl FieldAbility_ViewBrowse(void) {
    Backdrop();
    const UC h = B(::bof3::Img(0x8033D0));
    SetW(::bof3::Img(0x8032FC), h != 0 ? h + 0x4183u : 0);
    const U top = B(::bof3::Img(0x8033CE));
    const U cur = B(::bof3::Img(0x8033CF));
    const U x = L(::bof3::Img(0x8033C8)) + 7;
    SetW(::bof3::Img(0x803458), x);
    const U pressed = static_cast<std::uint32_t>(Input_Pressed) & Sigma(0x500C);
    WindowRecords[21].in_use = 1;
    WindowRecords[21].y = 13u * (cur - top + 2) + W(::bof3::Img(0x8033CA));
    const U repeat = AutoRepeat(pressed);
    const UC was = B(::bof3::Img(0x8033CF));
    UC cl, al;
    Scroll(repeat, kViewScroll, cl, al);
    Rest2e_AbilityViewCursor = cl;
    Rest2e_AbilityViewTop = al;
    if (was != cl) Sound(0x100);
    if (!Cancelled(static_cast<std::uint32_t>(Input_Pressed))) return;
    Sound(0x106);
    const auto step = static_cast<UC>(FieldS_Step + 1);
    WindowRecords[21].in_use = 0;
    B(::bof3::Img(0x8033C7)) = 1;
    B(::bof3::Img(kTimer.v)) = 7;
    FieldS_Step = step;
}

// original 0x58ECC0: FieldAbility_ViewSteps[3].
extern "C" void __cdecl FieldAbility_ViewLeave(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(::bof3::Img(kTimer.v)) - 1);
    B(::bof3::Img(kTimer.v)) = t;
    if (t != 0) return;
    WindowRecords[16].state = 2;
    Sound(0x102);
    const auto step = static_cast<UC>(FieldS_Step + 1);
    B(::bof3::Img(kTimer.v)) = 7;
    FieldS_Step = step;
}

// original 0x58ED10: FieldAbility_ViewSteps[4].
extern "C" void __cdecl FieldAbility_ViewEnd(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(::bof3::Img(kTimer.v)) - 1);
    B(::bof3::Img(kTimer.v)) = t;
    if (t != 0) return;
    const UC state = FieldS_State;
    FieldS_Step = 0;
    FieldS_State = static_cast<UC>(state - 7);
}

void Rest2E_Inject() {
    if (bof3::WantsShadow("rest_2e")) rest_2e::SelfTest();
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
}
