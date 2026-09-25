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
// Faithful: no divergence. Every call out goes through shop_states::g, so the
// start-up fuzz can stand recorders in for the callees. The four dispatchers
// jump, as the originals do ([[clang::musttail]]); every other function has
// disable_tail_calls. None of the five tables' indices is checked, in the
// original or here: an index past a table reads the next table's entries, and
// past the last the dwords after it - memory, which ours reads alike.
#include "game/shop_states.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/shop_states_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace shop_states {

namespace {
template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Menu_DrawTitleBox, Menu_DrawMoneyBox, Text_DrawAt, Msg_SystemPtr, Msg_OpenSystem,
    Sound_PlayEffect, Sound_LoadStream, Sound_StreamDone,
    SaveMenu_DrawSlots, Save_ReadSummaries, Input_AutoRepeat, Menu_YesNo,
    Transition_Start, Sprite_FaceDirection, Window_ResetAll,
    Crt_sprintf, TextRecord_Set, SaveMenu_DrawChoices, Menu_DrawBlackScreen, Party_RestoreAll,
};
Callees g = kOriginals;

}  // namespace shop_states

using namespace shop_states;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
unsigned char& B(U address) { return *At(address); }
std::uint16_t Word(U address) {
    std::uint16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutWord(U address, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

using Handler = void (__cdecl*)();
Handler Entry(U table, unsigned index) { return Fn<Handler>(Long(table + index * 4)); }

// The screen title's box every inn and save state draws first: (0x14, 0x12),
// 0x118 by 0x13, in the window colour. The original pushes the colour byte in
// a register whose upper bytes it never cleared (entry eax, or edx after a
// call); Menu_DrawTitleBox uses it only through Gpu_GetClut's six-bit field
// (colour * 32 + 16 >> 4 & 0x3F), so ours zero-extends.
void TitleBox() { g.title_box(0x14, 0x12, 0x118, 0x13, B(at::kColour)); }

// System message `id` at (0x1C, 0x16), colour 0, to its end.
void Message(unsigned id) {
    const unsigned char* const text = g.system_ptr(id);
    g.draw_text(0x1C, 0x16, 0, 0xFF, text);
}

// The zenny box at (0x62, 0x28).
void Zenny() { g.money_box(0x62, 0x28, 0, Long(at::kZenny)); }

bool NoObject() { return B(at::kObject) == 0xFF; }

}  // namespace

// The dispatchers jump, as the originals do: the state's function returns to
// the dispatcher's caller.

// original 0x57F500: `jmp [ShopMode_States + 4 * (0x929F00 & 0xFF)]` - the
// state byte read as the low byte of a dword. Jumped to by Game_Mode 7's step
// 1 (0x517323) and by 0x56D4D5, so the arguments and the return are Field_Task's.
extern "C" void __cdecl ShopMode_Dispatch(void) {
    [[clang::musttail]] return Entry(at::kModeStates, B(at::kState))();
}

// original 0x57F650, state 3 (the inn): `jmp [Inn_Steps + 4 * 0x929F01]`.
extern "C" void __cdecl Inn_Dispatch(void) {
    [[clang::musttail]] return Entry(at::kInnSteps, B(at::kStep))();
}

// original 0x57F6E0, the inn's step 1: `jmp [InnPrompt_States + 4 * 0x929F02]`.
extern "C" void __cdecl InnPrompt_Dispatch(void) {
    [[clang::musttail]] return Entry(at::kPromptStates, B(at::kSub))();
}

// original 0x57FB60, the inn's step 2: `jmp [InnNight_States + 4 * 0x929F02]`.
extern "C" void __cdecl InnNight_Dispatch(void) {
    [[clang::musttail]] return Entry(at::kNightStates, B(at::kSub))();
}

// original 0x57FDD0, the inn's step 3 (the save point's only one):
// `jmp [FieldSave_States + 4 * 0x929F02]`.
extern "C" void __cdecl FieldSave_Dispatch(void) {
    [[clang::musttail]] return Entry(at::kSaveStates, B(at::kSub))();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ===========================================================================
// The mode's begin and end
// ===========================================================================

// original 0x57F520, ShopMode_States 0. With no object touched (0x929F0C
// negative, s8): the state is 0x929EC2 + 2. Otherwise Sprite_Current is the
// object (Sprite_Objects + 0xA4 n), the state its +0x18 + 2 and the shop
// number 0x929EC3 its +0x1C. Then each of the eight character records' status
// word gets bit 0x2000 when a quarter of its max HP (+0x10 >> 2, u16) is above
// its HP (+8), and loses it otherwise; the step, the sub-state and 0x929F0A
// cleared, the counter 6, and 0x903844 = 0x658930 + 23 * the shop number.
extern "C" void __cdecl ShopMode_Begin(void) {
    const auto object = static_cast<signed char>(B(at::kObject));
    if (object < 0) {
        B(at::kState) = static_cast<unsigned char>(B(at::kShopKind) + 2);
    } else {
        unsigned char* const o = At(at::kSpriteObjects + at::kSpriteStride * static_cast<U>(static_cast<int>(object)));
        PutLong(at::kSpriteCurrent, Addr(o));
        B(at::kState) = static_cast<unsigned char>(o[0x18] + 2);
        B(at::kShopNumber) = o[0x1C];
    }
    for (U n = 0; n < at::kActors; ++n) {
        const U r = at::kActorStates + n * at::kActorStride;
        const auto quarter = static_cast<std::uint16_t>(Word(r + 0x10) >> 2);
        if (quarter > Word(r + 8)) B(r + 1) = static_cast<unsigned char>(B(r + 1) | 0x20);
        else PutWord(r, Word(r) & 0xDFFFu);
    }
    B(at::kStep) = 0;
    B(at::kSub) = 0;
    B(at::kCount) = 6;
    B(at::kClear0A) = 0;
    PutLong(at::kShopRecord, at::kShopRecords + at::kShopRecordBytes * B(at::kShopNumber));
}

// original 0x57F5D0 (PSX 0x801D0FF8), ShopMode_States 1 and 4: the way out.
// With an object touched: Sprite_Current is it; unless its +7 has bit 3, its
// facing +8 goes back to +0x85, Sprite_FaceDirection(Sprite_Current's +8) and
// +0x80 |= 8 - on the object, not on Sprite_Current read again; then +0x80
// loses bit 5 and Field_ScriptFlags bit 8. Always: Window_ResetAll and
// Game_Step + 1.
extern "C" void __cdecl ShopMode_End(void) {
    const auto object = static_cast<signed char>(B(at::kObject));
    if (object >= 0) {
        unsigned char* const o = At(at::kSpriteObjects + at::kSpriteStride * static_cast<U>(static_cast<int>(object)));
        PutLong(at::kSpriteCurrent, Addr(o));
        if ((o[7] & 8) == 0) {
            o[8] = o[0x85];
            g.face(At(Long(at::kSpriteCurrent))[8]);
            o[0x80] = static_cast<unsigned char>(o[0x80] | 8);
        }
        o[0x80] = static_cast<unsigned char>(o[0x80] & 0xDF);
        PutWord(at::kScriptFlags, Word(at::kScriptFlags) & 0xFEFFu);
    }
    g.reset_windows();
    PutWord(at::kGameStep, Word(at::kGameStep) + 1u);
}

// ===========================================================================
// The inn: step 0
// ===========================================================================

// original 0x57F660, Inn_Steps 0: the step on (to 1, the prompt), the inn's
// cursor 0, the counter 4, the sub-state 0. Field_InputFlags 0x40, or area
// 0xBC, 0x85 or 0xC1: the object kept in 0x929F0F, 0x929F0C = 0xFE and
// straight to step 3, the save menu. Otherwise an object of 0xFE goes there
// too.
extern "C" void __cdecl Inn_Begin(void) {
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    B(at::kChoice) = 0;
    B(at::kStep) = step;
    B(at::kCount) = 4;
    B(at::kSub) = 0;
    bool forced = B(at::kInputFlags) == 0x40;
    if (!forced) {
        const std::uint16_t area = Word(at::kArea);
        forced = area == 0xBC || area == 0x85 || area == 0xC1;
    }
    if (forced) {
        const unsigned char object = B(at::kObject);
        B(at::kObject) = 0xFE;
        B(at::kSavedObject) = object;
        B(at::kStep) = 3;
        B(at::kSub) = 0;
        return;
    }
    if (B(at::kObject) == 0xFE) {
        B(at::kStep) = 3;
        B(at::kSub) = 0;
    }
}

// ===========================================================================
// The inn: step 1, the prompt (InnPrompt_States)
// ===========================================================================

// original 0x57F6F0, prompt 0: the title box slides down from 0x12 - 20 *
// counter (the counter read before the call); counter - 1, at 0 the next
// state, 0x929F0B = 1 and, with an object, Msg_OpenSystem(0xD1).
extern "C" void __cdecl InnPrompt_TitleIn(void) {
    const unsigned char colour = B(at::kColour);
    const int y = 0x12 - 20 * static_cast<int>(B(at::kCount));
    g.title_box(0x14, y, 0x118, 0x13, colour);
    const auto count = static_cast<unsigned char>(B(at::kCount) - 1);
    B(at::kCount) = count;
    if (count != 0) return;
    const auto sub = static_cast<unsigned char>(B(at::kSub) + 1);
    const unsigned char object = B(at::kObject);
    B(at::kAnswer) = 1;
    B(at::kSub) = sub;
    if (object != 0xFF) g.open_system(0xD1);
}

// original 0x57F760 (PSX 0x801D3BA8), prompt 1: the title box; with an
// object, the price - "%d" of 10 * the shop number into 0x904BA0, made text
// record 0 of 8 (TextRecord_Set) - system message 0xA1 and the zenny box.
// When the system message is done (bit 1 of 0x7DEE44): counter 6, the next
// state, Sound_PlayEffect(0x102).
extern "C" void __cdecl InnPrompt_Greeting(void) {
    TitleBox();
    if (!NoObject()) {
        g.sprintf_(reinterpret_cast<char*>(At(at::kText)), reinterpret_cast<const char*>(At(at::kFormatD)),
                   static_cast<int>(B(at::kShopNumber) * 10u));
        g.text_record(0, 8, At(at::kText));
        Message(0xA1);
        Zenny();
    }
    if ((B(at::kMsgFlags) & 2) == 0) return;
    const auto sub = static_cast<unsigned char>(B(at::kSub) + 1);
    B(at::kCount) = 6;
    B(at::kSub) = sub;
    g.play(0x102);
}

// original 0x57F810, prompt 2: the three choices slide in from 0x6E - 48 *
// counter (y 0x4C); the title box; with an object, message 0xA1 and the zenny
// box; counter - 1, at 0 the next state.
extern "C" void __cdecl InnPrompt_ChoicesIn(void) {
    g.choices(0x6E - 48 * static_cast<int>(B(at::kCount)), 0x4C);
    TitleBox();
    if (!NoObject()) {
        Message(0xA1);
        Zenny();
    }
    const auto count = static_cast<unsigned char>(B(at::kCount) - 1);
    B(at::kCount) = count;
    if (count == 0) B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
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
    g.choices(0x6E, 0x4C);
    TitleBox();
    if (!NoObject()) {
        Message(0xA1);
        Zenny();
    }
    U pad = Long(at::kPressed);
    unsigned char cursor = B(at::kChoice);
    const unsigned char was = cursor;
    bool moved = false;
    if (pad & 0x1000) {
        cursor = static_cast<unsigned char>(cursor - 1);
        B(at::kChoice) = cursor;
        if (static_cast<signed char>(cursor) < 0) {
            cursor = 2;
            B(at::kChoice) = cursor;
        }
        moved = true;
    } else if (pad & 0x4000) {
        cursor = static_cast<unsigned char>(cursor + 1);
        B(at::kChoice) = cursor;
        if (cursor > 2) {
            cursor = 0;
            B(at::kChoice) = cursor;
        }
        moved = true;
    }
    if (moved && was != cursor) {
        g.play(0x101);
        cursor = B(at::kChoice);
        pad = Long(at::kPressed);
    }
    if (Word(at::kConfirm) & pad) {
        B(at::kCount) = 0;
        B(at::kCancelled) = 0;
        if (cursor != 0) {
            g.play(0x104);
            B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 2);
            return;
        }
        if (Long(at::kZenny) >= B(at::kShopNumber) * 10u) {
            g.play(0x104);
            PutLong(at::kZenny, Long(at::kZenny) - B(at::kShopNumber) * 10u);
            B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 2);
            return;
        }
        g.play(0x107);
        B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
        return;
    }
    if (Word(at::kCancel) & pad) {
        g.play(0x106);
        const auto sub = static_cast<unsigned char>(B(at::kSub) + 2);
        B(at::kCount) = 0;
        B(at::kSub) = sub;
        B(at::kCancelled) = 1;
    }
}

