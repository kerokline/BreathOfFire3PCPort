// Group R2E of round fourteen (wave two): three of the field menu's screens,
// 49 functions at 0x58B1D0..0x58ED3F - the cut's 48 rows (analysis/round14_cut.tsv)
// and the start in their span no list had (0x58CFC0), each read to its last
// instruction with capstone (2026-10-04) and fuzzed through the scenario
// harness's field mode (rest_2e_fuzz.cpp). docs/rest_2e.md has them one row
// each.
//
//   - The Items screen (FieldMenu_States[2], whose dispatcher 0x58AAE0 and
//     state table 0x667328 are R2D's): the three arrange steps R2D's 0x58B1C0
//     jumps to through FieldItems_ArrangeSteps, the discard prompt (state 6),
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
// byte (help words, the name tables, MoveScript_EffectState) ours reads the
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

constexpr U kMode = 0x929F00;          // the menu block: the screen (FieldMenu_States' index)
constexpr U kState = 0x929F01;         // its state
constexpr U kStep = 0x929F02;          // its step
constexpr U kTimer = 0x929F04;         // the open / close countdown
constexpr U kMember = 0x929F06;        // s8: a party member (Items, Equipment, Ability)
constexpr U kTarget = 0x929F08;        // s8: the ability's target member
constexpr U kAnswer = 0x929F0B;        // s8: a yes / no answer (1 yes)
constexpr U kBackdrop = 0x903A5B;      // the Config backdrop kind (Menu_DrawBackdrop's)
constexpr U kPressed = 0x7E1BEC;       // Input_Pressed
constexpr U kConfirm = 0x90358E;       // Field_ConfirmButtons
constexpr U kCancel = 0x903590;        // Field_CancelButtons
constexpr U kParty = 0x904062;         // the party's member ids
constexpr U kRecords = 0x903A70;       // CharacterRecords, 0xA4 each
constexpr U kRecordStride = 0xA4;

// The image's tables, read in place.
constexpr U kIdLists = 0x656B00;       // Inventory_IdLists, five pointers by category
constexpr U kCountLists = 0x656B14;    // Inventory_CountLists, five (the fifth 0)
constexpr U kConsumables = 0x656B28;   // NameTable_Consumables, 22 bytes a record
constexpr U kConsumableFlags = 0x656B38;  // ... +0x10, the flags byte
constexpr U kWeaponA = 0x657464;       // NameTable_Weapons + 28 id + 0x14
constexpr U kWeaponB = 0x657466;       // ... + 0x16
constexpr U kArmourType = 0x657D7A;    // NameTable_Armour + 26 id + 0x12
constexpr U kArmourA = 0x657D7B;       // ... + 0x13
constexpr U kArmourB = 0x657D7C;       // ... + 0x14
constexpr U kAbilityHelp = 0x65C4DE;   // NameTable_Abilities + 24 id + 6: the help word
constexpr U kAbilityFlags = 0x65C4D8;  // NameTable_Abilities + 24 id: the first parameter byte
constexpr U kAbilityRecords = 0x65C4C8;  // Ability_Records, 24 bytes an id (the name first)

// The per-member bytes the screens keep (FS's at::kEquipBytes block 0x939880..).
constexpr U kEquipTops = 0x939880;     // the equip item list's top by slot + 6 member
constexpr U kItemCursors = 0x939898;   // the Items list's cursor by category
constexpr U kItemTops = 0x9398B8;      // and its top
constexpr U kAbilityCursors = 0x9398C0;  // the ability list's cursor by type + 4 member
constexpr U kViewCursor = 0x939892;    // the ability view's cursor
constexpr U kViewTop = 0x9398CC;       // and its top
constexpr U kPreview = 0x6BDFA8;       // the six previewed equipment bytes
constexpr U kEquipCursor = 0x6BDFAF;   // FieldEquip_TopMenu's cursor
constexpr U kAbilityResult = 0x6BDFB6; // the last ability's answer
constexpr U kAbilityCursor = 0x6BDFB7; // FieldAbility_TopMenu's cursor

// --- the helpers ------------------------------------------------------------------------

void Backdrop() { SH_CALL(Menu_DrawBackdrop)(B(kBackdrop)); }
void Sound(U id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
U AutoRepeat(U pressed) { return SH_CALL(Input_AutoRepeat)(pressed); }
UC PartyCount() { return static_cast<UC>(SH_CALL(Party_Count)(0)); }
bool Confirmed(U pressed) { return (W(kConfirm) & pressed & 0xFFFF) != 0; }
bool Cancelled(U pressed) { return (W(kCancel) & pressed & 0xFFFF) != 0; }
// A party member's record index: MoveScript_EffectState[the member's id].
UC RecordOf(U id) { return MoveScript_EffectState[id & 0xFF]; }
UC PartyRecord(int member) { return RecordOf(B(static_cast<U>(static_cast<int>(kParty) + member))); }
unsigned char* Record(U index) { return At(kRecords + (index & 0xFF) * kRecordStride); }
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
    return At(L(kIdLists + 4 * category));
}
unsigned char* CountList(U category, const char* who) {
    if (category > 4) bof3::Fatal("%s: category %u reads past Inventory_CountLists (the original reads on)", who, (unsigned)category);
    const U list = L(kCountLists + 4 * category);
    if (list == 0) bof3::Fatal("%s: category %u's count list is a null pointer (the original faults)", who, (unsigned)category);
    return At(list);
}

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
    return static_cast<UC>(SH_AT(UC (__cdecl*)(unsigned, unsigned, unsigned, unsigned), at::kAbilityUse)(user, target, ability, 0));
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
    } else if (repeat & 4) {
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
        if (repeat & 8) {
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
constexpr ListScroll kItemScroll = {0x803340, 0x80333F, 0x803346, 0x7F, 0x77, false};
constexpr ListScroll kShortScroll = {0x803340, 0x80333F, 0x803346, 0x1F, 0x17, false};
constexpr ListScroll kViewScroll = {0x8033CF, 0x8033CE, 0x8033CC, 0x11, 9, true};

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
        cl = static_cast<UC>(B(0x8033AB) - 1);
        B(0x8033A9) = 0x32;
        B(0x8033AB) = cl;
        if (S8(cl) < 0) {
            cl = 3;
            B(0x8033AB) = cl;
        }
        return true;
    }
    if (repeat & 0x2000) {
        Sound(0x101);
        cl = static_cast<UC>(B(0x8033AB) + 1);
        B(0x8033A9) = 0x31;
        B(0x8033AB) = cl;
        if (cl > 3) {
            cl = 0;
            B(0x8033AB) = cl;
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
        const auto m = static_cast<UC>(B(0x8033AA) - 1);
        B(0x8033AA) = m;
        if (S8(m) < 0) B(0x8033AA) = static_cast<UC>(count - 1);
    } else if (repeat & 0x4000) {
        Sound(0x100);
        const auto m = static_cast<UC>(B(0x8033AA) + 1);
        B(0x8033AA) = m;
        if (static_cast<int>(m) > static_cast<int>(count) - 1) B(0x8033AA) = 0;
    }
}

}  // namespace

// ===========================================================================
// The Items screen
// ===========================================================================

