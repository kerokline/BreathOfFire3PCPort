// The shop's buy and sell states - group DG of the eighth takeover round
// (docs/takeover-queue-round8.md): the PSX SHOP.EMI's state machine compiled
// into the exe, reached only through the tables ShopTrade_States 0x664118 and
// the four step tables after it; the shop's close 0x584F70; and task 0's
// title loop 0x588E70 with its mode 0 0x588EB0. Each read to its last
// instruction with capstone against bof3/BOF3.exe, 2026-09-25
// (docs/shop_states2.md), entry and size:
//
//   ShopTrade_Step        0x5818B0 0x0E     ShopTrade_BuyMember   0x5822C0 0x205
//   ShopTrade_OpenStep    0x5818C0 0x0E     ShopTrade_BuySlot     0x5824D0 0x1CD
//   ShopTrade_Open        0x5818D0 0x9D     ShopTrade_BuyEquipped 0x5826A0 0xCE
//   ShopTrade_OpenWait    0x581970 0x3D     ShopTrade_SellStep    0x582770 0x0E
//   ShopTrade_ChoiceStep  0x5819B0 0x0E     ShopTrade_SellSetup   0x582780 0x6B
//   ShopTrade_Choice      0x5819C0 0x11C    ShopTrade_SellList    0x5827F0 0x366
//   ShopTrade_BuyStep     0x581AE0 0x0E     ShopTrade_SellCount   0x582B60 0x192
//   ShopTrade_BuySetup    0x581AF0 0xE8     ShopTrade_SellConfirm 0x582D00 0x177
//   ShopTrade_BuyList     0x581BE0 0x2ED    ShopTrade_SellClose   0x582E80 0x24
//   ShopTrade_BuyCount    0x581ED0 0x1B4    ShopTrade_Close       0x584F70 0x1B
//   ShopTrade_BuyConfirm  0x582090 0x147    TitleTask_Run         0x588E70 0x3A
//   ShopTrade_BuyAskEquip 0x5821E0 0xDF     TitleMode_Load        0x588EB0 0x44
//
// Faithful but for DIV-0027's yes / no hand under a Latin overlay
// (YesNoFrame, amended 2026-10-03). Every call out goes through shop_states2::g, so
// the start-up fuzz can stand recorders in for the callees.
//
// Registers the originals push whole - a byte loaded into al over whatever
// eax held - are passed here zero-extended: every callee reads the byte (or,
// Shop_ScalePrice's rate and Sound_PlayEffect's id, the word) and nothing
// more; docs/shop_states2.md section 3 lists each.
#include "game/shop_states2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/lang_layout.h"
#include "game/shop_states2_callees.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace shop_states2 {

const Callees kOriginals = {
    Shop_InitWindows, Shop_PriceRate, Shop_ScalePrice, Shop_SellPrice, Shop_Equip,
    Item_IconKind, Item_HelpMessage, Item_BasePrice, Inventory_Count, Inventory_Add, Item_NamePtr, TextRecord_Set,
    Item_EquipMask, Item_CanUse, Char_RecalcStats, Party_Count,
    Input_AutoRepeat, Sound_PlayEffect,
    Window_ResetAll, Task_ClearPrivate, Task_Sleep, Field_RunTaskRecords, LoadDatFile, File_LoadDone,
    Gfx_ClutStripRestore,
};
Callees g = kOriginals;

}  // namespace shop_states2

using namespace shop_states2;

// DIVERGENCE DIV-0027 (amended 2026-10-03, group YN): the shop's yes / no
// hand (YesNoFrame) over DIV-0027's re-spaced line, set with
// menu_draw_helpers.cpp's g_shop_yes_no_line under a Latin overlay, after
// the self-test, which compares the original's hand.
unsigned char g_shop_yes_no_hand = 0;

namespace {

unsigned char* AtShopStates2(shop_states2::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
shop_states2::U ByteShopStates2(shop_states2::U address) { return AtShopStates2(address)[0]; }
void PutByteShopStates2(shop_states2::U address, shop_states2::U v) { AtShopStates2(address)[0] = static_cast<unsigned char>(v); }
shop_states2::U Word(shop_states2::U address) {
    std::uint16_t v;
    std::memcpy(&v, AtShopStates2(address), sizeof v);
    return v;
}
void PutWord(shop_states2::U address, shop_states2::U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(AtShopStates2(address), &w, sizeof w);
}
shop_states2::U LongShopStates2(shop_states2::U address) {
    shop_states2::U v;
    std::memcpy(&v, AtShopStates2(address), sizeof v);
    return v;
}
void PutLong(shop_states2::U address, shop_states2::U v) { std::memcpy(AtShopStates2(address), &v, sizeof v); }
int S8(shop_states2::U v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// The shop's list (count, then category / item pairs), read afresh.
unsigned char* List() { return AtShopStates2(ShopStates2_ShopList); }
// The inventory list of a category: ids (0x656B00) or counts (0x656B14).
unsigned char* Ids(shop_states2::U kind) { return AtShopStates2(LongShopStates2(::bof3::Img(at::kIdLists.v) + kind * 4)); }
unsigned char* Counts(shop_states2::U kind) { return AtShopStates2(LongShopStates2(::bof3::Img(at::kCountLists.v) + kind * 4)); }
// The window record of party member i.
unsigned char* Panel(int i) { return AtShopStates2(::bof3::Img(WindowRecords_addr) + 0x6c + static_cast<shop_states2::U>(i) * at::kMemberStride); }
// Member m's record index: 0x66972C[0x904062[m]], m signed.
shop_states2::U RecordOf(int member) { return ByteShopStates2(::bof3::Img(at::kRecordOf.v) + ByteShopStates2(::bof3::Img(at::kPartyList.v) + static_cast<shop_states2::U>(member))); }
shop_states2::U PartyCount() { return static_cast<shop_states2::U>(shop_states2::g.party_count(0)) & 0xFF; }

bool Confirmed(shop_states2::U pressed) { return (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed & 0xFFFF) != 0; }
bool Cancelled(shop_states2::U pressed) { return (static_cast<std::uint32_t>(Field_CancelButtons) & pressed & 0xFFFF) != 0; }

void StepBy(int delta) { FieldS_Step = static_cast<std::uint32_t>(FieldS_Step) + static_cast<shop_states2::U>(delta); }

// The name of the chosen item into text record 0 - TextRecord_Set(0, 0x10,
// Item_NamePtr(kind, item)) - which five of the states do first.
void SetItemName() { shop_states2::g.text_set(0, 0x10, shop_states2::g.name_ptr(static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item))); }

// The yes / no prompt's frame (ShopTrade_BuyConfirm, _BuyAskEquip,
// _SellConfirm): the name, the help line `help`, the hand at x = 0x803164 -
// 36 * s8 answer + 0xE8, y = 0x803166 + 5; bits 0x8000 / 0x2000 of the
// pressed dword flip the answer with sound 0x100. Returns the pressed
// dword, read again after that sound.
shop_states2::U YesNoFrame(shop_states2::U help) {
    auto* const win21 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 21 * sizeof(records::WindowRecord));
    auto* const w0 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    SetItemName();
    const int answer = FieldS_Answer;
    const shop_states2::U y = static_cast<shop_states2::U>(w0->y);
    Window0_Arrows = help;
    if (g_shop_yes_no_hand)   // DIV-0027, amended 2026-10-03: over the line ShopWin_TitleRun draws at x + 7
        rv::Store<shop_states2::U>(win21->x, static_cast<shop_states2::U>(YesNoLayout_ShopHandX(static_cast<short>(static_cast<shop_states2::U>(w0->x)) + 7, answer)));
    else
        rv::Store<shop_states2::U>(win21->x, LongShopStates2(::bof3::Img(0x803164)) - static_cast<shop_states2::U>(answer * 36) + 0xE8);
    shop_states2::U pressed = LongShopStates2(::bof3::Img(at::kPressed.v));
    WindowRecords[21].in_use = 1;
    WindowRecords[21].y = y + 5;
    if ((pressed & 0xA000) != 0) {
        shop_states2::g.sound(0x100);
        FieldS_Answer = static_cast<std::uint32_t>(static_cast<unsigned char>(FieldS_Answer)) ^ 1;
        pressed = LongShopStates2(::bof3::Img(at::kPressed.v));
    }
    return pressed;
}

// A count between 1 and the most (ShopTrade_BuyCount, _SellCount): bit
// 0x8000 / 0x2000 of the repeat one down / up, then 0x4000 / 0x1000 ten, each
// followed by a clamp - below 1 as a signed byte is 1, above the most's low
// byte (unsigned) is the most - and by `sound` when the count differs from
// the one the function started with. As the originals have it: the second
// comparison is with the starting count too, so a step of one plays the
// sound twice in the frame; and a most of 0 leaves 0.
void StepCount(shop_states2::U repeat, unsigned short sound) {
    const shop_states2::U start = ShopStates2_Count;
    shop_states2::U c = start;
    if ((repeat & 0x8000) != 0) {
        c = (c - 1) & 0xFF;
        ShopStates2_Count = c;
    } else if ((repeat & 0x2000) != 0) {
        c = (c + 1) & 0xFF;
        ShopStates2_Count = c;
    }
    if (S8(c) < 1) {
        c = 1;
        ShopStates2_Count = c;
    }
    shop_states2::U most = ByteShopStates2(::bof3::Img(at::kMaxCount.v));
    if (c > most) {
        c = most;
        ShopStates2_Count = c;
    }
    if (start != c) {
        shop_states2::g.sound(sound);
        c = ShopStates2_Count;
        most = ByteShopStates2(::bof3::Img(at::kMaxCount.v));
    }
    if ((repeat & 0x4000) != 0) {
        c = (c - 10) & 0xFF;
        ShopStates2_Count = c;
    } else if ((repeat & 0x1000) != 0) {
        c = (c + 10) & 0xFF;
        ShopStates2_Count = c;
    }
    if (S8(c) < 1) {
        c = 1;
        ShopStates2_Count = c;
    }
    if (c > most) {
        c = most;
        ShopStates2_Count = c;
    }
    if (start != c) shop_states2::g.sound(sound);
}

// Every party member's panel byte +3: `self` for the member 0x929F06 names
// (re-read each time, compared as a byte), `other` for the rest, while the
// index is below Party_Count(0)'s low byte, asked again after each.
void MarkPanels(shop_states2::U self, shop_states2::U other) {
    shop_states2::U i = 0;
    if (PartyCount() == 0) return;
    for (;;) {
        Panel(static_cast<int>(i))[3] = static_cast<unsigned char>(BattleActorCopies_ItemActor == static_cast<int>(i) ? self : other);
        i = (i + 1) & 0xFF;
        if (i >= PartyCount()) return;
    }
}

using Handler = void (__cdecl*)();

// A dispatch's handler: entry `index` of a table in .data, read now. The
// index is not checked, as the originals have it: past its table the words
// after it are read - the next tables, then Shop_Equip's jump table and on.
// A null entry would be a call to 0 in the original; ours aborts.
Handler Entry(shop_states2::U table, shop_states2::U index, const char* who) {
    const shop_states2::U target = LongShopStates2(table + index * 4);
    if (target == 0) bof3::Fatal("%s: entry %u of the table 0x%X is null", who, static_cast<unsigned>(index), static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(target));
}

}  // namespace