// original 0x57FAB0, prompt 5: the choices slide out to 0x6E + 48 * counter;
// the title box; with an object, the zenny box (no message). Counter + 1; at
// 6: cancelled (0x6BC882) - the counter 0 and step + 3, the way out; else the
// sub-state 0, the counter 0x2D for cursor 0 (the night's message) or 0, and
// step + cursor + 1: 0 the night, 1 the save menu, 2 the way out.
extern "C" void __cdecl InnPrompt_ChoicesOut(void) {
    g.choices(0x6E + 48 * static_cast<int>(B(at::kCount)), 0x4C);
    TitleBox();
    if (!NoObject()) Zenny();
    const auto count = static_cast<unsigned char>(B(at::kCount) + 1);
    B(at::kCount) = count;
    if (count != 6) return;
    if (B(at::kCancelled) != 0) {
        const unsigned char step = B(at::kStep);
        B(at::kCount) = 0;
        B(at::kStep) = static_cast<unsigned char>(step + 3);
        return;
    }
    const unsigned char cursor = B(at::kChoice);
    B(at::kSub) = 0;
    B(at::kCount) = cursor != 0 ? 0 : 0x2D;
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + cursor + 1);
}

// ===========================================================================
// The inn: step 2, the night (InnNight_States)
// ===========================================================================