// original 0x58B1D0: FieldItems_ArrangeSteps[0] - the category.
extern "C" void __cdecl FieldItems_ArrangeCategory(void) {
    Backdrop();
    const U pressed = W(kPressed) & 0xF00C;
    const U x = L(0x803338) + 0x2A;
    const U y = W(0x80333A) + 7;
    SetW(0x8032FC, 0x17);
    B(0x803454) = 1;
    SetW(0x803458, x);
    SetW(0x80345A, y);
    const U repeat = AutoRepeat(pressed);
    B(0x803340) = 0xFF;
    bool turned = false;
    U category = 0;
    if (repeat & 0x8000) {
        Sound(0x101);
        const auto c = static_cast<UC>(B(0x80333E) - 1);
        SetW(0x803344, 0x32);
        B(0x80333E) = c;
        if (S8(c) < 0) B(0x80333E) = 3;
        category = B(0x80333E);
        turned = true;
    } else if (repeat & 0x2000) {
        Sound(0x101);
        auto c = static_cast<UC>(B(0x80333E) + 1);
        SetW(0x803344, 0x31);
        B(0x80333E) = c;
        if (c > 3) {
            c = 0;
            B(0x80333E) = c;
        }
        category = c;
        turned = true;
    }
    if (turned) B(0x80333F) = B(kItemTops + category);
    const U p = W(kPressed);
    if (Confirmed(p)) {
        Sound(0x103);
        B(0x803362) = B(0x80333E);
        const auto step = static_cast<UC>(B(kStep) + 1);
        B(0x803359) = 6;
        B(0x80335A) = 0xF;
        B(0x803358) = 1;
        SetW(0x80335C, 0xB4);
        SetW(0x80335E, 0x3C);
        B(0x803363) = 0;
        B(0x803364) = 0xFF;
        B(kStep) = step;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    B(kState) = static_cast<UC>(B(kState) - 3);
}

// original 0x58B330: FieldItems_ArrangeSteps[1] - the sort, a row of the
// category's FieldItems_ArrangeRows.
extern "C" void __cdecl FieldItems_ArrangeHow(void) {
    Backdrop();
    const U row = B(0x803363);
    const U pressed = W(kPressed) & 0x5000;
    const U x = L(0x80335C) + 5;
    const U y = (row << 4) + W(0x80335E) + 0x1A;
    SetW(0x8032FC, 0x18);
    B(0x803454) = 1;
    SetW(0x803458, x);
    SetW(0x80345A, y);
    B(0x803340) = 0xFF;
    B(0x803364) = 0xFF;
    const U repeat = AutoRepeat(pressed);
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto c = static_cast<UC>(B(0x803363) - 1);
        B(0x803363) = c;
        if (S8(c) < 0) B(0x803363) = static_cast<UC>(B(AddrOf(FieldItems_ArrangeRows) + 6u * B(0x803362)) - 1);
    } else if (repeat & 0x4000) {
        Sound(0x100);
        const auto c = static_cast<UC>(B(0x803363) + 1);
        const int last = static_cast<int>(B(AddrOf(FieldItems_ArrangeRows) + 6u * B(0x803362))) - 1;
        B(0x803363) = c;
        if (static_cast<int>(c) > last) B(0x803363) = 0;
    }
    const U p = W(kPressed);
    if (Confirmed(p)) {
        Sound(0x103);
        const UC how = B(AddrOf(FieldItems_ArrangeRows) + 1 + 6u * B(0x803362) + B(0x803363));
        if (how != 0) {
            SH_CALL(FieldItems_Sort)(how);
            return;
        }
        B(0x803341) = 0xFF;
        const UC cursor = B(kItemCursors + B(0x80333E));
        const auto step = static_cast<UC>(B(kStep) + 1);
        B(0x803340) = cursor;
        B(kStep) = step;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto step = static_cast<UC>(B(kStep) - 1);
    B(0x803358) = 0;
    B(kStep) = step;
}

// original 0x58B4B0: FieldItems_ArrangeSteps[2] - an entry picked and swapped
// with another, the list scrolled.
extern "C" void __cdecl FieldItems_ArrangeMove(void) {
    Backdrop();
    const U category = B(0x80333E);
    const UC item = IdList(category, "FieldItems_ArrangeMove")[B(0x803340)];
    const U help = SH_CALL(Item_HelpMessage)(category, item);
    const U top = B(0x80333F);
    SetW(0x8032FC, help);
    const U x = L(0x803338) + 7;
    B(0x803364) = 0;
    SetW(0x803458, x);
    B(0x803454) = 1;
    const U pressed = W(kPressed) & 0x500C;
    SetW(0x80345A, 13u * (B(0x803340) - top + 2) + W(0x80333A));
    const U repeat = AutoRepeat(pressed);
    const UC was = B(0x803340);
    UC cl, al;
    Scroll(repeat, kItemScroll, cl, al);
    const U now = B(0x80333E);
    B(kItemCursors + now) = cl;
    B(kItemTops + now) = al;
    if (was != cl) Sound(0x100);
    if (W(0x803346) != 0) return;
    const U p = W(kPressed);
    if (Confirmed(p)) {
        if (B(0x803341) == 0xFF) {
            Sound(0x103);
            if (IdList(B(0x80333E), "FieldItems_ArrangeMove")[B(0x803340)] != 0) {
                B(0x803341) = B(0x803340);
                return;
            }
            Sound(0x107);
            return;
        }
        Sound(0x103);
        {
            unsigned char* const ids = IdList(B(0x80333E), "FieldItems_ArrangeMove");
            const U cursor = B(0x803340);
            Swap(ids + B(0x803341), ids + cursor);
        }
        {
            const U cursor = B(0x803340);
            unsigned char* const counts = CountList(B(0x80333E), "FieldItems_ArrangeMove");
            Swap(counts + B(0x803341), counts + cursor);
        }
        B(0x803341) = 0xFF;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    if (B(0x803341) != 0xFF) {
        B(0x803341) = 0xFF;
        return;
    }
    B(kStep) = static_cast<UC>(B(kStep) - 1);
}

// original 0x58B750: the Items screen's state 6 - the yes / no to discard.
extern "C" void __cdecl FieldItems_DiscardConfirm(void) {
    Backdrop();
    const U category = B(0x80333E);
    const UC item = IdList(category, "FieldItems_DiscardConfirm")[B(0x803340)];
    const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
    SH_CALL(TextRecord_Set)(0, 0x10, name);
    const int answer = S8(B(kAnswer));
    const U x = L(0x8032F0) - static_cast<U>(36 * answer) + 0xE8;
    const U y = W(0x8032F2) + 5;
    U pressed = L(kPressed);
    SetW(0x8032FC, 0x1A);
    B(0x803454) = 1;
    SetW(0x803458, x);
    SetW(0x80345A, y);
    if (pressed & 0xA000) {
        Sound(0x101);
        B(kAnswer) = static_cast<UC>(B(kAnswer) ^ 1);
        pressed = L(kPressed);
    }
    if (Confirmed(pressed)) {
        if (B(kAnswer) != 0) {
            Sound(0x104);
            IdList(B(0x80333E), "FieldItems_DiscardConfirm")[B(0x803340)] = 0;
            CountList(B(0x80333E), "FieldItems_DiscardConfirm")[B(0x803340)] = 0;
            const auto state = static_cast<UC>(B(kState) - 3);
            B(0x803341) = 0xFF;
            B(kState) = state;
            return;
        }
    } else if (!Cancelled(pressed)) {
        return;
    }
    Sound(0x106);
    const auto state = static_cast<UC>(B(kState) - 3);
    B(0x803341) = 0xFF;
    B(kState) = state;
}

// original 0x58B8A0: the Items screen's state 7 - a consumable used on a member.
extern "C" void __cdecl FieldItems_UseOnMember(void) {
    Backdrop();
    const U category = B(0x80333E);
    const U index = B(0x803340);
    unsigned char* const id = IdList(category, "FieldItems_UseOnMember") + index;
    unsigned char* const count = CountList(category, "FieldItems_UseOnMember") + index;
    SH_CALL(TextRecord_Set)(0, 0x10, At(kConsumables + 22u * *id));
    const int member = S8(B(kMember));
    SetW(0x8032FC, 0x19);
    B(0x80340C) = 1;
    SetW(0x803410, 0xB6);
    SetW(0x803412, static_cast<U>(54 * member + 0x3E));
    const UC n = PartyCount();
    const U pressed = W(kPressed) & 0x5000;
    const U repeat = AutoRepeat(pressed);
    const UC was = B(kMember);
    UC cl = MoveMemberS8(kMember, repeat, n);
    if (was != cl) {
        Sound(0x101);
        cl = B(kMember);
    }
    const U p = W(kPressed);
    if (Confirmed(p)) {
        const UC item = *id;
        const UC record = PartyRecord(S8(cl));
        const UC answer = SH_CALL(ItemUse_Dispatch)(record, item, 0);
        if (answer != 0 && answer != 4) {
            Sound(0x107);
            return;
        }
        Sound(0x104);
        const auto left = static_cast<UC>(*count - 1);
        *count = left;
        if (left != 0) return;
        *id = 0;
    } else if (Cancelled(p)) {
        Sound(0x106);
    } else {
        return;
    }
    const auto state = static_cast<UC>(B(kState) - 4);
    B(0x80340C) = 0;
    B(0x803341) = 0xFF;
    B(kState) = state;
}

// original 0x58BA40: the Items screen's state 9 - a 32-entry list scrolled.
extern "C" void __cdecl FieldItems_ViewList32(void) {
    Backdrop();
    const U category = B(0x80333E);
    const UC item = IdList(category, "FieldItems_ViewList32")[B(0x803340)];
    const U help = SH_CALL(Item_HelpMessage)(category, item);
    const U top = B(0x80333F);
    SetW(0x8032FC, help);
    const U x = L(0x803338) + 7;
    B(0x803454) = 1;
    SetW(0x803458, x);
    const U pressed = W(kPressed) & 0x500C;
    SetW(0x80345A, 13u * (B(0x803340) - top + 2) + W(0x80333A));
    const U repeat = AutoRepeat(pressed);
    const UC was = B(0x803340);
    UC cl, al;
    Scroll(repeat, kShortScroll, cl, al);
    const U now = B(0x80333E);
    B(kItemCursors + now) = cl;
    B(kItemTops + now) = al;
    if (was != cl) Sound(0x100);
    if (W(0x803346) != 0) return;
    if (!Cancelled(W(kPressed))) return;
    Sound(0x106);
    const auto state = static_cast<UC>(B(kState) - 7);
    const UC t = B(kItemTops);
    B(kState) = state;
    B(0x80333E) = 0;
    B(0x80333F) = t;
    B(0x803340) = 0xFF;
    B(0x803341) = 0xFF;
}

// original 0x58BC30: the Items screen's windows.
extern "C" void __cdecl FieldItems_InitWindows(void) {
    B(0x8032EF) = 2;
    B(0x803313) = 2;
    B(0x803337) = 2;
    B(0x80333F) = B(kItemTops);
    B(0x803340) = 0xFF;
    B(0x803341) = 0xFF;
    B(0x8032ED) = 6;
    B(0x8032EE) = 9;
    B(0x8032EC) = 1;
    SetW(0x8032F0, 0x14);
    SetW(0x8032F2, 0xFFEC);
    SetW(0x8032FC, 0);
    B(0x803311) = 6;
    B(0x803312) = 0xD;
    B(0x803310) = 1;
    SetW(0x803314, 0x40);
    SetW(0x803316, 0xFFEC);
    B(0x80331A) = 0;
    B(0x80331B) = 0xFF;
    B(0x803335) = 6;
    B(0x803336) = 0xC;
    B(0x803334) = 1;
    SetW(0x803338, 0xFF38);
    SetW(0x80333A, 0x3E);
    B(0x80333E) = 0;
    B(0x80333C) = 0;
    B(0x80333D) = 0;
    SetW(0x803344, 0);
    SetW(0x803346, 0);
    B(0x80340D) = 6;
    B(0x80340E) = 0xA;
    B(0x80340C) = 0;
    B(0x803416) = 1;
    B(0x803417) = 1;
    B(0x803455) = 6;
    B(0x803456) = 0xE;
    B(0x803457) = 0;
    B(0x803454) = 0;
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
        const U category = B(0x80333E);
        unsigned char* const ids = IdList(category, "FieldItemSort_Compact");
        const U counts = L(kCountLists + 4 * category);
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
constexpr U kConsumableIds = 0x904154;   // Inventory_IdLists[0]; the counts 0x904354
constexpr U kWeaponIds = 0x9041D4;       // [1]
constexpr U kArmourIds = 0x904254;       // [2]

}  // namespace

// original 0x58BE00: FieldItems_Sorts[1] - consumables with flag bit 0 first.
extern "C" void __cdecl FieldItemSort_ConsumableFlag1(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(kConsumableIds, [](UC x, UC y) {
        return (B(kConsumableFlags + 22u * x) & 1) == 0 && (B(kConsumableFlags + 22u * y) & 1) != 0;
    });
}

// original 0x58BEB0: FieldItems_Sorts[2] - consumables with flag bit 1 first.
extern "C" void __cdecl FieldItemSort_ConsumableFlag2(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(kConsumableIds, [](UC x, UC y) {
        return (B(kConsumableFlags + 22u * x) & 2) == 0 && (B(kConsumableFlags + 22u * y) & 2) != 0;
    });
}

// original 0x58BF60: FieldItems_Sorts[3] - weapons by +0x16, the highest first.
extern "C" void __cdecl FieldItemSort_WeaponsByPower(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(kWeaponIds, [](UC x, UC y) { return B(kWeaponB + 28u * x) < B(kWeaponB + 28u * y); });
}

// original 0x58C010: FieldItems_Sorts[4] - armour by +0x14, the highest first.
extern "C" void __cdecl FieldItemSort_ArmourByPower(void) {
    SH_CALL(FieldItemSort_Compact)();
    BubbleFixed(kArmourIds, [](UC x, UC y) { return B(kArmourB + 26u * x) < B(kArmourB + 26u * y); });
}

// original 0x58C0B0: FieldItems_Sorts[5] - the category's list by Item_IconKind.
extern "C" void __cdecl FieldItemSort_ByIconKind(void) {
    SH_CALL(FieldItemSort_Compact)();
    const UC category = B(0x80333E);
    for (U bound = 0x7F; bound != 0; --bound) {
        unsigned char* const ids = IdList(category, "FieldItemSort_ByIconKind");
        unsigned char* const counts = CountList(category, "FieldItemSort_ByIconKind");
        for (U i = 0; i < bound; ++i) {
            if (ids[i] == 0) continue;
            const UC y = ids[i + 1];
            if (y == 0) continue;
            const auto second = static_cast<UC>(SH_CALL(Item_IconKind)(category, y));
            const UC x = ids[i];
            const auto first = static_cast<UC>(SH_CALL(Item_IconKind)(category, x));
            if (first <= second) continue;
            Swap(ids + i, ids + i + 1);
            Swap(counts + i, counts + i + 1);
        }
    }
}

// original 0x58C1A0: FieldItems_Sorts[6] - the entries member s8 0x929F06 can
// equip first.
extern "C" void __cdecl FieldItemSort_EquipableFirst(void) {
    SH_CALL(FieldItemSort_Compact)();
    const UC record = PartyRecord(S8(B(kMember)));
    const UC category = B(0x80333E);
    const UC bit = Bit8(record);
    for (U bound = 0x7F; bound != 0; --bound) {
        unsigned char* const ids = IdList(category, "FieldItemSort_EquipableFirst");
        unsigned char* const counts = CountList(category, "FieldItemSort_EquipableFirst");
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
            Swap(counts + i, counts + i + 1);
        }
    }
}

// original 0x58C2A0: three of the Items screen's windows closed.
extern "C" void __cdecl FieldItems_CloseWindows(void) {
    B(0x803310) = 0;
    B(0x803334) = 0;
    B(0x803358) = 0;
}

// ===========================================================================
// The Equipment screen
// ===========================================================================

// original 0x58C2C0: FieldMenu_States[4], a tail jump through FieldEquip_States
// by the state byte, unchecked.
extern "C" void __cdecl FieldEquip_Run(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(Entry(AddrOf(FieldEquip_States), 9, B(kState), "FieldEquip_Run"))();
}

// original 0x58C2D0: FieldEquip_States[0].
extern "C" void __cdecl FieldEquip_Open(void) {
    Backdrop();
    SH_CALL(FieldEquip_InitWindows)();
    Sound(0x102);
    const auto state = static_cast<UC>(B(kState) + 1);
    B(kEquipCursor) = 0;
    B(kTimer) = 5;
    B(kState) = state;
}

// original 0x58C310: FieldEquip_States[1] and [4], FieldAbility_States[1].
extern "C" void __cdecl FieldMenu_CountdownState(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(kTimer) - 1);
    B(kTimer) = t;
    if (t == 0) B(kState) = static_cast<UC>(B(kState) + 1);
}