// ============================================================================
// The dispatches: `xor eax, eax; mov al, [state]; jmp [eax * 4 + table]`. A
// jump, as the original's: the handler returns to our caller.
// ============================================================================

// original 0x5818B0: entry 2 of the field menus' table 0x663E40 (group DF's),
// the shop proper - ShopTrade_States by 0x929F01: 0 open, 1 buy or sell,
// 2 buy, 3 sell, 4 close.
extern "C" void __cdecl ShopTrade_Step(void) {
    const Handler h = Entry(::bof3::Img(ShopTrade_States_addr), static_cast<std::uint32_t>(FieldS_State), "ShopTrade_Step");
    [[clang::musttail]] return h();
}

// original 0x5818C0: state 0's two steps by 0x929F02 (0x66412C).
extern "C" void __cdecl ShopTrade_OpenStep(void) {
    const Handler h = Entry(::bof3::Img(ShopTrade_OpenSteps_addr), static_cast<std::uint32_t>(FieldS_Step), "ShopTrade_OpenStep");
    [[clang::musttail]] return h();
}

// original 0x5819B0: state 1's one step by 0x929F02 (0x664134).
extern "C" void __cdecl ShopTrade_ChoiceStep(void) {
    const Handler h = Entry(::bof3::Img(ShopTrade_ChoiceSteps_addr), static_cast<std::uint32_t>(FieldS_Step), "ShopTrade_ChoiceStep");
    [[clang::musttail]] return h();
}

// original 0x581AE0: state 2's eight steps by 0x929F02 (0x664138).
extern "C" void __cdecl ShopTrade_BuyStep(void) {
    const Handler h = Entry(::bof3::Img(ShopTrade_BuySteps_addr), static_cast<std::uint32_t>(FieldS_Step), "ShopTrade_BuyStep");
    [[clang::musttail]] return h();
}

// original 0x582770: state 3's five steps by 0x929F02 (0x664158; 0x66417C
// holds the same five for the menu kind 0x582EB0 dispatches).
extern "C" void __cdecl ShopTrade_SellStep(void) {
    const Handler h = Entry(::bof3::Img(ShopTrade_SellSteps_addr), static_cast<std::uint32_t>(FieldS_Step), "ShopTrade_SellStep");
    [[clang::musttail]] return h();
}

// Every function below keeps its calls as calls (docs/HANDOFF.md, Traps: the
// frame hash records return addresses).
#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ============================================================================
// State 0: open
// ============================================================================

// original 0x5818D0 (PSX 0x801D1154): Shop_InitWindows; the counter 6; the
// "sells equipment" byte 0; the step on (read after the call). Then each
// list entry (while the index is below the count byte, unsigned): its icon
// kind, Item_IconKind(category, item); the list pointer and the entry's
// category re-read after the call - a category not 0 with an icon kind not
// 0xA or 0xB sets the byte to 1 and ends the search. Then
// Shop_PriceRate(0x6BC8B4).
extern "C" void __cdecl ShopTrade_Open(void) {
    shop_states2::g.init_windows();
    const shop_states2::U sub = FieldS_Step;
    PutByteShopStates2(::bof3::Img(at::kCounter.v), 6);
    Rest2c_SellFlag = 0;
    FieldS_Step = sub + 1;
    unsigned char* list = List();
    if (list[0] != 0) {
        shop_states2::U i = 0;
        for (;;) {
            const shop_states2::U item = list[i * 2 + 2];
            const shop_states2::U kind = list[i * 2 + 1];
            const shop_states2::U icon = shop_states2::g.icon_kind(kind, item) & 0xFF;
            list = List();
            if (list[i * 2 + 1] != 0 && icon != 0xA && icon != 0xB) {
                Rest2c_SellFlag = 1;
                break;
            }
            i = (i + 1) & 0xFF;
            if (i >= list[0]) break;
        }
    }
    shop_states2::g.price_rate(reinterpret_cast<unsigned short*>(AtShopStates2(::bof3::Img(at::kRate.v))));
}