// original 0x57FB70, night 0: the title box and, with an object, message
// 0xA4; counter - 1, at 0 the next state and Transition_Start(0), the fade out.
extern "C" void __cdecl InnNight_Message(void) {
    TitleBox();
    if (!NoObject()) Message(0xA4);
    const auto count = static_cast<unsigned char>(B(at::kCount) - 1);
    B(at::kCount) = count;
    if (count != 0) return;
    B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
    g.transition(0);
}

// original 0x57FBE0, night 1: once the transition is over (0x66C810 zero)
// Menu_DrawBlackScreen and the next state; until then, with an object, the
// title box and message 0xA4.
extern "C" void __cdecl InnNight_FadeOut(void) {
    if (Word(at::kFade) == 0) {
        g.black();
        B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
        return;
    }
    if (NoObject()) return;
    TitleBox();
    Message(0xA4);
}

// original 0x57FC40, night 2: the black screen, Sound_LoadStream(0) - the
// inn's tune - the next state, the counter 0x96.
extern "C" void __cdecl InnNight_Jingle(void) {
    g.black();
    g.stream(0);
    const auto sub = static_cast<unsigned char>(B(at::kSub) + 1);
    B(at::kCount) = 0x96;
    B(at::kSub) = sub;
}

// original 0x57FC70, night 3: the black screen; the counter down to 0 (not
// below); at 0, when Sound_StreamDone answers non-zero, the next state and
// Transition_Start(1), the fade in.
extern "C" void __cdecl InnNight_Wait(void) {
    g.black();
    unsigned char count = B(at::kCount);
    if (count != 0) {
        --count;
        B(at::kCount) = count;
        if (count != 0) return;
    }
    if (g.stream_done() == 0) return;
    B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
    g.transition(1);
}