// original 0x58C340: FieldEquip_States[2] - the four-entry menu.
extern "C" void __cdecl FieldEquip_TopMenu(void) {
    Backdrop();
    const int c = S8(B(kEquipCursor));
    const U help = W(static_cast<U>(static_cast<int>(AddrOf(FieldEquip_TopHelp)) + 2 * c));
    const U x = L(0x803314) + static_cast<U>(48 * c);
    SetW(0x8032FC, help);
    const U pressed = W(kPressed) & 0xA000;
    B(0x803454) = 1;
    const U y = W(0x803316) + 4;
    SetW(0x803458, x);
    SetW(0x80345A, y);
    B(0x80331B) = 0xFF;
    B(0x803341) = 1;
    const U repeat = AutoRepeat(pressed);
    const UC was = B(kEquipCursor);
    const UC cl = TurnFour(kEquipCursor, repeat);
    if (was != cl) Sound(0x101);
    const U p = W(kPressed);
    if (Confirmed(p)) {
        Sound(0x104);
        B(0x80331B) = B(kEquipCursor);
        const auto state = static_cast<UC>(B(kState) + 1);
        B(0x803454) = 0;
        B(kState) = state;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x102);
    Sound(0x106);
    const auto state = static_cast<UC>(B(kState) + 5);
    B(0x8032EF) = 1;
    B(0x803313) = 1;
    B(0x803337) = 1;
    B(0x80340C) = 0;
    B(0x803454) = 0;
    B(kState) = state;
    B(kTimer) = 0;
}

