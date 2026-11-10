// Moved here by tools/move_funcs.py from field_s.cpp, rest_2c.cpp, save_menu.cpp, shop_states.cpp (docs/source-layout-round.md).
#include "game/shop/field_save.h"

#include <cstdint>
#include <cstring>
#include <io.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/field_s.h"
#include "game/field_s_callees.h"
#include "game/lang_layout.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/music_seq.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2c.h"
#include "game/rest_2c_callees.h"
#include "game/save_menu.h"
#include "game/save_menu_callees.h"
#include "game/scenario_harness.h"
#include "game/shop/field_save_callees.h"
#include "game/shop_states.h"
#include "game/shop_states_callees.h"
#include "game/sound_in.h"
#include "game/title_menu.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/run_speed.h"
#include "input/sigma.h"

namespace {

namespace at = field_save::at;

}  // namespace

using namespace save_menu;

using namespace shop_states;

namespace {

using U = std::uint32_t;

unsigned char* At(U address) { return move_script::At(address); }

U L(U address) { return static_cast<U>(move_script::Long(At(address))); }

void Sound(U id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

const unsigned char* Message(U id) { return SH_CALL(Msg_SystemPtr)(id); }

constexpr auto& AtRest2c = move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

unsigned char& BRest2c(U address) { return *AtRest2c(address); }

U LRest2c(U address) { return static_cast<U>(Long(AtRest2c(address))); }

void PutL(U address, U v) { SetLong(AtRest2c(address), static_cast<std::int32_t>(v)); }

void PutW(U address, unsigned v) { SetWord(AtRest2c(address), v); }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (its reader's
// reach, docs/rest_2c.md section 3). Past it the original jumps through the
// next table's cell; no state writes such an index (section 5), so ours
// aborts. The entry is called as the cell holds it: Capcom's address in the
// game (Inject's jmp to ours where it is ours), a recorder while the fuzz runs.
using Handler = void (__cdecl*)();

void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(LRest2c(table + 4 * index)))();
}

// The screen title's box the inn and save states draw first: (0x14, 0x12),
// 0x118 by 0x13, in the window colour (the original pushes the byte in a
// register whose upper bytes it never cleared; Menu_DrawTitleBox reads the
// byte - shop_states.cpp's TitleBox).
void TitleBox(int y) { SH_CALL(Menu_DrawTitleBox)(0x14, y, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour)); }

// System message `id` at (0x1C, 0x16), colour 0, to its end.
void MessageRest2c(unsigned id) {
    const unsigned char* const text = SH_CALL(Msg_SystemPtr)(id);
    SH_CALL(Text_DrawAt)(0x1C, 0x16, 0, 0xFF, text);
}

// A slot's summary: 0x905BC0 + 0x1C * slot, for the slot the original takes
// unchecked from 0x9036D4. Every writer of that dword keeps it below 16
// (FieldSave_Choose, Save_QuickWrite, the load menu); past it ours aborts,
// where the original writes past the sixteen summaries.
U SummaryOf(U slot, const char* who) {
    if (slot >= rest_2c::at::kSlotCount)
        bof3::Fatal("%s: save slot %u is past the %u summaries; the original writes at 0x%X", who, static_cast<unsigned>(slot),
                    rest_2c::at::kSlotCount, static_cast<unsigned>((rest_2c::at::kSummaries + slot * rest_2c::at::kSummaryBytes).v));
    return ::bof3::Img(rest_2c::at::kSummaries.v) + slot * rest_2c::at::kSummaryBytes;
}

// The seven dwords Save_BuildBlock filled at 0x904680 into the slot's summary.
void CopySummary(U slot, const char* who) {
    const U to = SummaryOf(slot, who);
    for (U i = 0; i < 7; ++i) PutL(to + 4 * i, LRest2c(::bof3::Img(rest_2c::at::kSummary.v) + 4 * i));
}

// A party id's form: id 4 is drawn and saved as 0xB unless the story byte's
// low seven bits are 5 or 0xC (Save_BuildBlock's and MasterPanel_DrawFace's
// test, the same instructions).
unsigned char FormOf(unsigned char id) {
    if (id != 4) return id;
    const unsigned story = AreaW0b_PartySet & 0x7F;
    return story == 5 || story == 0xC ? id : static_cast<unsigned char>(0xB);
}

unsigned char* AtSaveMenu(save_menu::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

save_menu::U AddrSaveMenu(const volatile void* p) { return Low32(p); }

save_menu::U Byte(save_menu::U address) { return AtSaveMenu(address)[0]; }

void PutByte(save_menu::U address, save_menu::U v) { AtSaveMenu(address)[0] = static_cast<unsigned char>(v); }

save_menu::U WordSaveMenu(save_menu::U address) {
    std::uint16_t v;
    std::memcpy(&v, AtSaveMenu(address), sizeof v);
    return v;
}

void PutWord(save_menu::U address, save_menu::U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(AtSaveMenu(address), &w, sizeof w);
}

unsigned char* Packet() { return Gfx_PacketNext; }

}  // namespace

namespace shop_states {

namespace {

template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

}  // namespace

}  // namespace shop_states

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

Handler Entry(shop_states::U table, unsigned index) { return shop_states::Fn<Handler>(LongShopStates(table + index * 4)); }

// The screen title's box every inn and save state draws first: (0x14, 0x12),
// 0x118 by 0x13, in the window colour. The original pushes the colour byte in
// a register whose upper bytes it never cleared (entry eax, or edx after a
// call); Menu_DrawTitleBox uses it only through Gpu_GetClut's six-bit field
// (colour * 32 + 16 >> 4 & 0x3F), so ours zero-extends.
void TitleBoxShopStates() { shop_states::g.title_box(0x14, 0x12, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour)); }

// System message `id` at (0x1C, 0x16), colour 0, to its end.
void MessageShopStates(unsigned id) {
    const unsigned char* const text = shop_states::g.system_ptr(id);
    shop_states::g.draw_text(0x1C, 0x16, 0, 0xFF, text);
}

// The zenny box at (0x62, 0x28).
void Zenny() { shop_states::g.money_box(0x62, 0x28, 0, Party_Zenny); }

bool NoObject() { return ModeStates_ShopObject == 0xFF; }

}  // namespace