// original 0x581970: the counter down; at 0 the bytes 0x6BC8AA, 0x6BC8A8,
// 0x6BC8AB, the counter and 0x929F05 cleared, the state on (read before),
// the step 0.
extern "C" void __cdecl ShopTrade_OpenWait(void) {
    const shop_states2::U c = (ByteShopStates2(::bof3::Img(at::kCounter.v)) - 1) & 0xFF;
    PutByteShopStates2(::bof3::Img(at::kCounter.v), c);
    if (c != 0) return;
    const shop_states2::U state = FieldS_State;
    ShopStates2_SellFlag = 0;
    ShopStates2_ByteA8 = 0;
    ShopStates2_Choice = 0;
    PutByteShopStates2(::bof3::Img(at::kCounter.v), 0);
    MenuLists_Cursor = 0;
    FieldS_State = state + 1;
    FieldS_Step = 0;
}

// ============================================================================
// State 1: buy or sell
// ============================================================================

// original 0x5819C0: the help line 0x46 + s8 choice; the hand on at x =
// 0x8031AC + 48 * choice + 4, y = 0x8031AE + 4. The repeat of the pressed
// 0xA000 bits flips the choice with sound 0x101. Then, the pressed word read
// again: confirm - sounds 0x104 and 0x102, the counter 5, the step 0, the
// state + 1 for buy or + 2 for sell (both read after the sounds); cancel -
// sound 0x106, the hands off, the three window bytes 0x803163, 0x8031AB,
// 0x803187 to 1, the counter 5, the state + 3 (close).
extern "C" void __cdecl ShopTrade_Choice(void) {
    auto* const win21 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 21 * sizeof(records::WindowRecord));
    auto* const w0 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    auto* const w1 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 1 * sizeof(records::WindowRecord));
    auto* const w2 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 2 * sizeof(records::WindowRecord));
    auto* const w20 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 20 * sizeof(records::WindowRecord));
    const int choice = ShopStates2_Choice;
    Window0_Arrows = static_cast<shop_states2::U>(choice + 0x46);
    WindowRecords[21].in_use = 1;
    const shop_states2::U x = static_cast<shop_states2::U>(choice * 48) + LongShopStates2(::bof3::Img(0x8031AC)) + 4;
    const shop_states2::U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0xA000;
    const shop_states2::U y = static_cast<shop_states2::U>(w2->y) + 4;
    rv::Store<shop_states2::U>(win21->x, x);
    rv::Store<shop_states2::U>(win21->y, y);
    if ((shop_states2::g.auto_repeat(pressed) & 0xA000) != 0) {
        shop_states2::g.sound(0x101);
        ShopStates2_Choice = static_cast<std::uint32_t>(static_cast<unsigned char>(ShopStates2_Choice)) ^ 1;
    }
    const shop_states2::U p = Input_Pressed;
    if (Confirmed(p)) {
        shop_states2::g.sound(0x104);
        shop_states2::g.sound(0x102);
        const shop_states2::U sell = static_cast<unsigned char>(ShopStates2_Choice);
        const shop_states2::U state = FieldS_State;
        PutByteShopStates2(::bof3::Img(at::kCounter.v), 5);
        FieldS_Step = 0;
        FieldS_State = state + (sell == 0 ? 1 : 2);
        return;
    }
    if (Cancelled(p)) {
        shop_states2::g.sound(0x106);
        WindowRecords[21].in_use = 0;
        rv::Store<shop_states2::U>(w20->in_use, 0);
        const shop_states2::U state = FieldS_State;
        rv::Store<shop_states2::U>(w0->state, 1);
        rv::Store<shop_states2::U>(w2->state, 1);
        rv::Store<shop_states2::U>(w1->state, 1);
        PutByteShopStates2(::bof3::Img(at::kCounter.v), 5);
        FieldS_State = state + 3;
    }
}

// ============================================================================
// State 2: buy
// ============================================================================

// original 0x581AF0: when the list sells equipment, each party member's
// panel (while the index is below Party_Count(0)'s low byte, asked again
// after each): +0xA the index, +4 0xFF6A, +6 0x3E + 54 i, +0..+3 1 7 5 2,
// +0xB and +0xC 0. Then the buy list's window 0x803280: at (0x140, 0x3E),
// +3 2 with equipment and 3 without, +0..+2 1 7 7, +0xA 0, +0xB the rate's
// low byte, +0x20 the list pointer; the cursor 0, the step on.
extern "C" void __cdecl ShopTrade_BuySetup(void) {
    auto* const w8 = rv::As<records::ItemListWindow>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 8 * sizeof(records::WindowRecord));
    if (Rest2c_SellFlag != 0) {
        shop_states2::U i = 0;
        if (PartyCount() != 0) {
            for (;;) {
                unsigned char* const w = Panel(static_cast<int>(i));
                w[0xA] = static_cast<unsigned char>(i);
                PutWord(::bof3::Img(0x8031CC) + i * at::kMemberStride + 4, 0xFF6A);
                PutWord(::bof3::Img(0x8031CC) + i * at::kMemberStride + 6, i * 54 + 0x3E);
                w[0] = 1;
                w[1] = 7;
                w[2] = 5;
                w[3] = 2;
                w[0xB] = 0;
                w[0xC] = 0;
                i = (i + 1) & 0xFF;
                if (i >= PartyCount()) break;
            }
        }
    }
    const shop_states2::U sells = Rest2c_SellFlag;
    const shop_states2::U rate = ByteShopStates2(::bof3::Img(at::kRate.v));
    const shop_states2::U list = ShopStates2_ShopList;
    rv::Store<shop_states2::U>(w8->x, 0x140);
    rv::Store<shop_states2::U>(w8->y, 0x3E);
    rv::Store<shop_states2::U>(w8->state, sells == 0 ? 3 : 2);
    const shop_states2::U sub = FieldS_Step;
    rv::Store<shop_states2::U>(w8->in_use, 1);
    rv::Store<shop_states2::U>(w8->handler, 7);
    rv::Store<shop_states2::U>(w8->kind, 7);
    PutByteShopStates2(::bof3::Img(0x80328A), 0);
    PutByteShopStates2(::bof3::Img(0x80328B), rate);
    rv::Store<shop_states2::U>(w8->shop_list, list);
    ShopStates2_Row = 0;
    FieldS_Step = sub + 1;
}