// original 0x58C4A0: FieldEquip_States[3] - the member.
extern "C" void __cdecl FieldEquip_PickMember(void) {
    Backdrop();
    const U m0 = B(0x803340);
    const U wx = W(0x8031F4);
    B(0x80340C) = 1;
    SetW(0x803410, wx);
    const U pressed = W(kPressed) & 0x5000;
    SetW(0x803412, 54 * m0 + 0x3E);
    const U repeat = AutoRepeat(pressed);
    const UC was = B(0x803340);
    const UC n = PartyCount();
    if (repeat & 0x1000) {
        const auto m = static_cast<UC>(B(0x803340) - 1);
        B(0x803340) = m;
        if (S8(m) < 0) B(0x803340) = static_cast<UC>(n - 1);
    } else if (repeat & 0x4000) {
        const auto m = static_cast<UC>(B(0x803340) + 1);
        B(0x803340) = m;
        if (static_cast<int>(B(0x803340)) > static_cast<int>(n) - 1) B(0x803340) = 0;
    }
    if (S8(was) != static_cast<int>(B(0x803340))) Sound(0x101);
    if (B(kEquipCursor) == 1) SH_CALL(FieldEquip_BestByPower)(B(0x803340));
    if (B(kEquipCursor) == 2) SH_CALL(FieldEquip_BestByOrder)(B(0x803340));
    SH_CALL(TextRecord_Set)(0, 8, Record(PartyRecord(B(0x803340))));
    const int c = S8(B(kEquipCursor));
    const U help = W(static_cast<U>(static_cast<int>(AddrOf(FieldEquip_MemberHelp)) + 2 * c));
    const UC flag = B(static_cast<U>(static_cast<int>(AddrOf(FieldEquip_MemberFlags)) + 2 * c));
    const U p = W(kPressed);
    SetW(0x8032FC, help);
    B(0x803341) = flag;
    if (Confirmed(p)) {
        Sound(0x103);
        const UC cursor = B(kEquipCursor);
        if (cursor == 0) {
            Sound(0x102);
            UC k = 0;
            if (PartyCount() != 0) {
                do {
                    B(0x8031F3 + 0x24u * k) = 7;
                    ++k;
                } while (k < PartyCount());
            }
            const UC m = B(0x803340);
            B(0x80340C) = 0;
            B(0x803337) = 3;
            B(0x80333E) = 0;
            B(0x80333F) = 0xFF;
            B(0x803359) = 6;
            B(0x803362) = B(kEquipTops + 6u * m);
            const auto state = static_cast<UC>(B(kState) + 1);
            B(0x80335A) = 0x12;
            B(0x80335B) = 4;
            B(0x803358) = 1;
            SetW(0x80335C, 0x140);
            SetW(0x80335E, 0x3E);
            B(0x803364) = m;
            B(0x803363) = 0xFF;
            B(0x803360) = 0;
            B(0x803365) = 0;
            B(0x803361) = 0;
            SetW(0x803368, 0);
            B(kMember) = m;
            B(kTimer) = 5;
            B(kState) = state;
            return;
        }
        if (cursor == 3) {
            const auto state = static_cast<UC>(B(kState) + 5);
            B(0x803417) = 0;
            B(0x80333E) = 1;
            B(0x80333F) = 0xFF;
            B(kState) = state;
            return;
        }
        SH_CALL(FieldEquip_ApplyPreview)();
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto state = static_cast<UC>(B(kState) - 1);
    const UC m = B(kMember);
    B(0x80340C) = 0;
    B(0x937F8E) = m;
    B(kTimer) = 0;
    B(kState) = state;
}

// original 0x58CD40: FieldEquip_States[7] - the screen closed after 4 frames.
extern "C" void __cdecl FieldEquip_Close(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(kTimer) + 1);
    B(kTimer) = t;
    if (t != 4) return;
    SH_CALL(FieldEquip_CloseWindows)();
    U k = 0;
    if (PartyCount() != 0) {
        do {
            B(0x8031F3 + 0x24u * k) = 4;
            ++k;
        } while (static_cast<int>(k) < static_cast<int>(PartyCount()));
    }
    const UC m = B(kMember);
    B(0x80325F) = 2;
    B(0x803283) = 2;
    B(0x8032A7) = 2;
    B(0x8032CB) = 2;
    B(0x937F8E) = m;
    B(kTimer) = 0;
    B(kMode) = 1;
    B(kState) = 0;
}

