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

using namespace shop_states2;

namespace {

unsigned char* AtShopStates2(shop_states2::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
shop_states2::U ByteShopStates2(shop_states2::U address) { return AtShopStates2(address)[0]; }
void PutByteShopStates2(shop_states2::U address, shop_states2::U v) { AtShopStates2(address)[0] = static_cast<unsigned char>(v); }
shop_states2::U LongShopStates2(shop_states2::U address) {
    shop_states2::U v;
    std::memcpy(&v, AtShopStates2(address), sizeof v);
    return v;
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

// Every function below keeps its calls as calls (docs/HANDOFF.md, Traps: the
// frame hash records return addresses).
#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ============================================================================
// State 0: open
// ============================================================================

// ============================================================================
// State 1: buy or sell
// ============================================================================

// ============================================================================
// State 2: buy
// ============================================================================

// ============================================================================
// State 3: sell
// ============================================================================

// ============================================================================
// State 4: close
// ============================================================================

// ============================================================================
// Task 0's title loop
// ============================================================================

#pragma clang attribute pop

void ShopStates2_Inject() {
    if (bof3::WantsShadow("shop_states2")) shop_states2::SelfTest();
}