// original 0x581BE0 (PSX 0x801D1634): the buy list. The entry under the
// cursor (s8 0x6BC8A9) becomes the chosen category and item; each party
// member's panel gets the item's icon kind (+0xB) and the item (+0xC, read
// after the call); the help line is Item_HelpMessage's word (0x591C20, a message
// id - section 1). With the list's window up (0x803283 = 0) the hand goes
// to the row: x = 0x803284 + 4, y = 0x803286 + 13 * row + 0xA, and the list
// pointer to 0x803474. The repeat of the pressed 0x5000 bits: 0x1000 up
// (below 0 wraps to the count less one), 0x4000 down (past the count less
// one, as signed bytes, wraps to 0); a move plays 0x100 and the list pointer
// and cursor are read again. The cursor to 0x80328C.
//
// Confirm: the entry under the cursor chosen, the count 1, the price
// Shop_ScalePrice(Item_BasePrice(category, item) & 0xFFFF, rate word) to
// 0x6BC8A0, the most the money buys (unsigned division) to 0x6BC8B0; then
// held = the low bytes of Inventory_Count(.., 1) + Inventory_Count(.., 0)
// as a byte: above 99 the count and the most are 0, otherwise the most is
// cut to 99 - held (the count then 1 if that is not 0, else 0). The money
// read again: at least the price - the answer 1, sound 0x103, the step on;
// less - sound 0x107. Cancel: sound 0x106, every panel's +3 to 1, the list's
// window +3 1, state 1 step 0.
//
// As the original has it: Shop_ScalePrice never answers 0, so its divide
// never faults; ours aborts where the original would fault.
extern "C" void __cdecl ShopTrade_BuyList(void) {
    auto* const win21 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 21 * sizeof(records::WindowRecord));
    auto* const w8 = rv::As<records::ItemListWindow>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 8 * sizeof(records::WindowRecord));
    auto* const w9 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 9 * sizeof(records::WindowRecord));
    const int row = ShopStates2_Row;
    unsigned char* list = List();
    ShopStates2_Kind = list[row * 2 + 1];
    ShopStates2_Item = list[row * 2 + 2];
    PutByteShopStates2(::bof3::Img(0x80328A), 0);
    shop_states2::U i = 0;
    if (PartyCount() != 0) {
        for (;;) {
            unsigned char* const w = Panel(static_cast<int>(i));
            const shop_states2::U icon = shop_states2::g.icon_kind(static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item));
            const shop_states2::U item = ShopStates2_Item;
            i = (i + 1) & 0xFF;
            w[0xB] = static_cast<unsigned char>(icon);
            w[0xC] = static_cast<unsigned char>(item);
            if (i >= PartyCount()) break;
        }
    }
    Window0_Arrows = shop_states2::g.help_id(static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item));
    const shop_states2::U open = static_cast<shop_states2::U>(w8->state);
    rv::Store<shop_states2::U>(w9->in_use, 0);
    if (open == 0) {
        const int r = ShopStates2_Row;
        const shop_states2::U x = LongShopStates2(::bof3::Img(0x803284)) + 4;
        WindowRecords[21].in_use = 1;
        rv::Store<shop_states2::U>(win21->x, x);
        PutLong(::bof3::Img(0x803474), ShopStates2_ShopList);
        WindowRecords[21].y = static_cast<shop_states2::U>(r * 13) + static_cast<shop_states2::U>(w8->y) + 0xA;
    }
    const shop_states2::U repeat = shop_states2::g.auto_repeat(static_cast<std::uint32_t>(Input_Pressed) & 0x5000);
    shop_states2::U cur = static_cast<unsigned char>(ShopStates2_Row);
    list = List();
    const shop_states2::U start = cur;
    if ((repeat & 0x1000) != 0) {
        cur = (cur - 1) & 0xFF;
        ShopStates2_Row = cur;
        if ((cur & 0x80) != 0) {
            cur = (list[0] - 1u) & 0xFF;
            ShopStates2_Row = cur;
        }
    } else if ((repeat & 0x4000) != 0) {
        cur = (cur + 1) & 0xFF;
        ShopStates2_Row = cur;
        if (S8(cur) > S8(list[0]) - 1) {
            cur = 0;
            ShopStates2_Row = cur;
        }
    }
    if (start != cur) {
        shop_states2::g.sound(0x100);
        list = List();
        cur = static_cast<unsigned char>(ShopStates2_Row);
    }
    const shop_states2::U p = Input_Pressed;
    PutByteShopStates2(::bof3::Img(0x80328C), cur);
    if (Confirmed(p)) {
        const int r = S8(cur);
        const shop_states2::U kind = list[r * 2 + 1];
        ShopStates2_Kind = kind;
        const shop_states2::U item = list[r * 2 + 2];
        const shop_states2::U rate = Word(::bof3::Img(at::kRate.v));
        ShopStates2_Item = item;
        ShopStates2_Count = 1;
        const shop_states2::U base = shop_states2::g.base_price(kind, item) & 0xFFFF;
        const shop_states2::U price = shop_states2::g.scale_price(base, rate);
        const shop_states2::U money = Party_Zenny;
        ShopStates2_UnitPrice = price;
        if (price == 0) bof3::Fatal("ShopTrade_BuyList: Shop_ScalePrice answered 0 - the original divides by it");
        PutLong(::bof3::Img(at::kMaxCount.v), money / price);
        const shop_states2::U n1 = shop_states2::g.inv_count(static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item), 1);
        const shop_states2::U n2 = shop_states2::g.inv_count(static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item), 0);
        const shop_states2::U held = (n1 + n2) & 0xFF;
        if (held > 0x63) {
            ShopStates2_Count = 0;
            PutLong(::bof3::Img(at::kMaxCount.v), 0);
        } else {
            const shop_states2::U room = (0x63 - held) & 0xFF;
            if (LongShopStates2(::bof3::Img(at::kMaxCount.v)) > room) {
                PutLong(::bof3::Img(at::kMaxCount.v), room);
                ShopStates2_Count = room != 0 ? 1 : 0;
            }
        }
        if (Party_Zenny >= ShopStates2_UnitPrice) {
            FieldS_Answer = 1;
            shop_states2::g.sound(0x103);
            StepBy(1);
        } else {
            shop_states2::g.sound(0x107);
        }
        return;
    }
    if (Cancelled(p)) {
        shop_states2::g.sound(0x106);
        shop_states2::U m = 0;
        if (PartyCount() != 0) {
            for (;;) {
                Panel(static_cast<int>(m))[3] = 1;
                m = (m + 1) & 0xFF;
                if (m >= PartyCount()) break;
            }
        }
        rv::Store<shop_states2::U>(w8->state, 1);
        FieldS_State = 1;
        FieldS_Step = 0;
    }
}

// original 0x581ED0 (PSX 0x801D1A88): how many to buy. The count window
// 0x8032A4 at (0x803284, 0x803286 + 0x2D) with the rate's low byte, the row,
// the count and the list pointer; the help line 0x48 with a count, 0x4E
// without; the hand at (0x803284 + 4, 0x803286 + 0x33). The count stepped by
// the repeat of the pressed 0xF000 bits (StepCount, sound 0x100). Then
// confirm: a count not 0 - sound 0x103, the answer 1, the step on; 0 - sound
// 0x107. Cancel: sound 0x106, the step back.
extern "C" void __cdecl ShopTrade_BuyCount(void) {
    auto* const win21 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 21 * sizeof(records::WindowRecord));
    auto* const w8 = rv::As<records::ItemListWindow>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 8 * sizeof(records::WindowRecord));
    auto* const w9 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 9 * sizeof(records::WindowRecord));
    const shop_states2::U y = static_cast<shop_states2::U>(w8->y);
    rv::Store<shop_states2::U>(w9->x, static_cast<shop_states2::U>(w8->x));
    const shop_states2::U rate = ByteShopStates2(::bof3::Img(at::kRate.v));
    rv::Store<shop_states2::U>(w9->y, y + 0x2D);
    PutByteShopStates2(::bof3::Img(0x8032AF), static_cast<std::uint32_t>(static_cast<unsigned char>(ShopStates2_Row)));
    PutLong(::bof3::Img(0x8032C4), ShopStates2_ShopList);
    PutByteShopStates2(::bof3::Img(0x8032AE), rate);
    const shop_states2::U count = ShopStates2_Count;
    PutByteShopStates2(::bof3::Img(0x8032B0), count);
    const shop_states2::U x = LongShopStates2(::bof3::Img(0x803284)) + 4;
    rv::Store<shop_states2::U>(w9->in_use, 1);
    rv::Store<shop_states2::U>(w9->handler, 7);
    Window0_Arrows = count != 0 ? 0x48 : 0x4E;
    const shop_states2::U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0xF000;
    rv::Store<shop_states2::U>(w9->kind, 8);
    PutByteShopStates2(::bof3::Img(0x80328A), 1);
    rv::Store<shop_states2::U>(win21->x, x);
    WindowRecords[21].y = y + 0x33;
    StepCount(shop_states2::g.auto_repeat(pressed), 0x100);
    const shop_states2::U c = ShopStates2_Count;
    const shop_states2::U p = Input_Pressed;
    if (Confirmed(p)) {
        if (c != 0) {
            shop_states2::g.sound(0x103);
            const shop_states2::U sub = FieldS_Step;
            FieldS_Answer = 1;
            FieldS_Step = sub + 1;
        } else {
            shop_states2::g.sound(0x107);
        }
        return;
    }
    if (Cancelled(p)) {
        shop_states2::g.sound(0x106);
        StepBy(-1);
    }
}