// original 0x58CDD0: FieldEquip_States[8] - a slot's item taken off.
extern "C" void __cdecl FieldEquip_RemoveSlot(void) {
    Backdrop();
    unsigned char* const record = Record(PartyRecord(B(0x803340)));
    U category;
    unsigned char* cell;
    switch (B(0x80333E)) {
    case 1: category = 2; cell = record + 0x13; break;
    case 2: category = 2; cell = record + 0x14; break;
    case 3: category = 2; cell = record + 0x15; break;
    case 4: category = 3; cell = record + 0x16; break;
    case 5: category = 3; cell = record + 0x17; break;
    default: category = 1; cell = record + 0x12; break;
    }
    const U help = SH_CALL(Item_HelpMessage)(category, *cell);
    const U x = L(0x803338) + 6;
    SetW(0x8032FC, help);
    const U y = 13u * B(0x80333E) + W(0x80333A) + 0x57;
    B(0x803454) = 1;
    SetW(0x803458, x);
    const U pressed = W(kPressed) & 0x5000;
    SetW(0x80345A, y);
    const U repeat = AutoRepeat(pressed);
    if (repeat & 0x4000) {
        Sound(0x101);
        const auto s = static_cast<UC>(B(0x80333E) + 1);
        B(0x80333E) = s;
        if (s > 5) B(0x80333E) = 1;
    } else if (repeat & 0x1000) {
        Sound(0x101);
        const auto s = static_cast<UC>(B(0x80333E) - 1);
        B(0x80333E) = s;
        if (s < 1) B(0x80333E) = 5;
    }
    const UC can = SH_CALL(FieldEquip_PreviewRemove)();
    if (Confirmed(W(kPressed))) {
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
    if (!Cancelled(W(kPressed))) return;
    Sound(0x106);
    const auto state = static_cast<UC>(B(kState) - 5);
    B(0x803417) = 1;
    B(0x803454) = 0;
    B(0x80333E) = 0xFF;
    B(kTimer) = 0;
    B(kState) = state;
}

// original 0x58CFC0: the Equipment screen's windows.
extern "C" void __cdecl FieldEquip_InitWindows(void) {
    B(0x8032ED) = 6;
    B(0x8032EE) = 9;
    B(0x8032EF) = 2;
    B(0x8032EC) = 1;
    SetW(0x8032F0, 0x14);
    SetW(0x8032F2, 0xFFEC);
    SetW(0x8032FC, 0);
    B(0x803311) = 6;
    B(0x803312) = 0xD;
    B(0x803313) = 2;
    B(0x803310) = 1;
    SetW(0x803314, 0x40);
    SetW(0x803316, 0xFFEC);
    B(0x80331A) = 1;
    B(0x80331B) = 0xFF;
    B(0x803335) = 6;
    B(0x803336) = 0x10;
    B(0x803337) = 2;
    B(0x803334) = 1;
    SetW(0x803338, 0x140);
    SetW(0x80333A, 0x3E);
    B(0x803340) = 0;
    B(0x80333E) = 0xFF;
    B(0x80333F) = 0xFF;
    B(0x803341) = 1;
    move_script::SetLong(At(0x803354), static_cast<std::int32_t>(kPreview));
    B(0x80340D) = 6;
    B(0x80340E) = 0xA;
    B(0x80340C) = 0;
    B(0x803416) = 1;
    B(0x803417) = 1;
    B(0x803455) = 6;
    B(0x803456) = 0xE;
    B(0x803457) = 0;
    B(0x803454) = 0;
}

// original 0x58D0C0: the preview of the highest +0x16 weapon and +0x14 armour
// the member can equip.
extern "C" void __cdecl FieldEquip_BestByPower(unsigned member) {
    const UC index = PartyRecord(static_cast<int>(member & 0xFF));
    const UC bit = Bit8(index);
    unsigned char* const record = Record(index);
    B(kPreview) = record[0x12];
    U best = B(kWeaponB + 28u * B(kPreview));
    for (U k = 0; k < 0x80; ++k) {
        const UC id = At(L(kIdLists + 4))[k];
        const UC power = B(kWeaponB + 28u * id);
        const U mask = SH_CALL(Item_EquipMask)(1, id);
        if ((bit & mask) == 0) continue;
        if (best < power || (best == power && B(kPreview) > id)) {
            best = B(kWeaponB + 28u * id);
            B(kPreview) = id;
        }
    }
    for (U j = 0; j < 3; ++j) {
        const U slot = kPreview + 1 + j;
        B(slot) = record[0x13 + j];
        best = B(kArmourB + 26u * B(slot));
        for (U k = 0; k < 0x80; ++k) {
            const UC id = At(L(kIdLists + 8))[k];
            const UC type = B(kArmourType + 26u * id);
            const UC power = B(kArmourB + 26u * id);
            if (type != 2 + j) continue;
            const U mask = SH_CALL(Item_EquipMask)(2, id);
            if ((bit & mask) == 0) continue;
            if (best < power || (best == power && B(slot) > id)) {
                best = B(kArmourB + 26u * id);
                B(slot) = id;
            }
        }
    }
    B(kPreview + 4) = record[0x16];
    B(kPreview + 5) = record[0x17];
}

// original 0x58D2E0: the preview by the lowest +0x14 / +0x13 first, then the
// highest +0x16 / +0x14 (docs/rest_2e.md section 7 for the armour's two quirks).
extern "C" void __cdecl FieldEquip_BestByOrder(unsigned member) {
    const UC index = PartyRecord(static_cast<int>(member & 0xFF));
    const UC bit = Bit8(index);
    unsigned char* const record = Record(index);
    B(kPreview) = record[0x12];
    const UC current = B(kPreview);
    U best_b = B(kWeaponB + 28u * current);
    UC best_a = B(kWeaponA + 28u * current);
    for (U k = 0; k < 0x80; ++k) {
        const UC id = At(L(kIdLists + 4))[k];
        const UC a = B(kWeaponA + 28u * id);
        const UC b = B(kWeaponB + 28u * id);
        const U mask = SH_CALL(Item_EquipMask)(1, id);
        if ((bit & mask) == 0) continue;
        const UC chosen = B(kPreview);
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
        best_b = B(kWeaponB + 28u * id);
        B(kPreview) = id;
        best_a = B(kWeaponA + 28u * id);
    }
    for (U j = 0; j < 3; ++j) {
        const U slot = kPreview + 1 + j;
        B(slot) = record[0x13 + j];
        const UC now = B(slot);
        best_b = B(kArmourB + 26u * now);
        best_a = B(kArmourA + 26u * now);
        for (U k = 0; k < 0x80; ++k) {
            const UC id = At(L(kIdLists + 8))[k];
            const UC a = B(kArmourA + 26u * id);
            const UC type = B(kArmourType + 26u * id);
            const UC b = B(kArmourB + 26u * id);
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
                take = B(kPreview) > id;   // the weapon's byte, not this slot's (Capcom's)
            if (!take) continue;
            const UC kept = B(kArmourA + 26u * id);
            B(slot) = id;
            best_a = kept;
            best_b = kept;   // +0x13, not +0x14 (Capcom's)
        }
    }
    B(kPreview + 4) = record[0x16];
    B(kPreview + 5) = record[0x17];
}

// original 0x58D570: the preview swapped into the member's six slots through
// the inventory.
extern "C" void __cdecl FieldEquip_ApplyPreview(void) {
    unsigned char* const record = Record(PartyRecord(B(0x803340)));
    for (U i = 0; i < 6; ++i) {
        const UC item = B(kPreview + i);
        if (item == 0) continue;
        unsigned char* const cell = record + 0x12 + i;
        if (item == *cell) continue;
        const UC category = B(AddrOf(FieldEquip_SlotCategories) + i);
        SH_CALL(Inventory_Remove)(category, item, 1);
        const UC old = *cell;
        const UC again = B(AddrOf(FieldEquip_SlotCategories) + i);
        SH_CALL(Inventory_Add)(again, old, 1);
        *cell = B(kPreview + i);
    }
    SH_CALL(Char_RecalcStats)(record);
}

// original 0x58D640: the preview with the chosen item in the slot.
extern "C" void __cdecl FieldEquip_PreviewItem(void) {
    const UC item = B(0x803365);
    const UC index = PartyRecord(B(0x803340));
    const UC category = B(0x803360);
    const U mask = SH_CALL(Item_EquipMask)(category, item);
    B(0x803341) = (Bit8(index) & mask) == 0 ? 1 : 0;
    const unsigned char* const record = Record(index);
    B(kPreview + 1) = record[0x13];
    B(kPreview) = record[0x12];
    B(kPreview + 3) = record[0x15];
    B(kPreview + 2) = record[0x14];
    B(kPreview + 4) = record[0x16];
    const U slot = B(0x80333E);
    B(kPreview + 5) = record[0x17];
    B(kPreview + slot) = item;
}

// original 0x58D700: the preview with the slot's item taken off; al 1 when
// there was one.
extern "C" unsigned char __cdecl FieldEquip_PreviewRemove(void) {
    const unsigned char* const record = Record(PartyRecord(B(0x803340)));
    B(kPreview) = record[0x12];
    B(kPreview + 1) = record[0x13];
    B(kPreview + 2) = record[0x14];
    B(kPreview + 3) = record[0x15];
    B(kPreview + 5) = record[0x17];
    const U slot = B(0x80333E);
    B(kPreview + 4) = record[0x16];
    if (B(kPreview + slot) != 0) {
        B(kPreview + slot) = 0;
        B(0x803341) = 0;
        return 1;
    }
    B(0x803341) = 1;
    return 0;
}

// original 0x58D7B0: two of the Equipment screen's windows closed.
extern "C" void __cdecl FieldEquip_CloseWindows(void) {
    B(0x803334) = 0;
    B(0x803358) = 0;
}

// ===========================================================================
// The Ability screen
// ===========================================================================

// original 0x58D7C0: FieldMenu_States[3], a tail jump through
// FieldAbility_States by the state byte, unchecked.
extern "C" void __cdecl FieldAbility_Run(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(Entry(AddrOf(FieldAbility_States), 10, B(kState), "FieldAbility_Run"))();
}

// original 0x58D7D0: FieldAbility_States[0].
extern "C" void __cdecl FieldAbility_Open(void) {
    Backdrop();
    SH_AT(Handler, at::kAbilityWindows)();
    Sound(0x102);
    const auto state = static_cast<UC>(B(kState) + 1);
    const UC member = B(0x905BA1);
    B(kTimer) = 5;
    B(kMember) = member;
    B(kAbilityCursor) = 0;
    B(kState) = state;
}

// original 0x58D820: FieldAbility_States[2] - the four-entry menu.
extern "C" void __cdecl FieldAbility_TopMenu(void) {
    Backdrop();
    const int c = S8(B(kAbilityCursor));
    const U help = W(static_cast<U>(static_cast<int>(AddrOf(FieldAbility_TopHelp)) + 4 * c));
    const U x = L(0x803314) + static_cast<U>(48 * c);
    SetW(0x8032FC, help);
    const U pressed = W(kPressed) & 0xA000;
    B(0x8033A8) = 0;
    const U y = W(0x803316) + 4;
    B(0x803454) = 1;
    SetW(0x803458, x);
    SetW(0x80345A, y);
    B(0x80331B) = 0xFF;
    const U repeat = AutoRepeat(pressed);
    const UC was = B(kAbilityCursor);
    UC cl = TurnFour(kAbilityCursor, repeat);
    if (was != cl) {
        Sound(0x101);
        cl = B(kAbilityCursor);
    }
    const U p = W(kPressed);
    if (Confirmed(p)) {
        B(0x80331B) = cl;
        Sound(0x104);
        const UC chosen = B(kAbilityCursor);
        if (chosen == 1) {
            const auto state = static_cast<UC>(B(kState) + 5);
            B(kStep) = 0;
            B(kState) = state;
            return;
        }
        if (chosen == 3) {
            const auto state = static_cast<UC>(B(kState) + 7);
            B(0x8033A3) = 1;
            B(kTimer) = 7;
            B(kState) = state;
            B(kStep) = 0;
            return;
        }
        if (chosen == 2) B(0x8033AB) = 3;
        B(kState) = static_cast<UC>(B(kState) + 1);
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x102);
    Sound(0x106);
    const auto state = static_cast<UC>(B(kState) + 6);
    B(0x8032EF) = 1;
    B(0x803313) = 1;
    B(0x8033A3) = 1;
    B(0x803454) = 0;
    B(kState) = state;
    B(kTimer) = 5;
}

// original 0x58D9C0: FieldAbility_States[3] - the member and the list type.
extern "C" void __cdecl FieldAbility_PickMember(void) {
    Backdrop();
    const UC cursor = B(kAbilityCursor);
    const UC window = B(0x8031F3);
    B(0x803454) = 0;
    B(0x8033A8) = cursor != 0 ? 3 : 1;
    const UC member = B(0x8033AA);
    if (window == 0) {
        const U wx = W(0x8031F4);
        B(0x80340C) = 1;
        B(0x803417) = 1;
        SetW(0x803410, wx);
        SetW(0x803412, 54u * member + W(0x8031F6));
    } else {
        B(0x80340C) = 0;
    }
    SH_CALL(TextRecord_Set)(0, 8, Record(PartyRecord(member)));
    const U pressed = W(kPressed) & 0xF000;
    SetW(0x8032FC, 0x2F);
    const U repeat = AutoRepeat(pressed);
    B(0x8033AC) = 0xFF;
    B(0x8033AD) = 0xFF;
    const UC n = PartyCount();
    const UC was = B(0x8033AA);
    MoveAbilityMember(repeat, n);
    if (B(kAbilityCursor) == 2) {
        SetW(0x8033B0, 1);
    } else {
        SetW(0x8033B0, 0);
        UC cl;
        TurnType(repeat, cl);
    }
    if (static_cast<int>(B(0x8033AA)) != S8(was)) Sound(0x101);
    const U p = W(kPressed);
    const UC now = B(0x8033AA);
    B(kMember) = now;
    if (Confirmed(p)) {
        Sound(0x104);
        const int m = S8(B(kMember));
        const U type = B(0x8033AB);
        const UC kept = B(static_cast<U>(static_cast<int>(kAbilityCursors + type) + 4 * m));
        const auto state = static_cast<UC>(B(kState) + 1);
        B(0x8033AC) = kept;
        B(kState) = state;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto state = static_cast<UC>(B(kState) - 1);
    B(0x80340C) = 0;
    B(kState) = state;
}

// original 0x58DC40: FieldAbility_States[4] - the ability.
extern "C" void __cdecl FieldAbility_PickAbility(void) {
    Backdrop();
    const UC window = B(0x8031F3);
    const UC member = B(0x8033AA);
    if (window == 0) {
        const U wx = W(0x8031F4);
        B(0x80340C) = 1;
        B(0x803417) = 0;
        SetW(0x803410, wx);
        SetW(0x803412, 54u * member + W(0x8031F6));
    } else {
        B(0x80340C) = 0;
    }
    const UC type0 = B(0x8033AB);
    const unsigned char* const list = AbilityList(member, type0);
    const UC id = list[B(0x8033AC)];
    const U help = W(kAbilityHelp + 24u * id);
    const UC busy = B(0x8033A3);
    SetW(0x8032FC, help);
    if (busy == 0) {
        const U cur = B(0x8033AC);
        const U x = L(0x8033A4) + 7;
        B(0x803454) = 1;
        SetW(0x803458, x);
        SetW(0x80345A, 13u * (cur + 2) + W(0x8033A6));
    } else {
        B(0x803454) = 0;
    }
    const U repeat = AutoRepeat(W(kPressed) & 0xF000);
    UC al, cl;
    bool turned = false;
    if (B(kAbilityCursor) != 2) turned = TurnType(repeat, cl);
    if (turned) {
        const int m = S8(B(kMember));
        al = B(static_cast<U>(static_cast<int>(kAbilityCursors + cl) + 4 * m));
        B(0x8033AC) = al;
    } else {
        al = B(0x8033AC);
        cl = B(0x8033AB);
    }
    const UC was = al;
    if (repeat & 0x1000) {
        if (al > 0) {
            --al;
            B(0x8033AC) = al;
        }
    } else if (repeat & 0x4000) {
        if (al < 9) {
            ++al;
            B(0x8033AC) = al;
        }
    }
    if (was != al) {
        Sound(0x100);
        al = B(0x8033AC);
        cl = B(0x8033AB);
    }
    {
        const int m = S8(B(kMember));
        B(static_cast<U>(static_cast<int>(kAbilityCursors + cl) + 4 * m)) = al;
    }
    const U p = W(kPressed);
    if (Confirmed(p)) {
        const unsigned char* const chosen = AbilityList(B(0x8033AA), cl);
        const UC ability = chosen[B(0x8033AC)];
        if (ability == 0) {
            Sound(0x107);
            return;
        }
        if (B(kAbilityCursor) != 0) {
            const UC mode = B(0x8033A8);
            const UC user = B(0x8033AA);
            const UC can = SH_CALL(Skill_CanUse)(mode, user, ability);
            if (can == 0) {
                Sound(0x107);
                return;
            }
            const auto state = static_cast<UC>(B(kState) + 2);
            const UC kept = B(0x8033AC);
            B(0x8033AD) = kept;
            B(kAnswer) = 0;
            B(kState) = state;
            return;
        }
        {
            const UC mode = B(0x8033A8);
            const UC user = B(0x8033AA);
            const UC can = SH_CALL(Skill_CanUse)(mode, user, ability);
            if (can == 0) {
                Sound(0x107);
                return;
            }
        }
        if ((B(kAbilityFlags + 24u * ability) & 0x10) != 0) {
            const unsigned char* const again = AbilityList(B(0x8033AA), B(0x8033AB));
            const UC used = again[B(0x8033AC)];
            const UC user = PartyRecord(B(0x8033AA));
            const UC answer = AbilityUse(user, 0, used);
            B(kAbilityResult) = answer;
            Sound(answer == 1 || answer == 5 ? 0x109 : 0x107);
            return;
        }
        Sound(0x103);
        UC k = 0;
        if (PartyCount() != 0) {
            do {
                const U o = 36u * k;
                B(0x80333E + o) = k;
                const U y = 54u * k + 0x3E;
                ++k;
                B(0x803335 + o) = 6;
                B(0x803334 + o) = 1;
                B(0x803336 + o) = 0;
                B(0x803337 + o) = 0xA;
                B(0x80333F + o) = 0;
                B(0x803340 + o) = 0;
                SetW(0x803338 + o, 0x140);
                SetW(0x80333A + o, y);
                B(0x8031F3 + o) = 7;
            } while (k < PartyCount());
        }
        const UC kept = B(0x8033AC);
        B(0x8033AD) = kept;
        const auto state = static_cast<UC>(B(kState) + 1);
        B(0x8033A3) = 4;
        B(0x803454) = 1;
        B(0x80340C) = 0;
        B(kTarget) = 0;
        B(kState) = state;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto state = static_cast<UC>(B(kState) - 1);
    B(0x8033AC) = 0xFF;
    B(0x8033AD) = 0xFF;
    B(kState) = state;
}

// original 0x58E070: FieldAbility_States[5] - the target.
extern "C" void __cdecl FieldAbility_PickTarget(void) {
    Backdrop();
    const UC window = B(0x803337);
    B(0x803454) = 0;
    if (window == 0) {
        const int t = S8(B(kTarget));
        const U wx = W(0x803338);
        B(0x80340C) = 1;
        B(0x803417) = 1;
        SetW(0x803410, wx);
        SetW(0x803412, static_cast<U>(54 * t) + W(0x80333A));
    } else {
        B(0x80340C) = 0;
    }
    const U repeat = AutoRepeat(W(kPressed) & 0xF000);
    const UC n = PartyCount();
    const UC was = B(kTarget);
    const UC cl = MoveMemberS8(kTarget, repeat, n);
    if (was != cl) Sound(0x101);
    const unsigned char* const list = AbilityList(B(0x8033AA), B(0x8033AB));
    const UC ability = list[B(0x8033AC)];
    SH_CALL(TextRecord_Set)(0, 0x10, At(kAbilityRecords + 24u * ability));
    const U p = W(kPressed);
    SetW(0x8032FC, 0x30);
    if (Confirmed(p)) {
        const unsigned char* const again = AbilityList(B(0x8033AA), B(0x8033AB));
        const UC used = again[B(0x8033AC)];
        const UC target = PartyRecord(S8(B(kTarget)));
        const UC user = PartyRecord(B(0x8033AA));
        const UC answer = AbilityUse(user, target, used);
        B(kAbilityResult) = answer;
        Sound(answer == 1 || answer == 5 ? 0x109 : 0x107);
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    Sound(0x102);
    UC k = 0;
    B(0x80340C) = 0;
    if (PartyCount() != 0) {
        do {
            const U o = 36u * k;
            ++k;
            B(0x803337 + o) = 9;
            B(0x8031F3 + o) = 8;
        } while (k < PartyCount());
    }
    const auto state = static_cast<UC>(B(kState) - 1);
    B(0x8033A3) = 3;
    B(0x8033AD) = 0xFF;
    B(kState) = state;
}

// original 0x58E2C0: FieldAbility_States[6] - the yes / no to add the ability
// to the shared list.
extern "C" void __cdecl FieldAbility_ShareConfirm(void) {
    Backdrop();
    const int answer = S8(B(kAnswer));
    const U x = L(0x8032F0) - static_cast<U>(36 * answer) + 0xE8;
    SetW(0x80345A, W(0x8032F2) + 5);
    U pressed = L(kPressed);
    B(0x803454) = 1;
    SetW(0x803458, x);
    SetW(0x8032FC, 0x31);
    if (pressed & 0xA000) {
        Sound(0x101);
        B(kAnswer) = static_cast<UC>(B(kAnswer) ^ 1);
        pressed = L(kPressed);
    }
    if (Confirmed(pressed)) {
        if (B(kAnswer) != 0) {
            Sound(0x104);
            const unsigned char* const list = AbilityList(B(0x8033AA), B(0x8033AB));
            const UC ability = list[B(0x8033AC)];
            SH_CALL(AbilityList_Add)(ability, 0, 1, 0);
            unsigned char* const again = const_cast<unsigned char*>(AbilityList(B(0x8033AA), B(0x8033AB)));
            again[B(0x8033AC)] = 0;
        } else {
            Sound(0x106);
        }
        const auto state = static_cast<UC>(B(kState) - 2);
        B(0x8033AD) = 0xFF;
        B(kState) = state;
        return;
    }
    if (!Cancelled(pressed)) return;
    const auto state = static_cast<UC>(B(kState) - 2);
    B(0x8033AD) = 0xFF;
    B(kState) = state;
    Sound(0x106);
}

// original 0x58E410: FieldAbility_States[7], a tail jump through
// FieldAbility_ArrangeSteps by the step byte, unchecked.
extern "C" void __cdecl FieldAbility_ArrangeRun(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(
        Entry(AddrOf(FieldAbility_ArrangeSteps), 3, B(kStep), "FieldAbility_ArrangeRun"))();
}

// original 0x58E420: FieldAbility_ArrangeSteps[0] - the member and the type.
extern "C" void __cdecl FieldAbility_ArrangeMember(void) {
    Backdrop();
    const U member = B(0x8033AA);
    const U wx = W(0x8031F4);
    SetW(0x8032FC, 0x17);
    SetW(0x803410, wx);
    const U hand_y = W(0x8033A6);
    B(0x80340C) = 1;
    const U x = L(0x8033A4) + 0x2A;
    B(0x803417) = 1;
    SetW(0x803412, 54u * member + W(0x8031F6));
    const U pressed = W(kPressed) & 0xF00C;
    B(0x803454) = 1;
    SetW(0x803458, x);
    SetW(0x80345A, hand_y + 7);
    const U repeat = AutoRepeat(pressed);
    B(0x8033AD) = 0xFF;
    const UC n = PartyCount();
    MoveAbilityMember(repeat, n);
    UC cl;
    TurnType(repeat, cl);
    B(kMember) = B(0x8033AA);
    const U p = W(kPressed);
    if (Confirmed(p)) {
        Sound(0x103);
        const UC three = B(0x8033AB) == 3 ? 1 : 0;
        const auto step = static_cast<UC>(B(kStep) + 1);
        B(0x80340C) = 0;
        B(0x8033E9) = 6;
        B(0x8033EA) = 0xF;
        B(0x8033E8) = 1;
        SetW(0x8033EC, 0x28);
        SetW(0x8033EE, 0x3C);
        B(0x8033F2) = static_cast<UC>(three + 5);
        B(0x8033F3) = 0;
        B(kStep) = step;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto state = static_cast<UC>(B(kState) - 5);
    B(0x80340C) = 0;
    B(kState) = state;
}

// original 0x58E640: FieldAbility_ArrangeSteps[1] - the sort, a row of
// FieldAbility_ArrangeRows.
extern "C" void __cdecl FieldAbility_ArrangeHow(void) {
    Backdrop();
    const U row = B(0x8033F3);
    const U pressed = W(kPressed) & 0x5000;
    const U x = L(0x8033EC) + 5;
    const U y = (row << 4) + W(0x8033EE) + 0x1A;
    SetW(0x8032FC, 0x18);
    B(0x8033F4) = 0xFF;
    B(0x803454) = 1;
    SetW(0x803458, x);
    SetW(0x80345A, y);
    B(0x8033AC) = 0xFF;
    const U repeat = AutoRepeat(pressed);
    const U which = B(0x8033AB) == 3 ? 1 : 0;
    const U rows = AddrOf(FieldAbility_ArrangeRows) + 6 * which;
    if (repeat & 0x1000) {
        Sound(0x100);
        const auto c = static_cast<UC>(B(0x8033F3) - 1);
        B(0x8033F3) = c;
        if (S8(c) < 0) B(0x8033F3) = static_cast<UC>(B(rows) - 1);
    } else if (repeat & 0x4000) {
        Sound(0x100);
        const auto c = static_cast<UC>(B(0x8033F3) + 1);
        B(0x8033F3) = c;
        if (static_cast<int>(c) > static_cast<int>(B(rows)) - 1) B(0x8033F3) = 0;
    }
    const U p = W(kPressed);
    if (Confirmed(p)) {
        Sound(0x103);
        const UC how = B(rows + 1 + B(0x8033F3));
        if (how != 0) {
            SH_AT(void (__cdecl*)(unsigned), at::kAbilitySort)(how);
            return;
        }
        const int m = S8(B(kMember));
        B(0x8033AD) = 0xFF;
        const U type = B(0x8033AB);
        const UC kept = B(static_cast<U>(static_cast<int>(kAbilityCursors + type) + 4 * m));
        const auto step = static_cast<UC>(B(kStep) + 1);
        B(0x8033AC) = kept;
        B(kStep) = step;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    const auto step = static_cast<UC>(B(kStep) - 1);
    B(0x8033E8) = 0;
    B(kStep) = step;
}

// original 0x58E7E0: FieldAbility_ArrangeSteps[2] - an ability picked and
// swapped with another.
extern "C" void __cdecl FieldAbility_ArrangeMove(void) {
    Backdrop();
    const U cur = B(0x8033AC);
    const U x = L(0x8033A4) + 7;
    SetW(0x803458, x);
    const UC type = B(0x8033AB);
    B(0x8033F4) = 0;
    const UC member = B(0x8033AA);
    B(0x803454) = 1;
    SetW(0x80345A, 13u * (cur + 2) + W(0x8033A6));
    const unsigned char* const list = AbilityList(member, type);
    const UC ability = list[B(0x8033AC)];
    const U pressed = W(kPressed) & 0x500C;
    SetW(0x8032FC, W(kAbilityHelp + 24u * ability));
    const U repeat = AutoRepeat(pressed);
    UC cl = B(0x8033AC);
    const UC was = cl;
    if (repeat & 0x1000) {
        if (cl > 0) {
            --cl;
            B(0x8033AC) = cl;
        }
    } else if (repeat & 0x4000) {
        if (cl < 9) {
            ++cl;
            B(0x8033AC) = cl;
        }
    }
    {
        const int m = S8(B(kMember));
        const U t = B(0x8033AB);
        B(static_cast<U>(static_cast<int>(kAbilityCursors + t) + 4 * m)) = cl;
    }
    if (was != cl) Sound(0x100);
    const U p = W(kPressed);
    if (Confirmed(p)) {
        if (B(0x8033AD) == 0xFF) {
            const unsigned char* const again = AbilityList(B(0x8033AA), B(0x8033AB));
            if (again[B(0x8033AC)] != 0) {
                Sound(0x103);
                B(0x8033AD) = B(0x8033AC);
                return;
            }
            Sound(0x107);
            return;
        }
        Sound(0x103);
        const UC type1 = B(0x8033AB);
        const UC member1 = B(0x8033AA);
        const unsigned char* const at_cursor = AbilityList(member1, type1);
        unsigned char* const first = const_cast<unsigned char*>(at_cursor) + B(0x8033AC);
        const UC member2 = B(0x8033AA);
        const UC type2 = B(0x8033AB);
        const unsigned char* const at_pick = AbilityList(member2, type2);
        unsigned char* const second = const_cast<unsigned char*>(at_pick) + B(0x8033AD);
        Swap(second, first);
        B(0x8033AD) = 0xFF;
        return;
    }
    if (!Cancelled(p)) return;
    Sound(0x106);
    if (B(0x8033AD) == 0xFF) {
        B(kStep) = static_cast<UC>(B(kStep) - 1);
        return;
    }
    B(0x8033AD) = 0xFF;
}

// original 0x58E9D0: FieldAbility_States[8] - the screen closed.
extern "C" void __cdecl FieldAbility_Close(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(kTimer) - 1);
    B(kTimer) = t;
    if (t != 0) return;
    SH_AT(Handler, at::kAbilityCloseWindows)();
    U k = 0;
    if (PartyCount() != 0) {
        do {
            B(0x8031F3 + 0x24u * k) = 4;
            ++k;
        } while (static_cast<int>(k) < static_cast<int>(PartyCount()));
    }
    const UC m = B(kMember);
    B(0x80325F) = 2;
    B(0x803283) = 2;
    B(0x8032A7) = 2;
    B(0x8032CB) = 2;
    B(0x905BA1) = m;
    B(kTimer) = 0;
    B(kMode) = 1;
    B(kState) = 0;
}

// original 0x58EA60: FieldAbility_States[9], a tail jump through
// FieldAbility_ViewSteps by the step byte, unchecked.
extern "C" void __cdecl FieldAbility_ViewRun(void) {
    [[clang::musttail]] return reinterpret_cast<Handler>(
        Entry(AddrOf(FieldAbility_ViewSteps), 5, B(kStep), "FieldAbility_ViewRun"))();
}

// original 0x58EA70: FieldAbility_ViewSteps[0].
extern "C" void __cdecl FieldAbility_ViewOpen(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(kTimer) - 1);
    B(kTimer) = t;
    if (t != 0) return;
    SH_AT(Handler, at::kAbilityViewWindow)();
    const UC cursor = B(kViewCursor);
    const UC top = B(kViewTop);
    B(0x8033CF) = cursor;
    B(0x8033CE) = top;
    Sound(0x102);
    const auto step = static_cast<UC>(B(kStep) + 1);
    B(kTimer) = 7;
    B(kStep) = step;
}

// original 0x58EAD0: FieldAbility_ViewSteps[1].
extern "C" void __cdecl FieldAbility_ViewWait(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(kTimer) - 1);
    B(kTimer) = t;
    if (t == 0) B(kStep) = static_cast<UC>(B(kStep) + 1);
}

// original 0x58EB00: FieldAbility_ViewSteps[2] - the list scrolled.
extern "C" void __cdecl FieldAbility_ViewBrowse(void) {
    Backdrop();
    const UC h = B(0x8033D0);
    SetW(0x8032FC, h != 0 ? h + 0x4183u : 0);
    const U top = B(0x8033CE);
    const U cur = B(0x8033CF);
    const U x = L(0x8033C8) + 7;
    SetW(0x803458, x);
    const U pressed = W(kPressed) & 0x500C;
    B(0x803454) = 1;
    SetW(0x80345A, 13u * (cur - top + 2) + W(0x8033CA));
    const U repeat = AutoRepeat(pressed);
    const UC was = B(0x8033CF);
    UC cl, al;
    Scroll(repeat, kViewScroll, cl, al);
    B(kViewCursor) = cl;
    B(kViewTop) = al;
    if (was != cl) Sound(0x100);
    if (!Cancelled(W(kPressed))) return;
    Sound(0x106);
    const auto step = static_cast<UC>(B(kStep) + 1);
    B(0x803454) = 0;
    B(0x8033C7) = 1;
    B(kTimer) = 7;
    B(kStep) = step;
}

// original 0x58ECC0: FieldAbility_ViewSteps[3].
extern "C" void __cdecl FieldAbility_ViewLeave(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(kTimer) - 1);
    B(kTimer) = t;
    if (t != 0) return;
    B(0x8033A3) = 2;
    Sound(0x102);
    const auto step = static_cast<UC>(B(kStep) + 1);
    B(kTimer) = 7;
    B(kStep) = step;
}

// original 0x58ED10: FieldAbility_ViewSteps[4].
extern "C" void __cdecl FieldAbility_ViewEnd(void) {
    Backdrop();
    const auto t = static_cast<UC>(B(kTimer) - 1);
    B(kTimer) = t;
    if (t != 0) return;
    const UC state = B(kState);
    B(kStep) = 0;
    B(kState) = static_cast<UC>(state - 7);
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
