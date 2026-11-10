// Moved here by tools/move_funcs.py from shop_states.cpp (docs/source-layout-round.md).
#include "game/shop/shop_mode.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/shop_states_callees.h"
#include "game/sound_in.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace shop_states;

namespace shop_states {

namespace {

template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

}  // namespace

}  // namespace shop_states

namespace {

unsigned char* AtShopStates(shop_states::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

unsigned char& BShopStates(shop_states::U address) { return *AtShopStates(address); }

std::uint16_t Word(U address) {
    std::uint16_t v;
    std::memcpy(&v, AtShopStates(address), sizeof v);
    return v;
}

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

U Addr(const void* p) { return Low32(p); }

using Handler = void (__cdecl*)();

Handler Entry(shop_states::U table, unsigned index) { return shop_states::Fn<Handler>(LongShopStates(table + index * 4)); }

}  // namespace

// shop_states's callee seam, moved here from shop_states.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace shop_states {

const Callees kOriginals = {
    Menu_DrawTitleBox, Menu_DrawMoneyBox, Text_DrawAt, Msg_SystemPtr, Msg_OpenSystem,
    Sound_PlayEffect, Sound_LoadStream, Sound_StreamDone,
    SaveMenu_DrawSlots, Save_ReadSummaries, Input_AutoRepeat, Menu_YesNo,
    Transition_Start, Sprite_FaceDirection, Window_ResetAll,
    Crt_sprintf, TextRecord_Set, SaveMenu_DrawChoices, Menu_DrawBlackScreen, Party_RestoreAll,
};
Callees g = kOriginals;

}  // namespace shop_states

// original 0x57F500: `jmp [ShopMode_States + 4 * (0x929F00 & 0xFF)]` - the
// state byte read as the low byte of a dword. Jumped to by Game_Mode 7's step
// 1 (0x517323) and by 0x56D4D5, so the arguments and the return are Field_Task's.
extern "C" void __cdecl ShopMode_Dispatch(void) {
    [[clang::musttail]] return Entry(::bof3::Img(ShopMode_States_addr), BattleE1_Mode)();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x57F520, ShopMode_States 0. With no object touched (0x929F0C
// negative, s8): the state is 0x929EC2 + 2. Otherwise Sprite_Current is the
// object (Sprite_Objects + 0xA4 n), the state its +0x18 + 2 and the shop
// number 0x929EC3 its +0x1C. Then each of the eight character records' status
// word gets bit 0x2000 when a quarter of its max HP (+0x10 >> 2, u16) is above
// its HP (+8), and loses it otherwise; the step, the sub-state and 0x929F0A
// cleared, the counter 6, and 0x903844 = 0x658930 + 23 * the shop number.
extern "C" void __cdecl ShopMode_Begin(void) {
    const auto object = static_cast<signed char>(ModeStates_ShopObject);
    if (object < 0) {
        BattleE1_Mode = static_cast<unsigned char>(BShopStates(::bof3::Img(shop_states::at::kShopKind.v)) + 2);
    } else {
        unsigned char* const o = AtShopStates(::bof3::Img(Sprite_Objects_addr) + shop_states::at::kSpriteStride * static_cast<U>(static_cast<int>(object)));
        PutLong(::bof3::Img(shop_states::at::kSpriteCurrent.v), Addr(o));
        BattleE1_Mode = static_cast<unsigned char>(o[0x18] + 2);
        ScenaSc1_ShopByteB = o[0x1C];
    }
    for (U n = 0; n < shop_states::at::kActors; ++n) {
        const U r = ::bof3::Img(Field_ActorStates_addr) + n * shop_states::at::kActorStride;
        // r is character record n's +0x10, its status word (Field_ActorStates 0x903A80 = CharacterRecords + 0x10)
        const auto* const c = rv::AsConst<records::CharRecord>(AtShopStates(r - 0x10));
        const auto quarter = static_cast<std::uint16_t>(c->stats.max_hp >> 2);
        if (quarter > c->hp) BShopStates(r + 1) = static_cast<unsigned char>(BShopStates(r + 1) | 0x20);
        else PutWordShopStates(r, Word(r) & 0xDFFFu);
    }
    FieldS_State = 0;
    FieldS_Step = 0;
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 6;
    ShopStates_Clear0A = 0;
    ShopStates2_ShopList = ::bof3::Img(shop_states::at::kShopRecords.v) + shop_states::at::kShopRecordBytes * ScenaSc1_ShopByteB;
}

// original 0x57F5D0 (PSX 0x801D0FF8), ShopMode_States 1 and 4: the way out.
// With an object touched: Sprite_Current is it; unless its +7 has bit 3, its
// facing +8 goes back to +0x85, Sprite_FaceDirection(Sprite_Current's +8) and
// +0x80 |= 8 - on the object, not on Sprite_Current read again; then +0x80
// loses bit 5 and Field_ScriptFlags bit 8. Always: Window_ResetAll and
// Game_Step + 1.
extern "C" void __cdecl ShopMode_End(void) {
    const auto object = static_cast<signed char>(ModeStates_ShopObject);
    if (object >= 0) {
        unsigned char* const o = AtShopStates(::bof3::Img(Sprite_Objects_addr) + shop_states::at::kSpriteStride * static_cast<U>(static_cast<int>(object)));
        PutLong(::bof3::Img(shop_states::at::kSpriteCurrent.v), Addr(o));
        if ((o[7] & 8) == 0) {
            o[8] = o[0x85];
            g.face(Sprite_Current[8]);
            o[0x80] = static_cast<unsigned char>(o[0x80] | 8);
        }
        o[0x80] = static_cast<unsigned char>(o[0x80] & 0xDF);
        Field_ScriptFlags = Field_ScriptFlags & 0xFEFFu;
    }
    g.reset_windows();
    Game_Step = Game_Step + 1u;
}

#pragma clang attribute pop

void ShopMode_Inject() {
    BOF3_INJECT(ShopMode_Dispatch);
    BOF3_INJECT(ShopMode_Begin);
    BOF3_INJECT(ShopMode_End);
}