// original 0x582090 (PSX 0x801D1D58): "buy?" (help 0x49). Yes: sound 0x104;
// the money less the count's low byte times the price (32 bits, unchecked);
// Inventory_Add(category, item, count); then, when the list sells equipment,
// the category is not 0 and the item's icon kind (read after) is not 0xA or
// 0xB - the answer 1, the step on (to "equip it?"); otherwise the step back
// two (to the list). No, or cancel: sound 0x106, the step back two.
extern "C" void __cdecl ShopTrade_BuyConfirm(void) {
    const shop_states2::U p = YesNoFrame(0x49);
    if (Confirmed(p)) {
        if (static_cast<unsigned char>(FieldS_Answer) != 0) {
            shop_states2::g.sound(0x104);
            const shop_states2::U count = LongShopStates2(::bof3::Img(at::kCount.v));
            Party_Zenny = Party_Zenny - (count & 0xFF) * ShopStates2_UnitPrice;
            shop_states2::g.inv_add(static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item), count);
            const shop_states2::U icon = shop_states2::g.icon_kind(static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item)) & 0xFF;
            if (Rest2c_SellFlag != 0 && ShopStates2_Kind != 0 && icon != 0xA && icon != 0xB) {
                const shop_states2::U sub = FieldS_Step;
                FieldS_Answer = 1;
                FieldS_Step = sub + 1;
                return;
            }
            StepBy(-2);
            return;
        }
    } else if (!Cancelled(p)) {
        return;
    }
    shop_states2::g.sound(0x106);
    StepBy(-2);
}

// original 0x5821E0 (PSX 0x801D1F74): "equip it?" (help 0x4A). Yes: sound
// 0x103, the member 0, the step on. No, or cancel: sound 0x106, the step
// back three (to the list).
extern "C" void __cdecl ShopTrade_BuyAskEquip(void) {
    const shop_states2::U p = YesNoFrame(0x4A);
    if (Confirmed(p)) {
        if (static_cast<unsigned char>(FieldS_Answer) != 0) {
            shop_states2::g.sound(0x103);
            const shop_states2::U sub = FieldS_Step;
            BattleActorCopies_ItemActor = 0;
            FieldS_Step = sub + 1;
            return;
        }
    } else if (!Cancelled(p)) {
        return;
    }
    shop_states2::g.sound(0x106);
    StepBy(-3);
}

// original 0x5822C0 (PSX 0x801D20D8): on whom. The name; help 0x4B; the
// cursor hand off and the member hand on at the member's panel +4 / +6. The
// repeat of the pressed 0x5000 bits moves the member (Party_Count(0)'s low
// byte asked before): 0x1000 up, below 0 to the count less one; 0x4000
// down, past the count less one (signed bytes) to 0; a move plays 0x101.
// Confirm: Item_EquipMask(category, item)'s low byte against bit (record
// & 31) of the member's record - none: sound 0x107; else the panels' +3 3
// for the member and 1 for the others, the slot box 0x803238 set up (1 7 6
// 2, at (0xFF6A, 0x74), +0xA the member), sound 0x103, the member hand off,
// the slot cursor 0, the counter 5, the step on. Cancel: sound 0x106, the
// member hand off, the step back four (to the list).
//
// As the original has it: the panels are marked while the index, as a
// signed byte, is below Party_Count's low byte.
extern "C" void __cdecl ShopTrade_BuyMember(void) {
    auto* const w6 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 6 * sizeof(records::WindowRecord));
    auto* const w20 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 20 * sizeof(records::WindowRecord));
    SetItemName();
    const int member = BattleActorCopies_ItemActor;
    Window0_Arrows = 0x4B;
    WindowRecords[21].in_use = 0;
    rv::Store<shop_states2::U>(w20->in_use, 1);
    rv::Store<shop_states2::U>(w20->x, Word(::bof3::Img(0x8031D0) + static_cast<shop_states2::U>(member * 36)));
    rv::Store<shop_states2::U>(w20->y, Word(::bof3::Img(0x8031D2) + static_cast<shop_states2::U>(member * 36)));
    const shop_states2::U n = PartyCount();
    const shop_states2::U repeat = shop_states2::g.auto_repeat(static_cast<std::uint32_t>(Input_Pressed) & 0x5000);
    shop_states2::U cur = static_cast<unsigned char>(BattleActorCopies_ItemActor);
    const shop_states2::U start = cur;
    if ((repeat & 0x1000) != 0) {
        cur = (cur - 1) & 0xFF;
        BattleActorCopies_ItemActor = cur;
        if ((cur & 0x80) != 0) {
            cur = (n - 1) & 0xFF;
            PutByteShopStates2(::bof3::Img(at::kMember.v), cur);
        }
    } else if ((repeat & 0x4000) != 0) {
        cur = (cur + 1) & 0xFF;
        BattleActorCopies_ItemActor = cur;
        if (S8(cur) > S8(n) - 1) {
            cur = 0;
            BattleActorCopies_ItemActor = cur;
        }
    }
    if (start != cur) shop_states2::g.sound(0x101);
    const shop_states2::U p = Input_Pressed;
    if (Confirmed(p)) {
        const shop_states2::U mask = shop_states2::g.equip_mask(static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item));
        const shop_states2::U record = RecordOf(BattleActorCopies_ItemActor);
        if (((1u << (record & 31)) & mask & 0xFF) == 0) {
            shop_states2::g.sound(0x107);
            return;
        }
        int i = 0;
        if (PartyCount() != 0) {
            for (;;) {
                Panel(i)[3] = static_cast<unsigned char>((static_cast<shop_states2::U>(i) & 0xFF) == static_cast<unsigned char>(BattleActorCopies_ItemActor) ? 3 : 1);
                i = S8(static_cast<shop_states2::U>(i + 1));
                if (i >= static_cast<int>(PartyCount())) break;
            }
        }
        const shop_states2::U m = static_cast<unsigned char>(BattleActorCopies_ItemActor);
        rv::Store<shop_states2::U>(w6->in_use, 1);
        rv::Store<shop_states2::U>(w6->handler, 7);
        rv::Store<shop_states2::U>(w6->kind, 6);
        rv::Store<shop_states2::U>(w6->state, 2);
        rv::Store<shop_states2::U>(w6->x, 0xFF6A);
        rv::Store<shop_states2::U>(w6->y, 0x74);
        PutByteShopStates2(::bof3::Img(0x803242), m);
        shop_states2::g.sound(0x103);
        const shop_states2::U sub = FieldS_Step;
        rv::Store<shop_states2::U>(w20->in_use, 0);
        ShopStates2_SlotCursor = 0;
        PutByteShopStates2(::bof3::Img(at::kCounter.v), 5);
        FieldS_Step = sub + 1;
        return;
    }
    if (Cancelled(p)) {
        shop_states2::g.sound(0x106);
        const shop_states2::U sub = FieldS_Step;
        rv::Store<shop_states2::U>(w20->in_use, 0);
        FieldS_Step = sub - 4;
    }
}