// original 0x57F650, state 3 (the inn): `jmp [Inn_Steps + 4 * 0x929F01]`.
extern "C" void __cdecl Inn_Dispatch(void) {
    [[clang::musttail]] return Entry(::bof3::Img(Inn_Steps_addr), FieldS_State)();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x57F660, Inn_Steps 0: the step on (to 1, the prompt), the inn's
// cursor 0, the counter 4, the sub-state 0. Field_InputFlags 0x40, or area
// 0xBC, 0x85 or 0xC1: the object kept in 0x929F0F, 0x929F0C = 0xFE and
// straight to step 3, the save menu. Otherwise an object of 0xFE goes there
// too.
extern "C" void __cdecl Inn_Begin(void) {
    const auto step = static_cast<unsigned char>(FieldS_State + 1);
    ShopStates_Choice = 0;
    FieldS_State = step;
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 4;
    FieldS_Step = 0;
    bool forced = Field_InputFlags == 0x40;
    if (!forced) {
        const std::uint16_t area = Game_AreaNumber;
        forced = area == 0xBC || area == 0x85 || area == 0xC1;
    }
    if (forced) {
        const unsigned char object = ModeStates_ShopObject;
        ModeStates_ShopObject = 0xFE;
        AreaW3c_Load0F = object;
        FieldS_State = 3;
        FieldS_Step = 0;
        return;
    }
    if (ModeStates_ShopObject == 0xFE) {
        FieldS_State = 3;
        FieldS_Step = 0;
    }
}

#pragma clang attribute pop

// original 0x57F6E0, the inn's step 1: `jmp [InnPrompt_States + 4 * 0x929F02]`.
extern "C" void __cdecl InnPrompt_Dispatch(void) {
    [[clang::musttail]] return Entry(::bof3::Img(InnPrompt_States_addr), FieldS_Step)();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x57F6F0, prompt 0: the title box slides down from 0x12 - 20 *
// counter (the counter read before the call); counter - 1, at 0 the next
// state, 0x929F0B = 1 and, with an object, Msg_OpenSystem(0xD1).
extern "C" void __cdecl InnPrompt_TitleIn(void) {
    const unsigned char colour = static_cast<unsigned char>(Config_WindowColour);
    const int y = 0x12 - 20 * static_cast<int>(BShopStates(::bof3::Img(shop_states::at::kCount.v)));
    shop_states::g.title_box(0x14, y, 0x118, 0x13, colour);
    const auto count = static_cast<unsigned char>(BShopStates(::bof3::Img(shop_states::at::kCount.v)) - 1);
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = count;
    if (count != 0) return;
    const auto sub = static_cast<unsigned char>(FieldS_Step + 1);
    const unsigned char object = ModeStates_ShopObject;
    FieldS_Answer = 1;
    FieldS_Step = sub;
    if (object != 0xFF) shop_states::g.open_system(0xD1);
}

// original 0x57F760 (PSX 0x801D3BA8), prompt 1: the title box; with an
// object, the price - "%d" of 10 * the shop number into 0x904BA0, made text
// record 0 of 8 (TextRecord_Set) - system message 0xA1 and the zenny box.
// When the system message is done (bit 1 of 0x7DEE44): counter 6, the next
// state, Sound_PlayEffect(0x102).
extern "C" void __cdecl InnPrompt_Greeting(void) {
    TitleBoxShopStates();
    if (!NoObject()) {
        shop_states::g.sprintf_(reinterpret_cast<char*>(AtShopStates(::bof3::Img(shop_states::at::kText.v))), reinterpret_cast<const char*>(AtShopStates(::bof3::Img(shop_states::at::kFormatD.v))),
                   static_cast<int>(ScenaSc1_ShopByteB * 10u));
        shop_states::g.text_record(0, 8, AtShopStates(::bof3::Img(shop_states::at::kText.v)));
        MessageShopStates(0xA1);
        Zenny();
    }
    if ((BShopStates(::bof3::Img(shop_states::at::kMsgFlags.v)) & 2) == 0) return;
    const auto sub = static_cast<unsigned char>(FieldS_Step + 1);
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 6;
    FieldS_Step = sub;
    shop_states::g.play(0x102);
}

// original 0x57F810, prompt 2: the three choices slide in from 0x6E - 48 *
// counter (y 0x4C); the title box; with an object, message 0xA1 and the zenny
// box; counter - 1, at 0 the next state.
extern "C" void __cdecl InnPrompt_ChoicesIn(void) {
    shop_states::g.choices(0x6E - 48 * static_cast<int>(BShopStates(::bof3::Img(shop_states::at::kCount.v))), 0x4C);
    TitleBoxShopStates();
    if (!NoObject()) {
        MessageShopStates(0xA1);
        Zenny();
    }
    const auto count = static_cast<unsigned char>(BShopStates(::bof3::Img(shop_states::at::kCount.v)) - 1);
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = count;
    if (count == 0) FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x57F8A0 (PSX 0x801D3D94), prompt 3, choosing. The same draw with
// the choices at 0x6E. Input_Pressed, read as a dword: up (0x1000) moves the
// cursor 0x6BC881 back, wrapping to 2 when it goes negative (s8); otherwise
// down (0x4000) on, to 0 past 2; a moved cursor Sound_PlayEffect(0x101), then
// the cursor and the pad read again. A confirm button (Field_ConfirmButtons &
// the pad's low word): the counter and 0x6BC882 cleared; cursor 0 (stay) with
// the zenny at least 10 * the shop number (unsigned): 0x104, the zenny less the
// price (both read again after the sound), sub-state + 2; short of it 0x107
// and + 1 (0x57FA40, "not enough", Capcom's); cursor 1 or 2: 0x104 and + 2.
// Else a cancel button: 0x106, + 2, the counter 0, 0x6BC882 = 1.
//
// As the original has it: before the counter store it computes (0x929F0C ==
// 0) and compares that with -1 (`sete dl; cmp edx, -1; je`) - never equal,
// so the branch it guards is dead and the read changes nothing.
extern "C" void __cdecl InnPrompt_Choose(void) {
    shop_states::g.choices(0x6E, 0x4C);
    TitleBoxShopStates();
    if (!NoObject()) {
        MessageShopStates(0xA1);
        Zenny();
    }
    shop_states::U pad = LongShopStates(::bof3::Img(shop_states::at::kPressed.v));
    unsigned char cursor = ShopStates_Choice;
    const unsigned char was = cursor;
    bool moved = false;
    if (pad & 0x1000) {
        cursor = static_cast<unsigned char>(cursor - 1);
        ShopStates_Choice = cursor;
        if (static_cast<signed char>(cursor) < 0) {
            cursor = 2;
            ShopStates_Choice = cursor;
        }
        moved = true;
    } else if (pad & 0x4000) {
        cursor = static_cast<unsigned char>(cursor + 1);
        ShopStates_Choice = cursor;
        if (cursor > 2) {
            cursor = 0;
            ShopStates_Choice = cursor;
        }
        moved = true;
    }
    if (moved && was != cursor) {
        shop_states::g.play(0x101);
        cursor = ShopStates_Choice;
        pad = LongShopStates(::bof3::Img(shop_states::at::kPressed.v));
    }
    if (Field_ConfirmButtons & pad) {
        BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 0;
        ShopStates_Cancelled = 0;
        if (cursor != 0) {
            shop_states::g.play(0x104);
            FieldS_Step = static_cast<unsigned char>(FieldS_Step + 2);
            return;
        }
        if (Party_Zenny >= ScenaSc1_ShopByteB * 10u) {
            shop_states::g.play(0x104);
            Party_Zenny = Party_Zenny - ScenaSc1_ShopByteB * 10u;
            FieldS_Step = static_cast<unsigned char>(FieldS_Step + 2);
            return;
        }
        shop_states::g.play(0x107);
        FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
        return;
    }
    if (Field_CancelButtons & pad) {
        shop_states::g.play(0x106);
        const auto sub = static_cast<unsigned char>(FieldS_Step + 2);
        BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 0;
        FieldS_Step = sub;
        ShopStates_Cancelled = 1;
    }
}

// original 0x57FA40, InnPrompt_States[4] ("not enough": InnPrompt_Choose's
// sub-state + 1 when the zenny is short): the choices at (0x6E, 0x4C), the
// title box, with an object the zenny box, system message 0xA2; any button
// pressed (Input_Pressed's low word) takes the sub-state back one.
extern "C" void __cdecl InnPrompt_NotEnough(void) {
    SH_CALL(SaveMenu_DrawChoices)(0x6E, 0x4C);
    TitleBox(0x12);
    if (ModeStates_ShopObject != 0xFF) SH_CALL(Menu_DrawMoneyBox)(0x62, 0x28, 0, Party_Zenny);
    MessageRest2c(0xA2);
    if (Input_Pressed != 0) --FieldS_Step;
}

// original 0x57FAB0, prompt 5: the choices slide out to 0x6E + 48 * counter;
// the title box; with an object, the zenny box (no message). Counter + 1; at
// 6: cancelled (0x6BC882) - the counter 0 and step + 3, the way out; else the
// sub-state 0, the counter 0x2D for cursor 0 (the night's message) or 0, and
// step + cursor + 1: 0 the night, 1 the save menu, 2 the way out.
extern "C" void __cdecl InnPrompt_ChoicesOut(void) {
    shop_states::g.choices(0x6E + 48 * static_cast<int>(BShopStates(::bof3::Img(shop_states::at::kCount.v))), 0x4C);
    TitleBoxShopStates();
    if (!NoObject()) Zenny();
    const auto count = static_cast<unsigned char>(BShopStates(::bof3::Img(shop_states::at::kCount.v)) + 1);
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = count;
    if (count != 6) return;
    if (ShopStates_Cancelled != 0) {
        const unsigned char step = FieldS_State;
        BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 0;
        FieldS_State = static_cast<unsigned char>(step + 3);
        return;
    }
    const unsigned char cursor = ShopStates_Choice;
    FieldS_Step = 0;
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = cursor != 0 ? 0 : 0x2D;
    FieldS_State = static_cast<unsigned char>(FieldS_State + cursor + 1);
}

#pragma clang attribute pop

// original 0x57FB60, the inn's step 2: `jmp [InnNight_States + 4 * 0x929F02]`.
extern "C" void __cdecl InnNight_Dispatch(void) {
    [[clang::musttail]] return Entry(::bof3::Img(InnNight_States_addr), FieldS_Step)();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x57FB70 (PSX: the state holding US 0x801D41EC), night 0: the title box and,
// with an object, message 0xA4; counter - 1, at 0 the next state and
// Transition_Start(0), the fade out.
//
// DIVERGENCE DIV-0169: then, with the switch, the area's song halted where it
// is - the PlayStation's SsSepPause of each SEP slot playing, after its
// Transition_Start(0) on the same frame (sound_in.h).
extern "C" void __cdecl InnNight_Message(void) {
    TitleBoxShopStates();
    if (!NoObject()) MessageShopStates(0xA4);
    const auto count = static_cast<unsigned char>(BShopStates(::bof3::Img(shop_states::at::kCount.v)) - 1);
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = count;
    if (count != 0) return;
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
    shop_states::g.transition(0);
    sound_in::NightFalls();  // DIV-0169
}

// original 0x57FBE0, night 1: once the transition is over (0x66C810 zero)
// Menu_DrawBlackScreen and the next state; until then, with an object, the
// title box and message 0xA4.
extern "C" void __cdecl InnNight_FadeOut(void) {
    if (Transition_Busy == 0) {
        shop_states::g.black();
        FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
        return;
    }
    if (NoObject()) return;
    TitleBoxShopStates();
    MessageShopStates(0xA4);
}

// original 0x57FC40, night 2: the black screen, Sound_LoadStream(0) - the
// inn's tune - the next state, the counter 0x96.
extern "C" void __cdecl InnNight_Jingle(void) {
    shop_states::g.black();
    shop_states::g.stream(0);
    const auto sub = static_cast<unsigned char>(FieldS_Step + 1);
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 0x96;
    FieldS_Step = sub;
}

// original 0x57FC70, night 3: the black screen; the counter down to 0 (not
// below); at 0, when Sound_StreamDone answers non-zero, the next state and
// Transition_Start(1), the fade in.
extern "C" void __cdecl InnNight_Wait(void) {
    shop_states::g.black();
    unsigned char count = BShopStates(::bof3::Img(shop_states::at::kCount.v));
    if (count != 0) {
        --count;
        BShopStates(::bof3::Img(shop_states::at::kCount.v)) = count;
        if (count != 0) return;
    }
    if (shop_states::g.stream_done() == 0) return;
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
    shop_states::g.transition(1);
}

// original 0x57FCB0 (PSX JP 0x801D447C, US 0x801D446C), night 4: the title
// box; once the transition is over, Party_RestoreAll(1), the next state,
// 0x929F0B = 1, Input_AutoRepeat's latch and timer cleared and, with an
// object, Msg_OpenSystem(0xD5).
//
// DIVERGENCE DIV-0169: then, with the switch, the song halted at nightfall
// started again at its position - the PlayStation's SsSepReplay(0, i) for
// each slot recorded as playing, last in this state (US 0x801D446C's tail,
// after the message), which the PC's port left out (sound_in.h).
extern "C" void __cdecl InnNight_Restore(void) {
    TitleBoxShopStates();
    if (Transition_Busy != 0) return;
    shop_states::g.restore(1);
    const auto sub = static_cast<unsigned char>(FieldS_Step + 1);
    const unsigned char object = ModeStates_ShopObject;
    FieldS_Answer = 1;
    FieldS_Step = sub;
    PutWordShopStates(::bof3::Img(shop_states::at::kRepeatLatch.v), 0);
    ItemUse_RepeatTimer = 0;
    if (object != 0xFF) shop_states::g.open_system(0xD5);
    sound_in::MorningComes();  // DIV-0169
}

// original 0x57FD20, night 5: the title box. With an object, the next state
// once the system message is done (bit 1 of 0x7DEE44). Without one, system
// message 0x1D (save?) and Menu_YesNo: an answer, the next state.
extern "C" void __cdecl InnNight_AskSave(void) {
    TitleBoxShopStates();
    if (!NoObject()) {
        if (BShopStates(::bof3::Img(shop_states::at::kMsgFlags.v)) & 2) FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
        return;
    }
    MessageShopStates(0x1D);
    if (shop_states::g.yes_no() != 0) FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x57FD80, night 6: the title box; the counter and the sub-state
// 0; step + 1 (the save menu) on a yes (0x929F0B), + 2 (the way out) on a no.
extern "C" void __cdecl InnNight_Leave(void) {
    TitleBoxShopStates();
    const unsigned char answer = static_cast<unsigned char>(FieldS_Answer);
    const unsigned char step = FieldS_State;
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 0;
    FieldS_Step = 0;
    FieldS_State = static_cast<unsigned char>(answer == 0 ? step + 2 : step + 1);
}

#pragma clang attribute pop

// original 0x57FDD0, the inn's step 3 (the save point's only one):
// `jmp [FieldSave_States + 4 * 0x929F02]`.
extern "C" void __cdecl FieldSave_Dispatch(void) {
    [[clang::musttail]] return Entry(::bof3::Img(FieldSave_States_addr), FieldS_Step)();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x57FDE0, save 0: Save_ReadSummaries (DIV-0002's refresh lives in
// Save_WriteFile, not here), the counter 4, the cursor and the first slot
// shown 0, the next state.
extern "C" void __cdecl FieldSave_Begin(void) {
    shop_states::g.read_summaries();
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 4;
    Rest2c_Slot = 0;
    PutLong(::bof3::Img(shop_states::at::kSlotTop.v), 0);
    FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x57FE00, save 1: Sound_PlayEffect(0x102) on the first frame
// (counter 4); the title box; the slots, lit, sliding in from 0x20 - 80 *
// counter (read after the box; y 0x30); counter - 1, at 0 the next state.
//
// The original builds x from `movzx ax` over Menu_DrawTitleBox's eax, so its
// upper 16 bits are what that call left; ours are the true product's. Every
// consumer of the x (SaveMenu_DrawSlots' two draws, Menu_DrawCursorBox) takes
// it as a u16, where the two agree.
extern "C" void __cdecl FieldSave_SlotsIn(void) {
    if (BShopStates(::bof3::Img(shop_states::at::kCount.v)) == 4) shop_states::g.play(0x102);
    TitleBoxShopStates();
    shop_states::g.slots(0x20 - 80 * static_cast<int>(BShopStates(::bof3::Img(shop_states::at::kCount.v))), 0x30, 1);
    const auto count = static_cast<unsigned char>(BShopStates(::bof3::Img(shop_states::at::kCount.v)) - 1);
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = count;
    if (count == 0) FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x57FE70, save 2, choosing a slot: the title box, message 0x9D,
// the slots lit at (0x20, 0x30); Input_AutoRepeat of Input_Pressed & 0x5000.
// Up (0x1000) with the slot non-zero: slot - 1, and the first shown - 1 when
// the slot drops below it (signed). Otherwise down (0x4000) with the slot
// below 15 (signed): slot + 1, and the first shown + 1 (read again) when the
// slot passes first + 2. A slot unequal to the old one's low byte (the whole
// dword against the byte): Sound_PlayEffect(0x101). Then Input_Pressed read
// again: a confirm button, 0x104, the next state and 0x929F0B = 0; else a
// cancel button, 0x106 and state 6 (FieldSave_SlotsOut).
extern "C" void __cdecl FieldSave_Choose(void) {
    TitleBoxShopStates();
    MessageShopStates(0x9D);
    shop_states::g.slots(0x20, 0x30, 1);
    const shop_states::U repeat = shop_states::g.auto_repeat(Input_Pressed & 0x5000u);
    shop_states::U slot = LongShopStates(::bof3::Img(shop_states::at::kSlot.v));
    const shop_states::U old = slot & 0xFF;
    if ((repeat & 0x1000) && slot != 0) {
        const shop_states::U top = LongShopStates(::bof3::Img(shop_states::at::kSlotTop.v));
        --slot;
        Rest2c_Slot = slot;
        if (static_cast<int>(slot) < static_cast<int>(top)) PutLong(::bof3::Img(shop_states::at::kSlotTop.v), top - 1);
    } else if ((repeat & 0x4000) && static_cast<int>(slot) < 15) {
        const shop_states::U limit = LongShopStates(::bof3::Img(shop_states::at::kSlotTop.v)) + 2;
        ++slot;
        Rest2c_Slot = slot;
        if (static_cast<int>(slot) > static_cast<int>(limit)) PutLong(::bof3::Img(shop_states::at::kSlotTop.v), LongShopStates(::bof3::Img(shop_states::at::kSlotTop.v)) + 1);
    }
    if (old != slot) shop_states::g.play(0x101);
    const std::uint16_t pad = Input_Pressed;
    if (Field_ConfirmButtons & pad) {
        shop_states::g.play(0x104);
        const auto sub = static_cast<unsigned char>(FieldS_Step + 1);
        FieldS_Answer = 0;
        FieldS_Step = sub;
        return;
    }
    if (Field_CancelButtons & pad) {
        shop_states::g.play(0x106);
        FieldS_Step = 6;
    }
}

#pragma clang attribute pop

// original 0x57FF80 (PSX 0x801D50D0): FieldSave_States[3] (docs/shop_states.md section 1). The
// title box (0x14, 0x12, 0x118, 0x13, style), message 0x9F at (0x1C, 0x16), the
// slots at (0x20, 0x30) unlit, then Menu_YesNo: on an answer, with 0x929F0B
// (yes) sound 0x104 and the step up one (state 4, the write), else sound 0x106
// and the step down one (back to the slots). The style goes out in eax over
// the entry's upper bytes; Menu_DrawTitleBox reads its byte.
extern "C" void __cdecl FieldSave_Confirm(void) {
    SH_CALL(Menu_DrawTitleBox)(0x14, 0x12, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(Text_DrawAt)(0x1C, 0x16, 0, 0xFF, Message(0x9F));
    SH_CALL(SaveMenu_DrawSlots)(0x20, 0x30, 0);
    if ((SH_CALL(Menu_YesNo)() & 0xFF) == 0) return;
    if (static_cast<unsigned char>(FieldS_Answer) != 0) {
        Sound(0x104);
        FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
        return;
    }
    Sound(0x106);
    FieldS_Step = static_cast<unsigned char>(FieldS_Step - 1);
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x580010, FieldSave_States[4] (FieldSave_Confirm's yes): the title
// box, system message 0x99, the slots at (0x20, 0x30) unlit; the block built
// (Save_BuildBlock), the file's name made from the slot (0x9036D4) and the
// file written, 0x12B0 bytes. Unless Save_WriteFile answered -1 (DIV-0003's
// failure path: a bare return): the sub-state up, the timer 0x1E, the slot's
// summary (the slot read again) and 0x6BC881 = 2.
extern "C" void __cdecl FieldSave_Write(void) {
    TitleBox(0x12);
    MessageRest2c(0x99);
    SH_CALL(SaveMenu_DrawSlots)(0x20, 0x30, 0);
    SH_CALL(Save_BuildBlock)();
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(AtRest2c(::bof3::Img(rest_2c::at::kPath.v))), reinterpret_cast<const char*>(AtRest2c(::bof3::Img(rest_2c::at::kPathFormat.v))), Rest2c_Slot);
    const int written = SH_CALL(Save_WriteFile)(reinterpret_cast<const char*>(AtRest2c(::bof3::Img(rest_2c::at::kPath.v))), rest_2c::at::kSaveSize);
    if (written == -1) return;
    const U slot = Rest2c_Slot;
    const auto step = static_cast<unsigned char>(FieldS_Step + 1);
    BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = 0x1E;
    FieldS_Step = step;
    CopySummary(slot, "FieldSave_Write");
    ShopStates_Choice = 2;
}

// original 0x5800D0, FieldSave_States[5]: the title box, system message 0x8F,
// the slots unlit; 0x6BC880 = 1 (FieldSave_End reads it). With no button
// pressed the timer runs down and nothing more until it reaches 0; then (or
// at once on a button) the timer 0, Sound_PlayEffect(0x104) and the sub-state
// up (read after the sound).
extern "C" void __cdecl FieldSave_Written(void) {
    TitleBox(0x12);
    MessageRest2c(0x8F);
    SH_CALL(SaveMenu_DrawSlots)(0x20, 0x30, 0);
    Rest2c_SaveBack = 1;
    if (Input_Pressed == 0) {
        const auto timer = static_cast<unsigned char>(BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) - 1);
        BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = timer;
        if (timer != 0) return;
    }
    BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = 0;
    SH_CALL(Sound_PlayEffect)(0x104);
    ++FieldS_Step;
}

// original 0x580150, save 6: the title box; the slots, unlit, sliding out to
// 0x20 + 80 * counter (the same leftover upper half as FieldSave_SlotsIn's);
// counter + 1, at 4 the next state.
extern "C" void __cdecl FieldSave_SlotsOut(void) {
    TitleBoxShopStates();
    shop_states::g.slots(0x20 + 80 * static_cast<int>(BShopStates(::bof3::Img(shop_states::at::kCount.v))), 0x30, 0);
    const auto count = static_cast<unsigned char>(BShopStates(::bof3::Img(shop_states::at::kCount.v)) + 1);
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = count;
    if (count == 4) FieldS_Step = static_cast<unsigned char>(FieldS_Step + 1);
}

// original 0x5801A0, save 7: the title box; the sub-state 0. An inn (0x929F0C
// not 0xFE) whose cursor is not 2 goes back to the prompt: step 1, sub-state
// 1 (InnPrompt_Greeting). Otherwise, with 0x66C7DA set and 0x6BC880 clear:
// Input_AutoRepeat's latch and timer and 0x929F0B cleared, state 8 (0x580230,
// Capcom's) and Msg_OpenSystem(0xD0); else the counter 0 and step + 1, the
// way out.
extern "C" void __cdecl FieldSave_End(void) {
    TitleBoxShopStates();
    const unsigned char cursor = ShopStates_Choice;
    FieldS_Step = 0;
    if (cursor != 2 && ModeStates_ShopObject != 0xFE) {
        FieldS_State = 1;
        FieldS_Step = 1;
        return;
    }
    if (ShopStates_InnFlag != 0 && Rest2c_SaveBack == 0) {
        PutWordShopStates(::bof3::Img(shop_states::at::kRepeatLatch.v), 0);
        ItemUse_RepeatTimer = 0;
        FieldS_Answer = 0;
        FieldS_Step = 8;
        shop_states::g.open_system(0xD0);
        return;
    }
    BShopStates(::bof3::Img(shop_states::at::kCount.v)) = 0;
    FieldS_State = static_cast<unsigned char>(FieldS_State + 1);
}

// original 0x580230, FieldSave_States[8] (FieldSave_End's question, system
// message 0xD0): the title box; once the message is done (0x7DEE44 bit 1):
// the answer 0x929F0B 0 - the timer 0 and the step up (the way out); else
// the step back to 3 (the save menu) and the sub-state 0.
extern "C" void __cdecl FieldSave_PromptAnswer(void) {
    TitleBox(0x12);
    if ((BRest2c(::bof3::Img(rest_2c::at::kMsgFlags.v)) & 2) == 0) return;
    if (static_cast<unsigned char>(FieldS_Answer) == 0) {
        BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = 0;
        ++FieldS_State;
        return;
    }
    FieldS_State = 3;
    FieldS_Step = 0;
}

// original 0x580280, Inn_Steps[4] (the way out): the title box at 0x12 - 20 *
// timer (the timer read before the draw; the original's upper half of the
// register is the entry's, and only the low 16 bits reach the callee); the
// timer up, at 5: mode 1 (ShopMode_End), the state 0, and with
// Field_InputFlags 0x40 or area 0xBC, 0x85 or 0xC1 (Inn_Begin's test) the
// object 0x929F0C back from 0x929F0F.
extern "C" void __cdecl Inn_TitleOut(void) {
    const int y = 0x12 - 20 * static_cast<int>(BRest2c(::bof3::Img(rest_2c::at::kTimer.v)));
    TitleBox(y);
    const auto timer = static_cast<unsigned char>(BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) + 1);
    BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = timer;
    if (timer != 5) return;
    BattleE1_Mode = 1;
    FieldS_State = 0;
    const unsigned area = Game_AreaNumber;
    if (Field_InputFlags == 0x40 || area == 0xBC || area == 0x85 || area == 0xC1) ModeStates_ShopObject = AreaW3c_Load0F;
}

// original 0x580300: `jmp [Rest_States + 4 * 0x929F01]`.
extern "C" void __cdecl Rest_Dispatch(void) { Jump(::bof3::Img(Rest_States_addr), 7, FieldS_State, "Rest_Dispatch"); }

#pragma clang attribute pop

// original 0x580310: Rest_States[0]. With story flag 0x77 set: state 6 and
// script message 0xEF (the state stored before the call). Else the flag set -
// unless the chapter byte is 0xE and the chapter row's flag 5 is set - then
// Transition_Start(0) and the state up one.
extern "C" void __cdecl Rest_Begin(void) {
    if ((SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x77) & 0xFF) != 0) {
        FieldS_State = 6;
        SH_CALL(Msg_OpenScript)(0xEF);
        return;
    }
    if (static_cast<unsigned char>(Cond_ByteFA) != 0xE ||
        (SH_CALL(Flags_Test)(reinterpret_cast<const unsigned char*>(Chapter_FlagRow), 5) & 0xFF) == 0)
        SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x77);
    SH_CALL(Transition_Start)(0);
    FieldS_State = static_cast<unsigned char>(FieldS_State + 1);
}

// original 0x580380: Rest_States[1]. Nothing while the transition runs (the
// wait word). Then the black screen; areas 0xBB and 0xC1 - and 0xBF in
// chapter 0xE with the row's flag 5 - only step the state on. Else
// Music_Track 0xFF; a place kind 0, 1 for area 0x85 (story flag 0x77 cleared,
// the area read again) or 2 for 0xBF; Field_ScriptFlags2's low three bits
// cleared; the members loaded (Field_MemberCount = Party_Count(0),
// Field_PartyLoad(0)); for each member i below Field_MemberCount (read again
// after each), Sprite_Current its ObjTrio record, Field_MemberSprite(its +0x89,
// i), Sprite_SetAnimation of its +8, +0x34 / +0x38 / +0x3C from the kind's
// row i of 0x663FD8 and its state bytes +1..+4 0 (each through Sprite_Current
// read again); the kind-2 sprite at the leader's +0x34 / +0x38,
// Field_ViewReset, Area_RunPlacement of the area descriptor's first dword,
// and the state up one.
//
// As the original has it: the place row is 16 x (3 kind + i) past 0x663FD8,
// unbounded by the member count; the area indexes Area_Descriptors (200) as a
// word, unchecked.
extern "C" void __cdecl Rest_PlaceParty(void) {
    if (Transition_Busy != 0) return;
    SH_CALL(Menu_DrawBlackScreen)();
    U area = Game_AreaNumber;
    if (area == 0xBB || area == 0xC1) {
        FieldS_State = static_cast<unsigned char>(FieldS_State + 1);
        return;
    }
    if (area == 0xBF && static_cast<unsigned char>(Cond_ByteFA) == 0xE) {
        if ((SH_CALL(Flags_Test)(reinterpret_cast<const unsigned char*>(Chapter_FlagRow), 5) & 0xFF) != 0) {
            FieldS_State = static_cast<unsigned char>(FieldS_State + 1);
            return;
        }
        area = Game_AreaNumber;
    }
    Music_Track = 0xFF;
    U kind = 0;
    if (area == 0x85) {
        SH_CALL(Flags_Clear)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x77);
        area = Game_AreaNumber;
        kind = 1;
    }
    if (area == 0xBF) kind = 2;
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF8);
    Field_MemberCount = static_cast<unsigned char>(SH_CALL(Party_Count)(0));
    SH_CALL(Field_PartyLoad)(0);
    if (Field_MemberCount != 0) {
        unsigned char i = 0;
        do {
            records::PartyMember* const obj = &ObjTrio[i];
            Sprite_Current = rv::Bytes(obj);
            SH_CALL(Field_MemberSprite)(obj->char_id, i);
            SH_CALL(Sprite_SetAnimation)(rv::Current<records::PartyMember>()->direction);
            const U row = ::bof3::Img(field_s::at::kPlaceTable) + 16 * (3 * kind + i);
            ++i;
            rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->x, static_cast<std::int32_t>(L(row)));
            rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->z, static_cast<std::int32_t>(L(row + 4)));
            rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->height, static_cast<std::int32_t>(L(row + 8)));
            rv::Current<records::PartyMember>()->state = 0;
            rv::Current<records::PartyMember>()->step = 0;
            rv::Current<records::PartyMember>()->state3 = 0;
            rv::Current<records::PartyMember>()->state4 = 0;
        } while (i < Field_MemberCount);
    }
    Field_Kind2X = static_cast<long>(static_cast<U>(ObjTrio[0].x));
    Field_Kind2Z = static_cast<long>(static_cast<U>(ObjTrio[0].z));
    SH_CALL(Field_ViewReset)();
    const U descriptor = L(::bof3::Img(field_s::at::kAreaDescriptors) + 4 * static_cast<std::uint32_t>(Game_AreaNumber));
    SH_CALL(Area_RunPlacement)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(L(descriptor))));
    FieldS_State = static_cast<unsigned char>(FieldS_State + 1);
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x580560, Rest_States[2]: black, Sound_LoadStream(0) (the jingle),
// the state up, the timer 0x96.
extern "C" void __cdecl Rest_LoadJingle(void) {
    SH_CALL(Menu_DrawBlackScreen)();
    SH_CALL(Sound_LoadStream)(0);
    const auto state = static_cast<unsigned char>(FieldS_State + 1);
    BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = 0x96;
    FieldS_State = state;
}

// original 0x580590, Rest_States[3]: black; a timer above 0 runs down and
// waits; at 0, once Sound_StreamDone answers not 0: the state up and
// Transition_Start(1).
extern "C" void __cdecl Rest_WaitJingle(void) {
    SH_CALL(Menu_DrawBlackScreen)();
    const unsigned char timer = BRest2c(::bof3::Img(rest_2c::at::kTimer.v));
    if (timer != 0) {
        BRest2c(::bof3::Img(rest_2c::at::kTimer.v)) = static_cast<unsigned char>(timer - 1);
        if (timer - 1 != 0) return;
    }
    if (SH_CALL(Sound_StreamDone)() == 0) return;
    ++FieldS_State;
    SH_CALL(Transition_Start)(1);
}

// original 0x5805D0, Rest_States[4]: when the wait word 0x66C810 is 0:
// Party_RestoreAll(0) and the state up (read after the call).
extern "C" void __cdecl Rest_Restore(void) {
    if (Transition_Busy != 0) return;
    SH_CALL(Party_RestoreAll)(0);
    ++FieldS_State;
}

// original 0x580600, Rest_States[5]: mode 1 (ShopMode_End), the state 0.
extern "C" void __cdecl Rest_End(void) {
    BattleE1_Mode = 1;
    FieldS_State = 0;
}

// original 0x580610, Rest_States[6] (Rest_Begin's, with its message 0xEF):
// once the message is done (0x7DEE44 bit 1), mode 1 and the state 0.
extern "C" void __cdecl Rest_EndAfterMessage(void) {
    if ((BRest2c(::bof3::Img(rest_2c::at::kMsgFlags.v)) & 2) == 0) return;
    BattleE1_Mode = 1;
    FieldS_State = 0;
}

// 0x580630 (PSX 0x801D68E0, the 'callers' tier). For each of the eight
// character records: with `full` (a byte) the HP scale +0x1E cleared,
// Char_ClearStatus(i, 0xA0, 0) and +0x1C = +0x2E; then +0x1D cleared,
// Char_RecalcStats, HP +0x18 = max +0x20 and AP +0x1A = max +0x22, +0x1D
// cleared again, Char_ClearStatus(i, 0x2000, 0). Then, when Flags_Test(0x904654,
// 4) and not Flags_Test(0x904657, 4), the byte 0x9045FB is cleared if it is
// below 0x1E. The inn (0x57FCD5) passes 1; 0x5805DC passes 0. The original's
// id dword is its loop byte over the entry's ecx; Char_ClearStatus reads the
// byte.
extern "C" void __cdecl Party_RestoreAll(unsigned full) {
    for (save_menu::U i = 0; i < 8; ++i) {
        const save_menu::U record = ::bof3::Img(CharacterRecords_addr) + i * save_menu::at::kCharStride;
        if ((full & 0xFF) != 0) {
            PutByte(record + 0x1E, 0);
            save_menu::g.clear_status(i, 0xA0, 0);
            PutByte(record + 0x1C, Byte(record + 0x2E));
        }
        PutByte(record + 0x1D, 0);
        save_menu::g.recalc(AtSaveMenu(record));
        const save_menu::U hp = WordSaveMenu(record + 0x20), ap = WordSaveMenu(record + 0x22);
        PutWord(record + 0x18, hp);
        PutWord(record + 0x1A, ap);
        PutByte(record + 0x1D, 0);
        save_menu::g.clear_status(i, 0x2000, 0);
    }
    if (save_menu::g.flags_test(AtSaveMenu(::bof3::Img(save_menu::at::kFlagsA.v)), 4) == 0) return;
    if (save_menu::g.flags_test(AtSaveMenu(::bof3::Img(save_menu::at::kFlagsB.v)), 4) != 0) return;
    if (static_cast<std::uint32_t>(AreaW1c_Byte9045FB) < 0x1E) AreaW1c_Byte9045FB = 0;
}

// original 0x5806F0: the live block 0x9039E0 gathered and copied out. Into
// the block: the chapter dwords 0x8034E0 / E4 / E8 / EC; the counters'
// bytes 0x903848..0x90384B; ObjTrio's leader +0x34 / +0x38 and its +8 byte;
// Game_AreaNumber; 0x903A50 = 0. The summary 0x904680: the buttons' dwords
// 0x903580 / 84 / 88 / 8C and word 0x903590 after it; the leader's name -
// the record of Character_IndexById[party id 0], strncpy 5 bytes, then 4
// from its +5 (both the C runtime's, through); the three party ids (id 4 as
// FormOf); record 0's +0xA (not the leader's), the dword 0x9040C8, the style
// bytes 0x903A5A / 0x903A5B, record 0's +0xC dword; Flags_Test(story flags,
// 0x92)'s al; 0x90412D = Field_ScriptFlags' byte 0x9039A2. Then the block's
// 0x10B0 bytes copied to 0x92A0E0 and summed (a 32-bit sum, its low word
// stored at 0x92A150 - the copy of the block's +0x70, which was 0 while
// summed), the next 0xD50 bytes cleared, and the summary copied to the
// slot's (0x9036D4).
// DIVERGENCE DIV-0076 (the owner, 2026-10-05): the summary's name is the
// leader's where its level and +0xC are record 0's; with the switch on, ours
// takes the name from record 0 too. The switch is set by Rest2C_Inject after
// the self-test, which compares Capcom's mix.
unsigned char g_summary_record0 = 0;

extern "C" void __cdecl Save_BuildBlock(void) {
    PutL(::bof3::Img(rest_2c::at::kBlock.v), LRest2c(::bof3::Img(0x8034E0)));
    PutL(::bof3::Img(rest_2c::at::kBlock.v) + 4, LRest2c(::bof3::Img(0x8034E4)));
    PutL(::bof3::Img(rest_2c::at::kBlock.v) + 0xC, LRest2c(::bof3::Img(0x8034EC)));
    const U counters = LRest2c(::bof3::Img(0x903848));
    PutL(::bof3::Img(rest_2c::at::kBlock.v) + 8, LRest2c(::bof3::Img(0x8034E8)));
    BRest2c(::bof3::Img(0x903A00)) = static_cast<unsigned char>(counters);
    BRest2c(::bof3::Img(0x903A01)) = static_cast<unsigned char>(counters >> 8);
    BRest2c(::bof3::Img(0x903A02)) = BRest2c(::bof3::Img(0x90384A));
    PutL(::bof3::Img(0x903A08), LRest2c(::bof3::Img(0x802D74)));
    PutL(::bof3::Img(0x903A0C), LRest2c(::bof3::Img(0x802D78)));
    BRest2c(::bof3::Img(0x903A07)) = ObjTrio[0].direction;
    SaveMenu_SavedButtons = LRest2c(::bof3::Img(0x903580));
    PutL(::bof3::Img(0x9046A4), LRest2c(::bof3::Img(0x903588)));
    BRest2c(::bof3::Img(0x903A03)) = BRest2c(::bof3::Img(0x90384B));
    PutL(::bof3::Img(0x9046A8), LRest2c(::bof3::Img(0x90358C)));
    FieldModes_Pending = Game_AreaNumber;
    PutL(::bof3::Img(0x9046A0), LRest2c(::bof3::Img(0x903584)));
    const unsigned record = BRest2c(::bof3::Img(rest_2c::at::kMemberRecord.v) + Cond_Flags[0xd2]);
    PutW(::bof3::Img(0x9046AC), Field_CancelButtons);
    PutW(::bof3::Img(0x903A50), 0);
    const U name = ::bof3::Img(CharacterRecords_addr) + (g_summary_record0 ? 0u : record) * rest_2c::at::kRecordStride; // DIV-0076
    SH_CALL(Crt_strncpy)(reinterpret_cast<char*>(AtRest2c(::bof3::Img(rest_2c::at::kSummary.v))), reinterpret_cast<const char*>(AtRest2c(name)), 5);
    SH_CALL(Crt_strncpy)(reinterpret_cast<char*>(AtRest2c(::bof3::Img(0x904696))), reinterpret_cast<const char*>(AtRest2c(name + 5)), 4);
    for (unsigned i = 0; i < 3; ++i) BRest2c(::bof3::Img(rest_2c::at::kPartyList.v) + 0x623 + i) = FormOf(BRest2c(::bof3::Img(rest_2c::at::kPartyList.v) + i));
    BRest2c(::bof3::Img(0x904688)) = BRest2c(::bof3::Img(0x903A7A));
    PutL(::bof3::Img(0x90468C), LRest2c(::bof3::Img(0x9040C8)));
    BRest2c(::bof3::Img(0x904689)) = static_cast<unsigned char>(Config_WindowColour);
    BRest2c(::bof3::Img(0x90468A)) = Config_Background;
    PutL(::bof3::Img(0x904690), LRest2c(::bof3::Img(0x903A7C)));
    const unsigned char flag = SH_CALL(Flags_Test)(AtRest2c(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x92);
    BRest2c(::bof3::Img(0x904694)) = flag;
    SaveMenu_ScriptFlagsSaved = BRest2c(::bof3::Img(0x9039A2));
    U sum = 0;
    for (U i = 0; i < rest_2c::at::kBlockBytes; ++i) {
        const unsigned char v = BRest2c(::bof3::Img(rest_2c::at::kBlock.v) + i);
        BRest2c(::bof3::Img(rest_2c::at::kStaging.v) + i) = v;
        sum += v;
    }
    std::memset(AtRest2c(::bof3::Img(Save_Staging_addr) + rest_2c::at::kBlockBytes), 0, rest_2c::at::kStagingClear);
    PutW(::bof3::Img(rest_2c::at::kChecksum.v), sum);
    CopySummary(Rest2c_Slot, "Save_BuildBlock");
}

// 0x5808E0 (PSX 0x801D6D7C). The window 0x57CF60(x + 4, y + 4, 0x52, 0x48, 0,
// colour byte) and 0x5762D0(x, y, 9, 8); then system messages 0xD2..0xD4 by
// Text_DrawAt at x + 0xC, one 20 px row each from y + 0xB, and Menu_DrawHand
// at x + 0xC on the row of the cursor byte 0x6BC881, y + 0xD. Kept, two
// register leftovers: the row index is `movzx si, bl` over x, so each text y
// has 20 * (x & 0xFFFF0000) in it (nothing for any x from 0 to 0xFFFF); and
// the hand's row is `movzx ax, byte`, over the upper half of the last
// Text_DrawAt's result. Both vanish in a 16-bit y.
extern "C" void __cdecl SaveMenu_DrawChoices(int x, int y) {
    save_menu::g.window_frame(x + 4, y + 4, 0x52, 0x48, 0, static_cast<std::uint32_t>(static_cast<unsigned char>(Config_WindowColour)));
    save_menu::g.window_box(x, y, 9, 8);
    const save_menu::U high = static_cast<save_menu::U>(x) & 0xFFFF0000u;
    save_menu::U last = 0;
    for (save_menu::U i = 0; i < 3; ++i) {
        const save_menu::U row = high | i;
        last = AddrSaveMenu(save_menu::g.text_draw(x + 0xC, static_cast<int>(static_cast<save_menu::U>(y) + row * 20 + 0xB), 0, 0xFF,
                                save_menu::g.msg_ptr(row + 0xD2)));
    }
    const save_menu::U hand = (last & 0xFFFF0000u) | static_cast<std::uint32_t>(ShopStates_Choice);
    save_menu::g.draw_hand(x + 0xC, static_cast<int>(static_cast<save_menu::U>(y) + hand * 20 + 0xD), 0);
}

// 0x580970 (PSX 0x801DF800). One TILE at Gfx_PacketNext: Gpu_SetTile,
// Gpu_SetSemiTrans(0), then (0, 0), 320 x 240 as floats and colour 0 - stored
// after the two calls - and Gfx_CommitPrim(1, 0x1C): the screen black behind
// the inn's night.
extern "C" void __cdecl Menu_DrawBlackScreen(void) {
    unsigned char* const p = Packet();
    save_menu::g.set_tile(p);
    save_menu::g.set_semi(p, 0);
    auto* const t = rv::As<records::GpuTile>(p);   // a TILE: Gpu_SetTile's packet
    const float wide = static_cast<float>(Widescreen_Live());   // DIV-0041: the black covers a wide picture whole
    t->x = 0.0f - wide;   // not -wide: that is -0.0f when wide is 0
    t->y = 0.0f;
    t->w = 320.0f + 2 * wide;
    t->h = 240.0f;
    t->r = 0;
    t->g = 0;
    t->b = 0;
    save_menu::g.commit(1, 0x1C);
}

// original 0x5809C0 (the window procedure's F12, Game_WndProc): slot 0, the
// file's name made with 0 (the slot not yet read), the block built, the name
// made again with the slot (read again), the file written; unless
// Save_WriteFile answered -1, the slot's summary.
extern "C" void __cdecl Save_QuickWrite(void) {
    Rest2c_Slot = 0;
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(AtRest2c(::bof3::Img(rest_2c::at::kPath.v))), reinterpret_cast<const char*>(AtRest2c(::bof3::Img(rest_2c::at::kPathFormat.v))), 0u);
    SH_CALL(Save_BuildBlock)();
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(AtRest2c(::bof3::Img(rest_2c::at::kPath.v))), reinterpret_cast<const char*>(AtRest2c(::bof3::Img(rest_2c::at::kPathFormat.v))), Rest2c_Slot);
    const int written = SH_CALL(Save_WriteFile)(reinterpret_cast<const char*>(AtRest2c(::bof3::Img(rest_2c::at::kPath.v))), rest_2c::at::kSaveSize);
    if (written == -1) return;
    CopySummary(Rest2c_Slot, "Save_QuickWrite");
}

