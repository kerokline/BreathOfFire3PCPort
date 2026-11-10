// The shop overlay's first table - group DF of the eighth takeover round
// (docs/takeover-queue-round8.md): the PSX SHOP.EMI's code as the port
// compiled it into the exe, which pe_funcs.py folded into the recorded start
// 0x57F340. Each read to its last instruction with capstone against
// bof3/BOF3.exe, 2026-09-25 (docs/shop_states.md), entry and size:
//
//   ShopMode_Dispatch     0x57F500 0x11    InnNight_Dispatch     0x57FB60 0xE
//   ShopMode_Begin        0x57F520 0xA4    InnNight_Message      0x57FB70 0x65
//   ShopMode_End          0x57F5D0 0x75    InnNight_FadeOut      0x57FBE0 0x54
//   Inn_Dispatch          0x57F650 0xE     InnNight_Jingle       0x57FC40 0x23
//   Inn_Begin             0x57F660 0x78    InnNight_Wait         0x57FC70 0x39
//   InnPrompt_Dispatch    0x57F6E0 0xE     InnNight_Restore      0x57FCB0 0x69
//   InnPrompt_TitleIn     0x57F6F0 0x65    InnNight_AskSave      0x57FD20 0x5D
//   InnPrompt_Greeting    0x57F760 0xA6    InnNight_Leave        0x57FD80 0x4D
//   InnPrompt_ChoicesIn   0x57F810 0x84    FieldSave_Dispatch    0x57FDD0 0xE
//   InnPrompt_Choose      0x57F8A0 0x191   FieldSave_Begin       0x57FDE0 0x1F
//   InnPrompt_ChoicesOut  0x57FAB0 0xAA    FieldSave_SlotsIn     0x57FE00 0x63
//                                          FieldSave_Choose      0x57FE70 0x10C
//                                          FieldSave_SlotsOut    0x580150 0x4B
//                                          FieldSave_End         0x5801A0 0x89
//
// Game_Mode 7 (0x496290) step 1 (0x517300) ends in `jmp 0x57F500`, and so
// does 0x56D4D0: ShopMode_Dispatch runs one state of ShopMode_States
// (0x663E40) by the byte 0x929F00 a frame. ShopMode_Begin (state 0) sets that
// byte to the touched object's +0x18 plus 2 - state 2 is the shop (group DG's
// 0x5818B0), 3 the inn (Inn_Dispatch, here), 4 the way out (ShopMode_End,
// which is also state 1), 5..10 the overlay's other machines (Capcom's).
//
// Faithful but for DIV-0169 (the night's pause and resume of the area's song,
// sound_in.h; two calls, each after the original's last). Every call out
// goes through shop_states::g, so the start-up fuzz can stand recorders in
// for the callees. The four dispatchers
// jump, as the originals do ([[clang::musttail]]); every other function has
// disable_tail_calls. None of the five tables' indices is checked, in the
// original or here: an index past a table reads the next table's entries, and
// past the last the dwords after it - memory, which ours reads alike.
#include "game/shop_states.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/shop_states_callees.h"
#include "game/sound_in.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace shop_states {

namespace {
template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace shop_states

using namespace shop_states;

namespace {

unsigned char* AtShopStates(shop_states::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
unsigned char& BShopStates(shop_states::U address) { return *AtShopStates(address); }
void PutWordShopStates(shop_states::U address, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(AtShopStates(address), &w, sizeof w);
}
shop_states::U LongShopStates(shop_states::U address) {
    shop_states::U v;
    std::memcpy(&v, AtShopStates(address), sizeof v);
    return v;
}
void PutLong(shop_states::U address, shop_states::U v) { std::memcpy(AtShopStates(address), &v, sizeof v); }

using Handler = void (__cdecl*)();
Handler Entry(shop_states::U table, unsigned index) { return shop_states::Fn<Handler>(LongShopStates(table + index * 4)); }

}  // namespace

// The dispatchers jump, as the originals do: the state's function returns to
// the dispatcher's caller.

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ===========================================================================
// The mode's begin and end
// ===========================================================================

// ===========================================================================
// The inn: step 0
// ===========================================================================

// ===========================================================================
// The inn: step 1, the prompt (InnPrompt_States)
// ===========================================================================

// ===========================================================================
// The inn: step 2, the night (InnNight_States)
// ===========================================================================

// ===========================================================================
// The inn: step 3, the save menu (FieldSave_States)
// ===========================================================================

#pragma clang attribute pop

// ===========================================================================

void ShopStates_Inject() {
    if (bof3::WantsShadow("shop_states")) shop_states::SelfTest();
}