// original 0x5824D0 (PSX 0x801D23EC): which slot. The item's icon kind (the
// slot base); the hand at (0x80323C + 6, 0x80323E + 13 * (slot cursor +
// kind + 1)). Kind 5 (two slots): help 0x4C, and the repeat of the pressed
// 0x5000 bits flips the slot cursor with sound 0x100; any other: the name,
// help 0x3A. Confirm: sound 0x104; Shop_Equip(the member's record, slot
// cursor + kind - 1, item); Char_RecalcStats of the member's record (both
// re-read); the step on. Cancel: sound 0x106, the panels' +3 4 for the
// member and 2 for the others, the slot box +3 1, the step back.
extern "C" void __cdecl ShopTrade_BuySlot(void) {
    auto* const win21 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 21 * sizeof(records::WindowRecord));
    auto* const w6 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 6 * sizeof(records::WindowRecord));
    const shop_states2::U kind = shop_states2::g.icon_kind(static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item)) & 0xFF;
    const shop_states2::U x = LongShopStates2(::bof3::Img(0x80323C)) + 6;
    const shop_states2::U slot = ShopStates2_SlotCursor;
    rv::Store<shop_states2::U>(win21->x, x);
    WindowRecords[21].in_use = 1;
    const shop_states2::U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0x5000;
    WindowRecords[21].y = (slot + kind + 1) * 13 + static_cast<shop_states2::U>(w6->y);
    const shop_states2::U repeat = shop_states2::g.auto_repeat(pressed);
    if (kind == 5) {
        Window0_Arrows = 0x4C;
        if ((repeat & 0x5000) != 0) {
            ShopStates2_SlotCursor = static_cast<std::uint32_t>(ShopStates2_SlotCursor) ^ 1;
            shop_states2::g.sound(0x100);
        }
    } else {
        SetItemName();
        Window0_Arrows = 0x3A;
    }
    const shop_states2::U p = Input_Pressed;
    if (Confirmed(p)) {
        shop_states2::g.sound(0x104);
        const shop_states2::U item = ShopStates2_Item;
        const shop_states2::U to = (static_cast<std::uint32_t>(ShopStates2_SlotCursor) + kind - 1) & 0xFF;
        shop_states2::g.equip(RecordOf(BattleActorCopies_ItemActor), to, item);
        shop_states2::g.recalc(AtShopStates2(::bof3::Img(CharacterRecords_addr) + RecordOf(BattleActorCopies_ItemActor) * at::kCharStride));
        StepBy(1);
        return;
    }
    if (Cancelled(p)) {
        shop_states2::g.sound(0x106);
        MarkPanels(4, 2);
        const shop_states2::U sub = FieldS_Step;
        rv::Store<shop_states2::U>(w6->state, 1);
        FieldS_Step = sub - 1;
    }
}

// original 0x5826A0 (PSX 0x801D26B0): after equipping, the new figures
// shown. The name; help 0x4D; the hand off. Any pressed bit: the panels' +3
// 4 for the member and 2 for the others, the slot box +3 1, and while one of
// the item is still held (Inventory_Count(category, item, 0)'s word) the
// step back two (another member), else back six (the list).
extern "C" void __cdecl ShopTrade_BuyEquipped(void) {
    auto* const w6 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 6 * sizeof(records::WindowRecord));
    SetItemName();
    const shop_states2::U pressed = Input_Pressed;
    Window0_Arrows = 0x4D;
    WindowRecords[21].in_use = 0;
    if (pressed == 0) return;
    MarkPanels(4, 2);
    const shop_states2::U kind = ShopStates2_Kind;
    const shop_states2::U item = ShopStates2_Item;
    rv::Store<shop_states2::U>(w6->state, 1);
    const shop_states2::U held = shop_states2::g.inv_count(kind, item, 0) & 0xFFFF;
    StepBy(held != 0 ? -2 : -6);
}

// ============================================================================
// State 3: sell
// ============================================================================

// original 0x582780: the sell list's window 0x80325C: at (0x140, 0x3E), the
// tab, top, row and +9 0, the two scroll words 0, +0..+3 1 7 9 2, +0xD 0xFF,
// +8 3 (Item_CanUse's mode); the step on.
extern "C" void __cdecl ShopTrade_SellSetup(void) {
    auto* const w7 = rv::As<records::ItemListWindow>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 7 * sizeof(records::WindowRecord));
    rv::Store<shop_states2::U>(w7->x, 0x140);
    rv::Store<shop_states2::U>(w7->category, 0);
    rv::Store<shop_states2::U>(w7->top, 0);
    rv::Store<shop_states2::U>(w7->marked, 0);
    rv::Store<shop_states2::U>(w7->greyed, 0);
    rv::Store<shop_states2::U>(w7->arrows, 0);
    rv::Store<shop_states2::U>(w7->scroll, 0);
    const shop_states2::U sub = FieldS_Step;
    rv::Store<shop_states2::U>(w7->y, 0x3E);
    rv::Store<shop_states2::U>(w7->in_use, 1);
    rv::Store<shop_states2::U>(w7->handler, 7);
    rv::Store<shop_states2::U>(w7->kind, 9);
    rv::Store<shop_states2::U>(w7->state, 2);
    rv::Store<shop_states2::U>(w7->cursor, 0xFF);
    rv::Store<shop_states2::U>(w7->use_mode, 3);
    FieldS_Step = sub + 1;
}