#pragma clang attribute pop

void FieldSaveFromRest2c_Inject() {
    // DIVERGENCE DIV-0076: after the self-test, which compares Capcom's summary.
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("SaveSummaryRecord0",
                         Low32(&g_summary_record0), &was, &is, 1);
        bof3::Log("DIV-0076    the save slot's summary names record 0, whose level it already shows");
    }
}

void FieldSave_Inject() {
    BOF3_INJECT(Inn_Dispatch);
    BOF3_INJECT(Inn_Begin);
    BOF3_INJECT(InnPrompt_Dispatch);
    BOF3_INJECT(InnPrompt_TitleIn);
    BOF3_INJECT(InnPrompt_Greeting);
    BOF3_INJECT(InnPrompt_ChoicesIn);
    BOF3_INJECT(InnPrompt_Choose);
    BOF3_INJECT(InnPrompt_NotEnough);
    BOF3_INJECT(InnPrompt_ChoicesOut);
    BOF3_INJECT(InnNight_Dispatch);
    BOF3_INJECT(InnNight_Message);
    BOF3_INJECT(InnNight_FadeOut);
    BOF3_INJECT(InnNight_Jingle);
    BOF3_INJECT(InnNight_Wait);
    BOF3_INJECT(InnNight_Restore);
    BOF3_INJECT(InnNight_AskSave);
    BOF3_INJECT(InnNight_Leave);
    BOF3_INJECT(FieldSave_Dispatch);
    BOF3_INJECT(FieldSave_Begin);
    BOF3_INJECT(FieldSave_SlotsIn);
    BOF3_INJECT(FieldSave_Choose);
    BOF3_INJECT(FieldSave_Confirm);
    BOF3_INJECT(FieldSave_Write);
    BOF3_INJECT(FieldSave_Written);
    BOF3_INJECT(FieldSave_SlotsOut);
    BOF3_INJECT(FieldSave_End);
    BOF3_INJECT(FieldSave_PromptAnswer);
    BOF3_INJECT(Inn_TitleOut);
    BOF3_INJECT(Rest_Dispatch);
    BOF3_INJECT(Rest_Begin);
    BOF3_INJECT(Rest_PlaceParty);
    BOF3_INJECT(Rest_LoadJingle);
    BOF3_INJECT(Rest_WaitJingle);
    BOF3_INJECT(Rest_Restore);
    BOF3_INJECT(Rest_End);
    BOF3_INJECT(Rest_EndAfterMessage);
    BOF3_INJECT(Party_RestoreAll);
    BOF3_INJECT(Save_BuildBlock);
    BOF3_INJECT(SaveMenu_DrawChoices);
    BOF3_INJECT(Menu_DrawBlackScreen);
    BOF3_INJECT(Save_QuickWrite);
}