// original 0x57FCB0 (PSX 0x801D447C), night 4: the title box; once the
// transition is over, Party_RestoreAll(1), the next state, 0x929F0B = 1,
// Input_AutoRepeat's latch and timer cleared and, with an object,
// Msg_OpenSystem(0xD5).
extern "C" void __cdecl InnNight_Restore(void) {
    TitleBox();
    if (Word(at::kFade) != 0) return;
    g.restore(1);
    const auto sub = static_cast<unsigned char>(B(at::kSub) + 1);
    const unsigned char object = B(at::kObject);
    B(at::kAnswer) = 1;
    B(at::kSub) = sub;
    PutWord(at::kRepeatLatch, 0);
    PutWord(at::kRepeatTimer, 0);
    if (object != 0xFF) g.open_system(0xD5);
}

// original 0x57FD20, night 5: the title box. With an object, the next state
// once the system message is done (bit 1 of 0x7DEE44). Without one, system
// message 0x1D (save?) and Menu_YesNo: an answer, the next state.
extern "C" void __cdecl InnNight_AskSave(void) {
    TitleBox();
    if (!NoObject()) {
        if (B(at::kMsgFlags) & 2) B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
        return;
    }
    Message(0x1D);
    if (g.yes_no() != 0) B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
}

// original 0x57FD80, night 6: the title box; the counter and the sub-state
// 0; step + 1 (the save menu) on a yes (0x929F0B), + 2 (the way out) on a no.
extern "C" void __cdecl InnNight_Leave(void) {
    TitleBox();
    const unsigned char answer = B(at::kAnswer);
    const unsigned char step = B(at::kStep);
    B(at::kCount) = 0;
    B(at::kSub) = 0;
    B(at::kStep) = static_cast<unsigned char>(answer == 0 ? step + 2 : step + 1);
}

// ===========================================================================
// The inn: step 3, the save menu (FieldSave_States)
// ===========================================================================