// original 0x5827F0: the sell list - four category tabs (0x803266, 0..3,
// the inventory's categories 0 consumables, 1 weapons, 2 armour, 3
// accessories), a top row (0x803267) and a row (0x803268) of the category's
// 128. The item under the cursor becomes the chosen one and its Item_HelpMessage
// word the help line; with the window up (0x80325F = 0) the hand goes to (x
// + 7, y + 13 * (row - top) + 0x1B). The repeat of the pressed 0xF00C bits:
// 0x8000 / 0x2000 the tab left / right with sound 0x101, wrapping, and the
// scroll word 0x80326C 0x32 / 0x31; then 0x1000 up (a row above the top
// starts the scroll 0xF0), 0x4000 down (at most 0x7F; nine below the top
// starts 0x10), 4 a page up (top and row less 9, to 0), 8 a page down (to
// the top 0x77, the row 0x7F); a row moved plays 0x100. Nothing more while
// the scroll word 0x80326E runs. Confirm: the item under the cursor chosen,
// the hand to it, sound 0x103, then Item_CanUse(mode 0x803264, 0, category,
// item): no - sound 0x107; yes - sound 0x103, the row to 0x6BC8A9, the most
// the stack's count, the count 1, the price Shop_SellPrice(category, item,
// 0x6BC8AA), the step on. Cancel: sound 0x106, the window +3 1, the counter
// 7, the step on three (to the close).
extern "C" void __cdecl ShopTrade_SellList(void) {
    auto* const win21 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 21 * sizeof(records::WindowRecord));
    auto* const w7 = rv::As<records::ItemListWindow>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 7 * sizeof(records::WindowRecord));
    auto* const w10 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 10 * sizeof(records::WindowRecord));
    const shop_states2::U tab = static_cast<shop_states2::U>(w7->category);
    const shop_states2::U row = static_cast<shop_states2::U>(w7->marked);
    const shop_states2::U item = Ids(tab)[row];
    ShopStates2_Kind = tab;
    ShopStates2_Item = item;
    Window0_Arrows = shop_states2::g.help_id(tab, item);
    const shop_states2::U closed = static_cast<shop_states2::U>(w7->state);
    rv::Store<shop_states2::U>(w7->greyed, 0);
    if (closed == 0) {
        const shop_states2::U top = static_cast<shop_states2::U>(w7->top);
        const shop_states2::U r = static_cast<shop_states2::U>(w7->marked);
        const shop_states2::U x = LongShopStates2(::bof3::Img(0x803260)) + 7;
        WindowRecords[21].in_use = 1;
        rv::Store<shop_states2::U>(win21->x, x);
        rv::Store<shop_states2::U>(w10->in_use, 0);
        WindowRecords[21].y = (r - top) * 13 + static_cast<shop_states2::U>(w7->y) + 0x1B;
    }
    const shop_states2::U repeat = shop_states2::g.auto_repeat(static_cast<std::uint32_t>(Input_Pressed) & Sigma(0xF00C));
    if ((repeat & 0x8000) != 0) {
        shop_states2::g.sound(0x101);
        // The tab read raw: typed, the arrows' store is scheduled after the
        // decrement (codegen_same).
        const shop_states2::U t = (ByteShopStates2(::bof3::Img(0x803266)) - 1) & 0xFF;
        rv::Store<shop_states2::U>(w7->arrows, 0x32);
        rv::Store<shop_states2::U>(w7->category, t);
        if ((t & 0x80) != 0) rv::Store<shop_states2::U>(w7->category, 3);
    } else if ((repeat & 0x2000) != 0) {
        shop_states2::g.sound(0x101);
        const shop_states2::U t = (static_cast<shop_states2::U>(w7->category) + 1) & 0xFF;
        rv::Store<shop_states2::U>(w7->arrows, 0x31);
        rv::Store<shop_states2::U>(w7->category, t);
        if (t > 3) rv::Store<shop_states2::U>(w7->category, 0);
    }
    shop_states2::U r = static_cast<shop_states2::U>(w7->marked);
    const shop_states2::U start = r;
    shop_states2::U top;
    if ((repeat & 0x1000) != 0) {
        if (r != 0) {
            r = (r - 1) & 0xFF;
            rv::Store<shop_states2::U>(w7->marked, r);
        }
        top = static_cast<shop_states2::U>(w7->top);
        if (r < top) rv::Store<shop_states2::U>(w7->scroll, 0xF0);
    } else if ((repeat & 0x4000) != 0) {
        if (r < 0x7F) {
            r = (r + 1) & 0xFF;
            rv::Store<shop_states2::U>(w7->marked, r);
        }
        top = static_cast<shop_states2::U>(w7->top);
        if (static_cast<shop_states2::U>(w7->marked) >= top + 9) rv::Store<shop_states2::U>(w7->scroll, 0x10);
    } else if ((repeat & Sigma(4)) != 0) {
        top = static_cast<shop_states2::U>(w7->top);
        if (top == 0) {
            r = 0;
            rv::Store<shop_states2::U>(w7->marked, r);
        } else if (top < 9) {
            r = (r - top) & 0xFF;
            top = 0;
            rv::Store<shop_states2::U>(w7->marked, r);
            rv::Store<shop_states2::U>(w7->top, top);
        } else {
            r = (r - 9) & 0xFF;
            top = (top - 9) & 0xFF;
            rv::Store<shop_states2::U>(w7->marked, r);
            rv::Store<shop_states2::U>(w7->top, top);
        }
    } else {
        top = static_cast<shop_states2::U>(w7->top);
        if ((repeat & Sigma(8)) != 0) {
            if (top == 0x77) {
                r = 0x7F;
                rv::Store<shop_states2::U>(w7->marked, r);
            } else if (top > 0x6E) {
                r = (r + (0x77 - top)) & 0xFF;
                top = 0x77;
                rv::Store<shop_states2::U>(w7->marked, r);
                rv::Store<shop_states2::U>(w7->top, top);
            } else {
                r = (r + 9) & 0xFF;
                top = (top + 9) & 0xFF;
                rv::Store<shop_states2::U>(w7->marked, r);
                rv::Store<shop_states2::U>(w7->top, top);
            }
        }
    }
    if (start != r) {
        shop_states2::g.sound(0x100);
        top = static_cast<shop_states2::U>(w7->top);
        r = static_cast<shop_states2::U>(w7->marked);
    }
    if (static_cast<shop_states2::U>(w7->scroll) != 0) return;
    const shop_states2::U p = Input_Pressed;
    if (Confirmed(p)) {
        const shop_states2::U t = static_cast<shop_states2::U>(w7->category);
        const shop_states2::U row_now = static_cast<shop_states2::U>(w7->marked);
        const shop_states2::U chosen = Ids(t)[row_now];
        ShopStates2_Kind = t;
        const shop_states2::U x = LongShopStates2(::bof3::Img(0x803260)) + 7;
        ShopStates2_Item = chosen;
        rv::Store<shop_states2::U>(win21->x, x);
        WindowRecords[21].y = (r - top) * 13 + static_cast<shop_states2::U>(w7->y) + 0x1B;
        shop_states2::g.sound(0x103);
        const shop_states2::U ok = shop_states2::g.can_use(static_cast<shop_states2::U>(w7->use_mode), 0, static_cast<std::uint32_t>(ShopStates2_Kind), static_cast<std::uint32_t>(ShopStates2_Item)) & 0xFF;
        if (ok == 0) {
            shop_states2::g.sound(0x107);
            return;
        }
        shop_states2::g.sound(0x103);
        const shop_states2::U kind = ShopStates2_Kind;
        const shop_states2::U sold_row = static_cast<shop_states2::U>(w7->marked);
        ShopStates2_Row = sold_row;
        const shop_states2::U have = Counts(kind)[S8(sold_row)];
        const shop_states2::U flag = ShopStates2_SellFlag;
        const shop_states2::U it = ShopStates2_Item;
        PutLong(::bof3::Img(at::kMaxCount.v), have);
        const shop_states2::U k = ShopStates2_Kind;
        ShopStates2_Count = 1;
        ShopStates2_UnitPrice = shop_states2::g.sell_price(k, it, flag);
        StepBy(1);
        return;
    }
    if (Cancelled(p)) {
        shop_states2::g.sound(0x106);
        const shop_states2::U sub = FieldS_Step;
        rv::Store<shop_states2::U>(w7->state, 1);
        PutByteShopStates2(::bof3::Img(at::kCounter.v), 7);
        FieldS_Step = sub + 3;
    }
}

// original 0x582B60: how many to sell. The count window 0x8032C8 at
// (0x803260, 0x803262 + 13 * (row - top) + 0x14) with the category, the
// item, the count and the flag byte; help 0x51. The count stepped by the
// repeat of the pressed 0xF000 bits (StepCount, sound 0x101). Confirm:
// sound 0x103, the answer 1, the step on. Cancel: sound 0x106, the step
// back.
extern "C" void __cdecl ShopTrade_SellCount(void) {
    auto* const w7 = rv::As<records::ItemListWindow>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 7 * sizeof(records::WindowRecord));
    auto* const w10 = rv::As<records::WindowRecord>(AtShopStates2(::bof3::Img(WindowRecords_addr)) + 10 * sizeof(records::WindowRecord));
    rv::Store<shop_states2::U>(w10->x, static_cast<shop_states2::U>(w7->x));
    const shop_states2::U top = static_cast<shop_states2::U>(w7->top);
    const int row = ShopStates2_Row;
    PutByteShopStates2(::bof3::Img(0x8032D2), static_cast<std::uint32_t>(ShopStates2_Kind));
    const shop_states2::U flag = ShopStates2_SellFlag;
    Window0_Arrows = 0x51;
    rv::Store<shop_states2::U>(w7->greyed, 1);
    rv::Store<shop_states2::U>(w10->in_use, 1);
    const shop_states2::U item = ShopStates2_Item;
    const shop_states2::U y = static_cast<shop_states2::U>((row - static_cast<int>(top)) * 13) + static_cast<shop_states2::U>(w7->y) + 0x14;
    PutByteShopStates2(::bof3::Img(0x8032D3), item);
    const shop_states2::U pressed = static_cast<std::uint32_t>(Input_Pressed) & 0xF000;
    rv::Store<shop_states2::U>(w10->handler, 7);
    rv::Store<shop_states2::U>(w10->y, y);
    const shop_states2::U count = ShopStates2_Count;
    rv::Store<shop_states2::U>(w10->kind, 0xA);
    PutByteShopStates2(::bof3::Img(0x8032D4), count);
    PutByteShopStates2(::bof3::Img(0x8032D5), flag);
    StepCount(shop_states2::g.auto_repeat(pressed), 0x101);
    const shop_states2::U p = Input_Pressed;
    if (Confirmed(p)) {
        shop_states2::g.sound(0x103);
        const shop_states2::U sub = FieldS_Step;
        FieldS_Answer = 1;
        FieldS_Step = sub + 1;
        return;
    }
    if (Cancelled(p)) {
        shop_states2::g.sound(0x106);
        StepBy(-1);
    }
}

// original 0x582D00: "sell?" (help 0x52). Yes: sound 0x104; the money plus
// the count's low byte times the price, then at most 9,999,999 (unsigned);
// the stack's count (row s8, category and row re-read for each access) less
// the count, and at 0 its id and count 0; the step back two. No, or cancel:
// sound 0x106, the step back two (to the list).
extern "C" void __cdecl ShopTrade_SellConfirm(void) {
    const shop_states2::U p = YesNoFrame(0x52);
    if (Confirmed(p)) {
        if (static_cast<unsigned char>(FieldS_Answer) != 0) {
            shop_states2::g.sound(0x104);
            const shop_states2::U count = LongShopStates2(::bof3::Img(at::kCount.v)) & 0xFF;
            const shop_states2::U money = Party_Zenny + count * ShopStates2_UnitPrice;
            Party_Zenny = money;
            if (money > at::kMoneyMax) Party_Zenny = at::kMoneyMax;
            unsigned char* const stack = Counts(static_cast<std::uint32_t>(ShopStates2_Kind)) + ShopStates2_Row;
            *stack = static_cast<unsigned char>(*stack - count);
            if (Counts(static_cast<std::uint32_t>(ShopStates2_Kind))[ShopStates2_Row] == 0) {
                Ids(static_cast<std::uint32_t>(ShopStates2_Kind))[ShopStates2_Row] = 0;
                Counts(static_cast<std::uint32_t>(ShopStates2_Kind))[ShopStates2_Row] = 0;
            }
            StepBy(-2);
            return;
        }
    } else if (!Cancelled(p)) {
        return;
    }
    shop_states2::g.sound(0x106);
    StepBy(-2);
}

// original 0x582E80: the cursor hand off; the counter down, and at 0 state
// 1 (buy or sell again) step 0.
extern "C" void __cdecl ShopTrade_SellClose(void) {
    const shop_states2::U c = (ByteShopStates2(::bof3::Img(at::kCounter.v)) - 1) & 0xFF;
    WindowRecords[21].in_use = 0;
    PutByteShopStates2(::bof3::Img(at::kCounter.v), c);
    if (c != 0) return;
    FieldS_State = 1;
    FieldS_Step = 0;
}

// ============================================================================
// State 4: close
// ============================================================================

// original 0x584F70 (PSX 0x801E0E08): the counter down, and at 0
// Window_ResetAll and the menu ended (0x929F00 = 1). Also entry 4 of
// 0x664264 (group DF's, or no group's: the menu kind 0x584120's states).
extern "C" void __cdecl ShopTrade_Close(void) {
    const U c = (ByteShopStates2(::bof3::Img(at::kCounter.v)) - 1) & 0xFF;
    PutByteShopStates2(::bof3::Img(at::kCounter.v), c);
    if (c != 0) return;
    g.window_reset();
    BattleE1_Mode = 1;
}

// ============================================================================
// Task 0's title loop
// ============================================================================

// original 0x588E70: task 0's entry for the title (Title_StateMenu creates
// it). Game_Mode and Game_Step 0; Task_ClearPrivate; Window_ResetAll; then
// for ever: Task_Sleep(1), the entry of TitleTask_Modes 0x667294 that
// Game_Mode (a word, read afresh) picks - 0 TitleMode_Load, 1 TitleFlow_Step
// - and Field_RunTaskRecords. The task leaves only through Task_Restart
// (TitleFlow_EnterGame). As the original has it, the mode is not checked:
// the table is two long and a 0 follows (ours aborts on a null entry).
extern "C" void __cdecl TitleTask_Run(void) {
    Game_Mode = 0;
    Game_Step = 0;
    shop_states2::g.task_clear();
    shop_states2::g.window_reset();
    for (;;) {
        shop_states2::g.task_sleep(1);
        Entry(::bof3::Img(TitleTask_Modes_addr), LongShopStates2(::bof3::Img(at::kGameMode.v)) & 0xFFFF, "TitleTask_Run")();
        shop_states2::g.run_task_records();
    }
}

// original 0x588EB0: task 0's mode 0 - LoadDatFile(0x31D), Task_Sleep(1)
// until File_LoadDone, Gfx_ClutStripRestore, then Game_Mode + 1 and the CLUT
// strip's dirty byte + 1 (read before).
extern "C" void __cdecl TitleMode_Load(void) {
    shop_states2::g.load_dat(static_cast<int>(at::kTitleFile));
    if (shop_states2::g.load_done() == 0) {
        do shop_states2::g.task_sleep(1);
        while (shop_states2::g.load_done() == 0);
    }
    shop_states2::g.clut_restore();
    const shop_states2::U dirty = Gfx_ClutStripDirty;
    Game_Mode = static_cast<std::uint32_t>(Game_Mode) + 1;
    Gfx_ClutStripDirty = dirty + 1;
}

#pragma clang attribute pop

void ShopStates2_Inject() {
    if (bof3::WantsShadow("shop_states2")) shop_states2::SelfTest();
    if (Lang_Latin()) {   // DIVERGENCE DIV-0027 (amended 2026-10-03), DIV-0056
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("ShopYesNoLayout", Low32(&g_shop_yes_no_hand),
                         &was, &is, 1);
        bof3::Log("DIV-0027    the shop's buy / equip / sell yes / no hand over the re-spaced line");
    }
    BOF3_INJECT(ShopTrade_Step);
    BOF3_INJECT(ShopTrade_OpenStep);
    BOF3_INJECT(ShopTrade_Open);
    BOF3_INJECT(ShopTrade_OpenWait);
    BOF3_INJECT(ShopTrade_ChoiceStep);
    BOF3_INJECT(ShopTrade_Choice);
    BOF3_INJECT(ShopTrade_BuyStep);
    BOF3_INJECT(ShopTrade_BuySetup);
    BOF3_INJECT(ShopTrade_BuyList);
    BOF3_INJECT(ShopTrade_BuyCount);
    BOF3_INJECT(ShopTrade_BuyConfirm);
    BOF3_INJECT(ShopTrade_BuyAskEquip);
    BOF3_INJECT(ShopTrade_BuyMember);
    BOF3_INJECT(ShopTrade_BuySlot);
    BOF3_INJECT(ShopTrade_BuyEquipped);
    BOF3_INJECT(ShopTrade_SellStep);
    BOF3_INJECT(ShopTrade_SellSetup);
    BOF3_INJECT(ShopTrade_SellList);
    BOF3_INJECT(ShopTrade_SellCount);
    BOF3_INJECT(ShopTrade_SellConfirm);
    BOF3_INJECT(ShopTrade_SellClose);
    BOF3_INJECT(ShopTrade_Close);
    BOF3_INJECT(TitleTask_Run);
    BOF3_INJECT(TitleMode_Load);
}