// original 0x57FDE0, save 0: Save_ReadSummaries (DIV-0002's refresh lives in
// Save_WriteFile, not here), the counter 4, the cursor and the first slot
// shown 0, the next state.
extern "C" void __cdecl FieldSave_Begin(void) {
    g.read_summaries();
    B(at::kCount) = 4;
    PutLong(at::kSlot, 0);
    PutLong(at::kSlotTop, 0);
    B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
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
    if (B(at::kCount) == 4) g.play(0x102);
    TitleBox();
    g.slots(0x20 - 80 * static_cast<int>(B(at::kCount)), 0x30, 1);
    const auto count = static_cast<unsigned char>(B(at::kCount) - 1);
    B(at::kCount) = count;
    if (count == 0) B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
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
    TitleBox();
    Message(0x9D);
    g.slots(0x20, 0x30, 1);
    const U repeat = g.auto_repeat(Word(at::kPressed) & 0x5000u);
    U slot = Long(at::kSlot);
    const U old = slot & 0xFF;
    if ((repeat & 0x1000) && slot != 0) {
        const U top = Long(at::kSlotTop);
        --slot;
        PutLong(at::kSlot, slot);
        if (static_cast<int>(slot) < static_cast<int>(top)) PutLong(at::kSlotTop, top - 1);
    } else if ((repeat & 0x4000) && static_cast<int>(slot) < 15) {
        const U limit = Long(at::kSlotTop) + 2;
        ++slot;
        PutLong(at::kSlot, slot);
        if (static_cast<int>(slot) > static_cast<int>(limit)) PutLong(at::kSlotTop, Long(at::kSlotTop) + 1);
    }
    if (old != slot) g.play(0x101);
    const std::uint16_t pad = Word(at::kPressed);
    if (Word(at::kConfirm) & pad) {
        g.play(0x104);
        const auto sub = static_cast<unsigned char>(B(at::kSub) + 1);
        B(at::kAnswer) = 0;
        B(at::kSub) = sub;
        return;
    }
    if (Word(at::kCancel) & pad) {
        g.play(0x106);
        B(at::kSub) = 6;
    }
}

// original 0x580150, save 6: the title box; the slots, unlit, sliding out to
// 0x20 + 80 * counter (the same leftover upper half as FieldSave_SlotsIn's);
// counter + 1, at 4 the next state.
extern "C" void __cdecl FieldSave_SlotsOut(void) {
    TitleBox();
    g.slots(0x20 + 80 * static_cast<int>(B(at::kCount)), 0x30, 0);
    const auto count = static_cast<unsigned char>(B(at::kCount) + 1);
    B(at::kCount) = count;
    if (count == 4) B(at::kSub) = static_cast<unsigned char>(B(at::kSub) + 1);
}

// original 0x5801A0, save 7: the title box; the sub-state 0. An inn (0x929F0C
// not 0xFE) whose cursor is not 2 goes back to the prompt: step 1, sub-state
// 1 (InnPrompt_Greeting). Otherwise, with 0x66C7DA set and 0x6BC880 clear:
// Input_AutoRepeat's latch and timer and 0x929F0B cleared, state 8 (0x580230,
// Capcom's) and Msg_OpenSystem(0xD0); else the counter 0 and step + 1, the
// way out.
extern "C" void __cdecl FieldSave_End(void) {
    TitleBox();
    const unsigned char cursor = B(at::kChoice);
    B(at::kSub) = 0;
    if (cursor != 2 && B(at::kObject) != 0xFE) {
        B(at::kStep) = 1;
        B(at::kSub) = 1;
        return;
    }
    if (B(at::kInnFlag) != 0 && B(at::kSaveBack) == 0) {
        PutWord(at::kRepeatLatch, 0);
        PutWord(at::kRepeatTimer, 0);
        B(at::kAnswer) = 0;
        B(at::kSub) = 8;
        g.open_system(0xD0);
        return;
    }
    B(at::kCount) = 0;
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

#pragma clang attribute pop

// ===========================================================================

void ShopStates_Inject() {
    if (bof3::WantsShadow("shop_states")) shop_states::SelfTest();
    BOF3_INJECT(ShopMode_Dispatch);
    BOF3_INJECT(ShopMode_Begin);
    BOF3_INJECT(ShopMode_End);
    BOF3_INJECT(Inn_Dispatch);
    BOF3_INJECT(Inn_Begin);
    BOF3_INJECT(InnPrompt_Dispatch);
    BOF3_INJECT(InnPrompt_TitleIn);
    BOF3_INJECT(InnPrompt_Greeting);
    BOF3_INJECT(InnPrompt_ChoicesIn);
    BOF3_INJECT(InnPrompt_Choose);
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
    BOF3_INJECT(FieldSave_SlotsOut);
    BOF3_INJECT(FieldSave_End);
}
