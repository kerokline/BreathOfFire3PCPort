// Moved here by tools/move_funcs.py from area_entry.cpp, effect_1g.cpp, field_e2.cpp, game_last.cpp, mode_flow.cpp, rest_2f.cpp, window_task.cpp (docs/source-layout-round.md).
#include "game/menu/item_trade.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/area_data_reload.h"
#include "game/area_entry_callees.h"
#include "game/effect_1g.h"
#include "game/effect_1g_callees.h"
#include "game/field_e2.h"
#include "game/field_e2_callees.h"
#include "game/game_last_callees.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu/item_trade_callees.h"
#include "game/mode_flow.h"
#include "game/mode_flow_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2f.h"
#include "game/rest_2f_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "game/window_task.h"
#include "game/window_task_callees.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = item_trade::at;

}  // namespace

namespace {

using namespace move_script;

using namespace area_entry;

}  // namespace

using namespace mode_flow;

using namespace window_task;

namespace {

unsigned char* Ptr(const unsigned char* at) { return move_script::At(static_cast<std::uint32_t>(move_script::Long(at))); }

unsigned char* Descriptor() { return Area_Descriptors[Game_AreaNumber]; }

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U a) { return *move_script::At(a); }

U L(U a) { return static_cast<U>(move_script::Long(move_script::At(a))); }

std::uint16_t W(U a) { return move_script::Word(move_script::At(a)); }

U Key(const void* p) { return Low32(p); }

void PutFloat(unsigned char* p, float f) { __builtin_memcpy(p, &f, 4); }

using Handler = void (__cdecl*)();

// A .data table's entry read in place (swapped for recorders while the fuzz
// runs); an entry that is not code - an index past the table - aborts, where
// the original jumps there (field_e2.cpp's CodeAt).
U CodeAt(U table, U index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = L(cell);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}

// The record entry k of the screen's row names: kTradeRecords +
// 8 * kTradeIndex[k + row * 10], the row byte read here (the originals read it
// again before each use). Unchecked, as the originals: an entry past its row's
// 0xFF, or a row past the table, reads further into .data (docs/effect_1g.md
// section 6).
U Record(U k) { return ::bof3::Img(effect_1g::at::kTradeRecords.v) + 8u * B(::bof3::Img(effect_1g::at::kTradeIndex.v) + k + 10u * AreaEntry_PendingFlags); }

// A prompt: MessagePools + the u16 at `word`.
const unsigned char* Pool(U word) { return move_script::At(::bof3::Img(bof3::addr::MessagePools) + W(word)); }

char* NumberText() { return reinterpret_cast<char*>(move_script::At(::bof3::Img(effect_1g::at::kNumberText.v))); }

const char* NumberFormat() { return reinterpret_cast<const char*>(move_script::At(::bof3::Img(effect_1g::at::kNumberFormat.v))); }

// E1B's frame of the trade screen, as every state draws it.
void TradeBoxEffect1g() { SH_AT(void (__cdecl*)(int, int, int, int, int), effect_1g::at::kTradeBox.v)(0x14, 0x12, 0x118, 0x13, 0); }

void PieceMode(unsigned id) { SH_AT(void (__cdecl*)(unsigned, unsigned), effect_1g::at::kPieceMode.v)(id, 1); }

void Piece(unsigned id, U x, U y) { SH_AT(void (__cdecl*)(unsigned, unsigned, U, U), effect_1g::at::kPiece.v)(id, 1, x, y); }

void Quad(U x, U y, unsigned length, unsigned piece) { SH_AT(void (__cdecl*)(U, U, unsigned, unsigned), effect_1g::at::kQuad.v)(x, y, length, piece); }

// The yes / no prompt of the leave steps: the frame, the question, the hand
// at (36 * the answer + 0xE0, 0x16), the list, the ingredients, the backdrop.
void LeavePrompt() {
    TradeBoxEffect1g();
    if (g_trade_leave_layout) {
        const YesNoTail t = YesNoLayout_Tail(Pool(::bof3::Img(effect_1g::at::kPoolWordAsk.v)), 0x18, "Manillo's prompt");
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, t.line);
        SH_CALL(Menu_DrawHand)(t.stop[0] + Effect1g_TradeAnswer * (t.stop[1] - t.stop[0]), 0x16, 0);
    } else {
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, Pool(::bof3::Img(effect_1g::at::kPoolWordAsk.v)));
        SH_CALL(Menu_DrawHand)(static_cast<int>(36u * Effect1g_TradeAnswer + 0xE0u), 0x16, 0);
    }
    SH_CALL(ItemTrade_DrawList)(0);
    SH_CALL(ItemTrade_DrawNeeds)();
    SH_CALL(ItemTrade_DrawBackground)();
}

unsigned char& BFieldE2(U a) { return *move_script::At(a); }

U LFieldE2(U a) { return static_cast<U>(move_script::Long(move_script::At(a))); }

int S8FieldE2(unsigned v) { return static_cast<signed char>(v); }

// A .data table's entry read in place (swapped for recorders while the fuzz
// runs); an entry that is not code - an index past the table - aborts, where
// the original jumps there (scena_sc0.cpp's CodeAt).
U CodeAtFieldE2(U table, U index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = LFieldE2(cell);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}

// The record the row's entry k names (0x66AD10[k + row * 10] * 8), unchecked.
U TradeRecord(int k) {
    return ::bof3::Img(field_e2::at::kTradeRecords) + static_cast<U>(BFieldE2(::bof3::Img(field_e2::at::kTradeIndex) + static_cast<U>(k + AreaEntry_PendingFlags * 10))) * 8u;
}

void TradeBox() { SH_AT(void (__cdecl*)(int, int, int, int, int), field_e2::at::kTradeBox.v)(0x14, 0x12, 0x118, 0x13, 0); }

void TradeList(unsigned flag) { SH_AT(void (__cdecl*)(unsigned), field_e2::at::kTradeList.v)(flag); }

unsigned char TradeLacks(unsigned k, unsigned quantity) {
    return SH_AT(unsigned char (__cdecl*)(unsigned, unsigned), field_e2::at::kTradeLacks.v)(k, quantity);
}

const unsigned char* PoolFieldE2(U word_cell) { return move_script::At(::bof3::Img(bof3::addr::MessagePools) + (LFieldE2(word_cell) & 0xFFFF)); }

struct ConfirmAnswers {
    const unsigned char* line;   // a static buffer, good until the next call
    int x;
    int stop[2];                 // the hand on the first answer, on the second
};

ConfirmAnswers ConfirmLayout(const unsigned char* s) {
    constexpr unsigned kMoved = 3;
    static unsigned char line[32];
    static const unsigned char kSpace[2] = {0x20, 0};
    unsigned first = 0;
    while (s[first] && s[first] != 0x20) ++first;
    unsigned second = first;
    while (s[second] == 0x20) ++second;
    unsigned end = second;
    while (s[end] && s[end] != 0x20) ++end;
    bool plain = first != 0 && second != first && end != second && s[end] == 0 && end + kMoved < sizeof line;
    for (unsigned i = 0; plain && i < end; ++i) plain = s[i] >= 0x20 && s[i] < 0x80;
    if (!plain) bof3::Fatal("DIV-0027: the trade's answers are not two one-byte words with spaces between");
    unsigned n = 0;
    for (unsigned i = 0; i < first; ++i) line[n++] = s[i];
    for (unsigned i = 0; i < second - first + kMoved; ++i) line[n++] = 0x20;
    const unsigned second_at = n;
    for (unsigned i = second; i < end; ++i) line[n++] = s[i];
    line[n] = 0;
    const int x = 0xDE - static_cast<int>(kMoved) * TextAdvance_Of(kSpace);
    int pen = x;
    for (unsigned i = 0; i < second_at; ++i) pen += TextAdvance_Of(line + i);
    return {line, x, {x - 2, pen - 2}};
}

}  // namespace

namespace mode_flow {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

namespace {

template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

}  // namespace

}  // namespace mode_flow

namespace {

constexpr ::bof3::image::Va kTradeKinds{0x66AD10};  // 10 a spot: the trade's kind

constexpr ::bof3::image::Va kTradeNeeds{0x66AB5A};  // 8 a kind: three item bytes, then their counts at +3

unsigned char& BRest2f(U address) { return move_script::At(address)[0]; }

}  // namespace

namespace window_task {

using move_script::At;

using move_script::Long;

using move_script::Word;

}  // namespace window_task

// window_task's callee seam, moved here from window_task.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace window_task {
namespace {

template <typename T> T As(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Area_ClassifyPending,
    Area_PickMusic,
    Area_ZoneAtIn,
    Window_DrawFrame,
    Window_DrawOutline,
    Window_DrawLine,
    Window_FreeCurrent,
    Flags_Test,
    As<unsigned char(__cdecl*)()>(::bof3::Code(kMsgBoxFrameTask.v)),
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_SetTile,
    Gpu_SetPolyGT4,
    Gpu_SetLineF4,
    Gpu_SetLineF2,
    Gpu_SetSemiTrans,
    Gpu_GetTPage,
    Gpu_GetClut,
    As<Handler>(::bof3::Code(kListSetUp.v)),
    As<Handler>(::bof3::Code(kListDraw.v)),
    As<Handler>(::bof3::Code(kListCursorDraw.v)),
    As<Handler>(::bof3::Code(kSetDraw.v)),
    As<Handler>(::bof3::Code(kSetCursorDraw.v)),
    {As<Handler>(::bof3::Code(kRecordHandlers[0])), As<Handler>(::bof3::Code(kRecordHandlers[1])), As<Handler>(::bof3::Code(kRecordHandlers[2])),
     As<Handler>(::bof3::Code(kRecordHandlers[3])), As<Handler>(::bof3::Code(kRecordHandlers[4])), As<Handler>(::bof3::Code(kRecordHandlers[5])),
     As<Handler>(::bof3::Code(kRecordHandlers[6])), As<Handler>(::bof3::Code(kRecordHandlers[7])), As<Handler>(::bof3::Code(kRecordHandlers[8]))},
    {As<Handler>(::bof3::Code(bof3::addr::Window_Kind0States)), As<Handler>(::bof3::Code(bof3::addr::Window_Kind1States)),
     As<Handler>(::bof3::Code(bof3::addr::Window_Kind2States))},
    {As<Handler>(::bof3::Code(bof3::addr::Window_Kind0Open)), As<Handler>(::bof3::Code(bof3::addr::Window_Grow)),
     As<Handler>(::bof3::Code(bof3::addr::Window_Kind0Frame)), As<Handler>(::bof3::Code(bof3::addr::Window_Shrink)),
     As<Handler>(::bof3::Code(bof3::addr::Window_Kind0Close))},
    {As<Handler>(::bof3::Code(bof3::addr::Window_Kind1Open)), As<Handler>(::bof3::Code(bof3::addr::Window_Grow)),
     As<Handler>(::bof3::Code(bof3::addr::Window_Kind1Frame)), As<Handler>(::bof3::Code(bof3::addr::Window_Shrink)),
     As<Handler>(::bof3::Code(bof3::addr::Window_FreeState))},
    {As<Handler>(::bof3::Code(bof3::addr::Window_Kind2Open)), As<Handler>(::bof3::Code(bof3::addr::Window_Grow)),
     As<Handler>(::bof3::Code(bof3::addr::Window_Kind2Frame)), As<Handler>(::bof3::Code(bof3::addr::Window_Shrink)),
     As<Handler>(::bof3::Code(bof3::addr::Window_FreeState))},
};
Callees g = kOriginals;

}  // namespace window_task

// original 0x593950 (GameMode8_TradeStep's tail jump 0x52CF30): xor eax, eax;
// mov al, [0x93985C]; jmp [ItemTrade_States + eax * 4] - unchecked. The three
// states are ItemTrade_Open, ItemTrade_Run, ItemTrade_Leave (all ours); the
// byte is written 0, 1, +1 and -1 and nothing else (docs/game-last.md section
// 1.2). Past the three the original jumps through ItemTrade_OpenSteps' cells
// (ItemTrade_OpenStart for 3); ours aborts with a message. The handler is the
// table's cell read in place - in the game Capcom's address or the jump Inject
// put there to ours, a recorder while the fuzz runs ours.
extern "C" void __cdecl ItemTrade_Dispatch(void) {
    const unsigned state = Effect1g_TradeState;
    if (state >= ItemTrade_States_count)
        bof3::Fatal("ItemTrade_Dispatch: the trade state 0x93985C is %u, past the %u entries of ItemTrade_States 0x66A470 - "
                    "the original jumps through the dword after (docs/game-last.md section 4)",
                    state, ItemTrade_States_count);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ItemTrade_States[state]))();
}

// original 0x593960 (ItemTrade_States[0]; PSX 0x800F5048): the open step (ItemTrade_OpenSteps
// 0x66A47C by 0x93985E, past its two ours aborts only where the table runs
// into what is not code), then the frame 0x469750(0x14, 0x12, 0x118, 0x13, 0),
// the list 0x594410(0), 0x5947D0 and a tail jump to 0x5942C0.
extern "C" void __cdecl ItemTrade_Open(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAtFieldE2(::bof3::Img(ItemTrade_OpenSteps_addr), Effect1g_TradeStep, "ItemTrade_Open")))();
    TradeBox();
    TradeList(0);
    SH_AT(void (__cdecl*)(), field_e2::at::kTradeCursor.v)();
    SH_AT(void (__cdecl*)(), field_e2::at::kTradeFrame.v)();
}

// original 0x5939A0 (ItemTrade_OpenSteps[0]; PSX 0x800F50B4): Transition_Start(1), the pick
// 0x6BE08C = 0, the quantity 0x6BE08E = 1, the row count 0x6BE08D = 0x594790's
// al, the step up.
extern "C" void __cdecl ItemTrade_OpenStart(void) {
    SH_CALL(Transition_Start)(1);
    Effect1g_TradePick = 0;
    Effect1g_TradeQuantity = 1;
    Effect1g_TradeRowCount = SH_AT(unsigned char (__cdecl*)(), field_e2::at::kTradeRows.v)();
    Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep + 1);
}

// original 0x5939D0 (ItemTrade_OpenSteps[1]; PSX 0x800F510C): once Transition_Busy is
// 0, the state 0x93985C = 1 and the step 0.
extern "C" void __cdecl ItemTrade_OpenWait(void) {
    if (Transition_Busy != 0) return;
    Effect1g_TradeState = 1;
    Effect1g_TradeStep = 0;
}

// original 0x5939F0 (ItemTrade_States[1]; PSX 0x800F5138): the run step (ItemTrade_RunSteps
// 0x66A484 by 0x93985E: pick, count, confirm, and 0x594060's message), then
// 0x5942C0, 0x5947D0 and, with the pick's bits 6 and 7 clear, the hand at
// (0x1A, 0x51 + 13 * the s8 pick).
extern "C" void __cdecl ItemTrade_Run(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAtFieldE2(::bof3::Img(ItemTrade_RunSteps_addr), Effect1g_TradeStep, "ItemTrade_Run")))();
    SH_AT(void (__cdecl*)(), field_e2::at::kTradeFrame.v)();
    SH_AT(void (__cdecl*)(), field_e2::at::kTradeCursor.v)();
    const unsigned char pick = static_cast<unsigned char>(Effect1g_TradePick);
    if (pick & 0xC0) return;
    SH_CALL(Menu_DrawHand)(0x1A, static_cast<int>(13u * static_cast<U>(S8FieldE2(pick)) + 0x51u), 0);
}

// original 0x593A30 (ItemTrade_RunSteps[0]; PSX 0x800F51BC): the pick. Up and down
// (Input_AutoRepeat of the pressed word's 0x5000) move it, wrapping at the
// row count, with sound 0x100. Confirm: 0x594700(pick, 1) answering (an
// ingredient short) - sound 0x107; else the two Inventory_Count of the
// entry's item (in the bag, then equipped) summed as a byte: 99 or more - the
// pick's bit 7, sound 0x104, the quantity 0, step 3 (0x594060's message);
// else bit 6, sound 0x104, the step up. Cancel: bit 7, 0x6BE08F = 1, sound
// 0x106, the state 0x93985C up. Then the frame, and with bits 6 and 7 clear
// the entry's help line (Item_HelpMessage through Msg_SystemPtr) at (0x18,
// 0x14); the list 0x594410(0).
extern "C" void __cdecl ItemTrade_PickItem(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0x5000u);
    if (moved & 0x1000u) {
        const auto pick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) - 1);
        Effect1g_TradePick = pick;
        if (static_cast<signed char>(pick) < 0) Effect1g_TradePick = static_cast<unsigned char>(Effect1g_TradeRowCount - 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else if (moved & 0x4000u) {
        const auto pick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) + 1);
        Effect1g_TradePick = pick;
        if (static_cast<signed char>(pick) >= Effect1g_TradeRowCount) Effect1g_TradePick = 0;
        SH_CALL(Sound_PlayEffect)(0x100);
    }
    const std::uint16_t pressed = Input_Pressed;
    if (Field_ConfirmButtons & pressed) {
        if (TradeLacks(static_cast<unsigned char>(Effect1g_TradePick), 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x107);
        } else {
            const U record = TradeRecord(Effect1g_TradePick);
            const auto bag = static_cast<unsigned char>(SH_CALL(Inventory_Count)(BFieldE2(record + 1), BFieldE2(record), 1));
            const auto worn = static_cast<unsigned char>(SH_CALL(Inventory_Count)(BFieldE2(record + 1), BFieldE2(record), 0));
            if (static_cast<unsigned char>(bag + worn) >= 0x63) {
                Effect1g_TradePick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) | 0x80);
                SH_CALL(Sound_PlayEffect)(0x104);
                Effect1g_TradeQuantity = 0;
                Effect1g_TradeStep = 3;
            } else {
                Effect1g_TradePick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) | 0x40);
                SH_CALL(Sound_PlayEffect)(0x104);
                BFieldE2(::bof3::Img(field_e2::at::kTradeStep)) = static_cast<unsigned char>(Effect1g_TradeStep + 1);
            }
        }
    } else if (Field_CancelButtons & pressed) {
        Effect1g_TradeAnswer = 1;
        Effect1g_TradePick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) | 0x80);
        SH_CALL(Sound_PlayEffect)(0x106);
        Effect1g_TradeState = static_cast<unsigned char>(Effect1g_TradeState + 1);
    }
    TradeBox();
    const unsigned char pick = static_cast<unsigned char>(Effect1g_TradePick);
    if ((pick & 0xC0) == 0) {
        const U record = TradeRecord(S8FieldE2(pick));
        const unsigned help = SH_CALL(Item_HelpMessage)(BFieldE2(record + 1), BFieldE2(record));
        const unsigned char* const text = SH_CALL(Msg_SystemPtr)(help);
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, text);
    }
    TradeList(0);
}

// original 0x593C60 (ItemTrade_RunSteps[1]; PSX 0x800F550C): the count. Input_AutoRepeat of
// the pressed word's 0xF000: 0x2000 +1, 0x1000 +10, 0x4000 -10, 0x8000 -1
// (a byte), the quantity 0x6BE08E held to 1..99 (s8); lowered while
// 0x594700(pick & 0xF, quantity) answers; lowered while it plus the item's
// two Inventory_Count summed as a byte passes 99; sound 0x100 if it moved.
// Confirm: 0x6BE08F = 0, sound 0x104, the step up. Cancel: the pick's bit 6
// cleared, the quantity 1, sound 0x106, the step down. Then the frame, the
// prompt (MessagePools + the word at 0x80361C) at (0x18, 0x14), the list
// 0x594410(1), 0x594AD0 and the hand at (0x1A, 0x6B).
extern "C" void __cdecl ItemTrade_PickCount(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000u) & 0xFFFFu;
    unsigned char step = 0;
    if (moved == 0x8000u) step = 0xFF;
    else if (moved == 0x4000u) step = 0xF6;
    else if (moved == 0x1000u) step = 0xA;
    else if (moved == 0x2000u) step = 1;
    const unsigned char old = static_cast<unsigned char>(Effect1g_TradeQuantity);
    auto quantity = static_cast<unsigned char>(old + step);
    if (static_cast<signed char>(quantity) < 1) quantity = 1;
    else if (static_cast<signed char>(quantity) > 0x63) quantity = 0x63;
    const unsigned k = static_cast<unsigned char>(Effect1g_TradePick) & 0xFu;
    Effect1g_TradeQuantity = quantity;
    while (TradeLacks(k, quantity) != 0) {
        quantity = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradeQuantity) - 1);
        Effect1g_TradeQuantity = quantity;
    }
    const U record = TradeRecord(static_cast<int>(k));
    const auto bag = static_cast<unsigned char>(SH_CALL(Inventory_Count)(BFieldE2(record + 1), BFieldE2(record), 1));
    const auto worn = static_cast<unsigned char>(SH_CALL(Inventory_Count)(BFieldE2(record + 1), BFieldE2(record), 0));
    const U held = static_cast<unsigned char>(bag + worn);
    // The s8 quantity comes down until it plus the held count is 99 or less:
    // with more than 227 held no s8 does, and the original loops for ever.
    if (held > 227) bof3::Fatal("ItemTrade_PickCount: %u held - no quantity brings the sum to 99 (the original never ends)", (unsigned)held);
    unsigned char q = static_cast<unsigned char>(Effect1g_TradeQuantity);
    while (static_cast<int>(S8FieldE2(q)) + static_cast<int>(held) > 0x63) q = static_cast<unsigned char>(q - 1);
    Effect1g_TradeQuantity = q;
    if (q != old) SH_CALL(Sound_PlayEffect)(0x100);
    const std::uint16_t pressed = Input_Pressed;
    if (Field_ConfirmButtons & pressed) {
        Effect1g_TradeAnswer = 0;
        SH_CALL(Sound_PlayEffect)(0x104);
        Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep + 1);
    } else if (Field_CancelButtons & pressed) {
        Effect1g_TradeQuantity = 1;
        Effect1g_TradePick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) & 0xBF);
        SH_CALL(Sound_PlayEffect)(0x106);
        Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep - 1);
    }
    TradeBox();
    SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, PoolFieldE2(::bof3::Img(field_e2::at::kPoolWordPick)));
    TradeList(1);
    SH_AT(void (__cdecl*)(), field_e2::at::kTradeCount.v)();
    SH_CALL(Menu_DrawHand)(0x1A, 0x6B, 0);
}

// DIVERGENCE DIV-0027 (amended 2026-10-03, after the live check): the per-item
// "Is <item> OK?" answers. Capcom draws the two answer words, one space apart,
// at x 0xDE and the hand at 0xDC + 36 * the answer - stops fitted to the Chinese
// words: under the English overlay the hand on No covers Yes (the owner's
// masterAndManillo.txt, frames 2700 and 3030). With the flag on (FieldE2_Inject,
// a Latin overlay, after the self-test) the layout is the load / save screen's,
// as the leave prompt's is (effect_1g.cpp, yes_no_layout.cpp): the first answer
// three spaces further left, the second where it was, the hand two units left
// of each.
unsigned char g_trade_confirm_layout = 0;

// original 0x593E60 (ItemTrade_RunSteps[2]): the yes / no. Left or right
// (Input_AutoRepeat of 0xA000) flips the hand 0x6BE08F, sound 0x100. Else
// cancel: sound 0x106, the step down; confirm on no: the same; confirm on
// yes: sound 0x105, the pick's bit 6 cleared, Inventory_Add(the entry's
// category, item, the quantity) (a fourth word 0 pushed), 0x594D90 (the
// ingredients taken), the quantity 1 and step 0. Then the frame; at step 2
// the item's name (Item_NamePtr, 16 bytes) into Text_Records, the two
// prompts (the words at 0x80360C, 0x803614) at (0x18, 0x14) and (0xDE, 0x14),
// the hand at (0xDC + 36 * the answer, 0x16); the list 0x594410(1); at any
// step but 0 a tail jump to 0x594AD0.
extern "C" void __cdecl ItemTrade_Confirm(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xA000u);
    if (moved & 0xA000u) {
        Effect1g_TradeAnswer = static_cast<unsigned char>(Effect1g_TradeAnswer ^ 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else {
        const std::uint16_t pressed = Input_Pressed;
        if (Field_CancelButtons & pressed) {
            SH_CALL(Sound_PlayEffect)(0x106);
            Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep - 1);
        } else if (Field_ConfirmButtons & pressed) {
            if (Effect1g_TradeAnswer != 0) {
                SH_CALL(Sound_PlayEffect)(0x106);
                Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep - 1);
            } else {
                SH_CALL(Sound_PlayEffect)(0x105);
                const auto pick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) & 0xBF);
                Effect1g_TradePick = pick;
                const U record = TradeRecord(S8FieldE2(pick));
                SH_CALL(Inventory_Add)(BFieldE2(record + 1), BFieldE2(record), static_cast<unsigned char>(Effect1g_TradeQuantity));
                SH_AT(void (__cdecl*)(), field_e2::at::kTradeTake.v)();
                Effect1g_TradeQuantity = 1;
                Effect1g_TradeStep = 0;
            }
        }
    }
    TradeBox();
    if (Effect1g_TradeStep == 2) {
        const U record = TradeRecord(static_cast<int>(static_cast<unsigned char>(Effect1g_TradePick) & 0xFu));
        const unsigned char* const name = SH_CALL(Item_NamePtr)(BFieldE2(record + 1), BFieldE2(record));
        std::memcpy(move_script::At(::bof3::Img(field_e2::at::kTextRecords)), name, 0x10);
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, PoolFieldE2(::bof3::Img(field_e2::at::kPoolWordName)));
        if (g_trade_confirm_layout) {
            const ConfirmAnswers a = ConfirmLayout(PoolFieldE2(::bof3::Img(field_e2::at::kPoolWordAsk)));
            SH_CALL(Text_DrawAt)(a.x, 0x14, 0, 0xFF, a.line);
            SH_CALL(Menu_DrawHand)(a.stop[Effect1g_TradeAnswer != 0 ? 1 : 0], 0x16, 0);
        } else {
            SH_CALL(Text_DrawAt)(0xDE, 0x14, 0, 0xFF, PoolFieldE2(::bof3::Img(field_e2::at::kPoolWordAsk)));
            SH_CALL(Menu_DrawHand)(static_cast<int>(Effect1g_TradeAnswer * 36u + 0xDCu), 0x16, 0);
        }
    }
    TradeList(1);
    if (Effect1g_TradeStep == 0) return;
    SH_AT(void (__cdecl*)(), field_e2::at::kTradeCount.v)();
}

// original 0x594060 (PSX 0x800F5AE0; ItemTrade_RunSteps[3], hidden in Sprite_ClutWord's
// catalog extent; PSX twin 0x800F5AE0): the step ItemTrade_PickItem sets when
// the item held is 99 or more. The frame; then with a confirm or cancel button
// pressed: the quantity 1 (before the sound), sound 0x106, the step 0, the
// pick's bit 7 cleared, the list ItemTrade_DrawList(0), done. Else the notice
// (MessagePools + the word at 0x80361A) at (0x18, 0x14), the list with its
// flag 1, and a tail jump to ItemTrade_DrawCount.
extern "C" void __cdecl ItemTrade_FullMessage(void) {
    TradeBoxEffect1g();
    if (static_cast<std::uint16_t>(Field_CancelButtons | Field_ConfirmButtons) & Input_Pressed) {
        Effect1g_TradeQuantity = 1;
        SH_CALL(Sound_PlayEffect)(0x106);
        const auto pick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) & 0x7F);
        Effect1g_TradeStep = 0;
        Effect1g_TradePick = pick;
        SH_CALL(ItemTrade_DrawList)(0);
        return;
    }
    SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, Pool(::bof3::Img(effect_1g::at::kPoolWordFull.v)));
    SH_CALL(ItemTrade_DrawList)(1);
    SH_CALL(ItemTrade_DrawCount)();
}

// original 0x5940F0 (PSX 0x800F5BAC; ItemTrade_States[2], hidden in Sprite_ClutWord's catalog
// extent; PSX twin 0x800F5BAC): the leave state - a tail jump through
// ItemTrade_LeaveSteps 0x66A494 by the step byte 0x93985E, unbounded, as the
// original (past the two entries ours aborts where the table runs into bytes).
extern "C" void __cdecl ItemTrade_Leave(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAt(::bof3::Img(ItemTrade_LeaveSteps_addr), Effect1g_TradeStep, "ItemTrade_Leave")))();
}

// original 0x594100 (ItemTrade_LeaveSteps[0], hidden in Sprite_ClutWord's
// catalog extent; PSX twin 0x800F5BE8): the leave question. Input_AutoRepeat of
// the pressed word's 0xA000; a move (its bits 13 or 15) flips the hand
// 0x6BE08F, sound 0x100. Else: cancel - the hand 1, sound 0x106, the pick's
// bit 7 cleared and the state 0x93985C down (both read after the sound); then,
// Input_Pressed read again, confirm - on yes (the hand 0) Transition_Start(0)
// and the step up (read after it), on no sound 0x104, bit 7 cleared, the state
// down. Both tests run in one frame when both buttons are down. Then the
// prompt (LeavePrompt, its last draw a tail jump in the original).
extern "C" void __cdecl ItemTrade_LeaveAsk(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xA000u);
    if ((moved >> 8) & 0xA0u) {
        Effect1g_TradeAnswer = static_cast<unsigned char>(Effect1g_TradeAnswer ^ 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else {
        if (Field_CancelButtons & Input_Pressed) {
            Effect1g_TradeAnswer = 1;
            SH_CALL(Sound_PlayEffect)(0x106);
            const auto pick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) & 0x7F);
            const auto state = static_cast<unsigned char>(Effect1g_TradeState - 1);
            Effect1g_TradePick = pick;
            Effect1g_TradeState = state;
        }
        if (Field_ConfirmButtons & Input_Pressed) {
            if (Effect1g_TradeAnswer == 0) {
                SH_CALL(Transition_Start)(0);
                Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep + 1);
            } else {
                SH_CALL(Sound_PlayEffect)(0x104);
                const auto pick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) & 0x7F);
                const auto state = static_cast<unsigned char>(Effect1g_TradeState - 1);
                Effect1g_TradePick = pick;
                Effect1g_TradeState = state;
            }
        }
    }
    LeavePrompt();
}

// original 0x594240 (ItemTrade_LeaveSteps[1], hidden in Sprite_ClutWord's
// catalog extent; PSX twin 0x800F5DA8): once Transition_Busy is 0 (the
// fade Transition_Start began), Game_Step = 1 and nothing drawn; until then
// the prompt as ItemTrade_LeaveAsk draws it.
extern "C" void __cdecl ItemTrade_LeaveWait(void) {
    if (Transition_Busy == 0) {
        Game_Step = 1;
        return;
    }
    LeavePrompt();
}

// original 0x5942C0 (PSX twin 0x800F5E5C): the screen's backdrop. A RECT in
// the packet pool - (32 * (Frame_Counter >> 4 & 3), 0x80, 0x20, 0x20) - as the
// texture window of a draw mode (page 0x95) at the next packet, committed to
// slot 7 (0xC); two POLY_FT4s side by side, x 0..0xA0 and 0xA0..0x140, y 0 to
// 240.0 (floats), u 0..0xA0, v 0..0xF0, CLUT 0x7A80, page 0x95, shade 0x40,
// each committed to slot 7 (0x48); then a draw mode whose texture window is
// the RECT (0, 0, 0x100, 0x100), committed to slot 7. The quads are built at
// Gfx_PacketNext as read before Gpu_SetPolyFT4.
//
// Not as the original under a wide picture (DIV-0041, docs/widescreen.md
// section 3d): the left quad starts at 0 - columns and the right one ends at
// 320 + columns, each with its u range grown by the same columns, so the
// pattern carries on into the bands - more tiles, not a stretch
// (ItemTrade_BackdropSpan). At 21:9 the grown halves would pass u 255, so the
// original's two halves are drawn as they are with a band quad each side
// after them (ItemTrade_BackdropQuads, docs/ultrawide-frame.md). Off, and while any self-test runs
// (Widescreen_Fill() is 0 until InjectAll arms it), the original's quads.
extern "C" void __cdecl ItemTrade_DrawBackground(void) {
    unsigned char* rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    move_script::SetWord(rect, ((Frame_Counter >> 4) & 3u) << 5);
    move_script::SetWord(rect + 2, 0x80);
    move_script::SetWord(rect + 6, 0x20);
    move_script::SetWord(rect + 4, 0x20);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, static_cast<unsigned long>(Key(rect)));
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
    const unsigned columns = Widescreen_Fill();
    for (unsigned quad = 0, quads = effect_1g::ItemTrade_BackdropQuads(columns); quad < quads; ++quad) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        const effect_1g::BackdropSpan s = effect_1g::ItemTrade_BackdropSpan(quad, columns);
        const float x0 = s.x0, x1 = s.x1, y1 = 240.0f;
        PutFloat(prim + 8, x0);
        move_script::SetWord(prim + 0x26, 0x95);
        move_script::SetWord(prim + 0x16, 0x7A80);
        PutFloat(prim + 0xC, 0.0f);
        PutFloat(prim + 0x1C, 0.0f);
        PutFloat(prim + 0x18, x1);
        PutFloat(prim + 0x2C, y1);
        PutFloat(prim + 0x28, x0);
        PutFloat(prim + 0x38, x1);
        PutFloat(prim + 0x3C, y1);
        prim[0x14] = s.u0;
        prim[0x15] = 0;
        prim[0x24] = s.u1;
        prim[0x25] = 0;
        prim[0x34] = s.u0;
        prim[0x35] = 0xF0;
        prim[0x44] = s.u1;
        prim[0x45] = 0xF0;
        prim[4] = 0x40;
        prim[5] = 0x40;
        prim[6] = 0x40;
        SH_CALL(Gfx_CommitPrim)(7, 0x48);
    }
    rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    move_script::SetWord(rect, 0);
    move_script::SetWord(rect + 2, 0);
    move_script::SetWord(rect + 6, 0x100);
    move_script::SetWord(rect + 4, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, static_cast<unsigned long>(Key(rect)));
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
}

// original 0x594410 (PSX twin 0x800F6084): the list of the row's entries.
// ItemTrade_DrawListFrame(0x14, 0x38, flag); the heading (MessagePools + the
// word at 0x80360E) at (0x36, 0x3E) in colour 7 * the flag's byte. Then for
// each entry i below the row count 0x6BE08D (read again every pass), y = 13 i:
// its name (Item_NamePtr(category, item)) and ItemTrade_Lacks(i, 1); with the
// flag's byte set the name at (0x25, y + 0x51) in colour 7 and the icon at
// (0x1C, y + 0x54) dim 1. Else on the picked entry (s8 0x6BE08C): unless an
// ingredient is short, the name at (0x25, y + 0x51) in colour 7 and the icon at
// (0x1C, y + 0x54) dim 1 - a shadow - and then, short or not, the name at
// (0x25, y + 0x4F) in colour 7 * the answer's byte and the icon at (0x1C,
// y + 0x52) dimmed by it; on every other entry the name at (0x25, y + 0x51) in
// colour 7 * the answer and the icon at (0x1C, y + 0x54) dimmed by it. The
// count of characters is Text_CharCount's; each record is read again for the
// icon (the row byte after the calls).
extern "C" void __cdecl ItemTrade_DrawList(unsigned flag) {
    SH_CALL(ItemTrade_DrawListFrame)(0x14, 0x38, flag);
    const auto on = static_cast<unsigned char>(flag);
    SH_CALL(Text_DrawAt)(0x36, 0x3E, static_cast<unsigned char>(on * 7u), 0xFF, Pool(::bof3::Img(effect_1g::at::kPoolWordList.v)));
    for (U i = 0; i < Effect1g_TradeRowCount; ++i) {
        U record = Record(i);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(B(record + 1), B(record));
        const unsigned char lacks = SH_CALL(ItemTrade_Lacks)(i, 1);
        const U y = 13u * i;
        if (on != 0) {
            SH_CALL(Text_DrawAt)(0x25, static_cast<int>(y + 0x51), 7, SH_CALL(Text_CharCount)(name), name);
            record = Record(i);
            SH_CALL(Item_DrawIcon)(0x1C, static_cast<int>(y + 0x54), B(record), B(record + 1), 1);
            continue;
        }
        if (static_cast<int>(i) == Effect1g_TradePick) {
            if (lacks == 0) {
                SH_CALL(Text_DrawAt)(0x25, static_cast<int>(y + 0x51), 7, SH_CALL(Text_CharCount)(name), name);
                record = Record(i);
                SH_CALL(Item_DrawIcon)(0x1C, static_cast<int>(y + 0x54), B(record), B(record + 1), 1);
            }
            SH_CALL(Text_DrawAt)(0x25, static_cast<int>(y + 0x4F), static_cast<unsigned char>(lacks * 7u), SH_CALL(Text_CharCount)(name), name);
            record = Record(i);
            SH_CALL(Item_DrawIcon)(0x1C, static_cast<int>(y + 0x52), B(record), B(record + 1), lacks);
        } else {
            SH_CALL(Text_DrawAt)(0x25, static_cast<int>(y + 0x51), static_cast<unsigned char>(lacks * 7u), SH_CALL(Text_CharCount)(name), name);
            record = Record(i);
            SH_CALL(Item_DrawIcon)(0x1C, static_cast<int>(y + 0x54), B(record), B(record + 1), lacks);
        }
    }
}

// original 0x594610 (PSX twin 0x800F63E4): the list's frame at (x, y).
// Menu_DrawBox(x, y + 1, 0x8C, 0xA0, the flag's byte | 0x80, the style byte
// 0x903A5A); E1F's pieces - the mode 9, the top row (0x25 at x, ten 0x26 from
// x + 0x20 by 8, 0x27 at x + 0x70, 0x28 at x + 0x88, all at y), E1B's two sides
// (x and x + 0x88, y + 0x18, length 0x84, pieces 0 and 1), the bottom row (0x30
// at x, sixteen 0x31 from x + 8 by 8, 0x32 at x + 0x88, all at y + 0x9C).
extern "C" void __cdecl ItemTrade_DrawListFrame(int x, int y, unsigned flag) {
    const U xs = static_cast<U>(x), ys = static_cast<U>(y);
    SH_CALL(Menu_DrawBox)(x, static_cast<int>(ys + 1), 0x8C, 0xA0, static_cast<unsigned char>(flag | 0x80), static_cast<unsigned char>(Config_WindowColour));
    PieceMode(9);
    Piece(0x25, xs, ys);
    for (U i = 0; i < 10; ++i) Piece(0x26, xs + 8 * i + 0x20, ys);
    Piece(0x27, xs + 0x70, ys);
    Piece(0x28, xs + 0x88, ys);
    Quad(xs, ys + 0x18, 0x84, 0);
    Quad(xs + 0x88, ys + 0x18, 0x84, 1);
    Piece(0x30, xs, ys + 0x9C);
    for (U i = 0; i < 16; ++i) Piece(0x31, xs + 8 * i + 8, ys + 0x9C);
    Piece(0x32, xs + 0x88, ys + 0x9C);
}

// original 0x594700 (PSX twin 0x800F65C0): whether an ingredient of entry k (a
// byte) falls short for `quantity` (a byte). For each of the record's three
// ingredient bytes +2.. up to an 0xFF: Inventory_Count(0, the byte + 0x38, 0)
// (a consumable held), its low word below the ingredient's count +5.. times
// the quantity (signed) answers 1; all three met (or an 0xFF first) answers 0.
// The record is read again (the row byte) before each call, the ingredient
// byte of the next pass from the record read before the call - as the
// original's registers hold it.
extern "C" unsigned char __cdecl ItemTrade_Lacks(unsigned entry, unsigned quantity) {
    const U k = entry & 0xFF, q = quantity & 0xFF;
    U record = Record(k);
    for (U i = 0; i < 3; ++i) {
        const unsigned char ingredient = B(record + 2 + i);
        if (ingredient == 0xFF) return 0;
        record = Record(k);
        const U held = SH_CALL(Inventory_Count)(0, static_cast<unsigned char>(ingredient + effect_1g::at::kIngredientBase), 0) & 0xFFFFu;
        if (static_cast<std::int32_t>(held) < static_cast<std::int32_t>(B(record + 5 + i) * q)) return 1;
    }
    return 0;
}

// original 0x594790 (PSX twin 0x800F66EC): the entries in the screen's row -
// its bytes from the first up to the first 0xFF, ten at most.
extern "C" unsigned char __cdecl ItemTrade_RowCount(void) {
    const U row = ::bof3::Img(effect_1g::at::kTradeIndex.v) + 10u * AreaEntry_PendingFlags;
    unsigned char n = 0;
    while (B(row + n) != 0xFF) {
        ++n;
        if (n >= 10) break;
    }
    return n;
}

// original 0x5947D0 (PSX twin 0x800F6748): the ingredients of the picked entry
// (its low nibble, read once). Two frames ItemTrade_DrawNeedsFrame at (0xA8,
// 0x40) and (0xA8, 0x8C), each with its heading (the words at 0x803610 and
// 0x803612) at (0xDA, 0x47) and (0xDA, 0x93). Unless the pick has bit 7 (read
// after those), for each ingredient i up to an 0xFF, y = 13 i (the record read
// again - the row byte - before each use): its name (Item_NamePtr(0, the byte
// + 0x38)) at (0xAD, y + 0x58); the count needed times the s8 quantity through
// Crt_sprintf at (0x110, y + 0x5C) (Text_DrawFont8); the count held
// (Inventory_Count(0, the byte + 0x38, 0), its byte) against the count
// needed: below it, the name at (0xAD, y + 0xA4) and the held count at (0x110,
// y + 0xA8) in colour 7; else in colour 0.
extern "C" void __cdecl ItemTrade_DrawNeeds(void) {
    const U k = static_cast<unsigned char>(Effect1g_TradePick) & 0xFu;
    SH_CALL(ItemTrade_DrawNeedsFrame)(0xA8, 0x40);
    SH_CALL(Text_DrawAt)(0xDA, 0x47, 0, 0xFF, Pool(::bof3::Img(effect_1g::at::kPoolWordNeeds.v)));
    SH_CALL(ItemTrade_DrawNeedsFrame)(0xA8, 0x8C);
    SH_CALL(Text_DrawAt)(0xDA, 0x93, 0, 0xFF, Pool(::bof3::Img(effect_1g::at::kPoolWordHeld.v)));
    if (static_cast<unsigned char>(Effect1g_TradePick) & 0x80) return;
    for (U i = 0; i < 3; ++i) {
        const unsigned char ingredient = B(Record(k) + 2 + i);
        if (ingredient == 0xFF) return;
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, static_cast<unsigned char>(ingredient + effect_1g::at::kIngredientBase));
        const U y = 13u * i;
        SH_CALL(Text_DrawAt)(0xAD, static_cast<int>(y + 0x58), 0, SH_CALL(Text_CharCount)(name), name);
        const std::int32_t need = static_cast<std::int32_t>(B(Record(k) + 5 + i)) * Effect1g_TradeQuantity;
        SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), need);
        SH_CALL(Text_DrawFont8)(0x110, static_cast<int>(y + 0x5C), 0, reinterpret_cast<const unsigned char*>(NumberText()));
        const auto held = static_cast<unsigned char>(
            SH_CALL(Inventory_Count)(0, static_cast<unsigned char>(B(Record(k) + 2 + i) + effect_1g::at::kIngredientBase), 0));
        const int colour = held < B(Record(k) + 5 + i) ? 7 : 0;
        SH_CALL(Text_DrawAt)(0xAD, static_cast<int>(y + 0xA4), colour, SH_CALL(Text_CharCount)(name), name);
        SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), static_cast<int>(held));
        SH_CALL(Text_DrawFont8)(0x110, static_cast<int>(y + 0xA8), colour, reinterpret_cast<const unsigned char*>(NumberText()));
    }
}

// original 0x5949F0 (PSX twin 0x800F6A9C): an ingredient window's frame at (x,
// y). Menu_DrawBox(x, y + 1, 0x84, 0x42, 0x80, the style byte); E1F's pieces -
// the mode 9, the top row (0x25 at x, nine 0x26 from x + 0x20 by 8, 0x27 at
// x + 0x68, 0x28 at x + 0x80, at y), E1B's sides (x and x + 0x80, y + 0x18,
// length 0x28, pieces 0 and 1), the bottom row (0x30 at x, fifteen 0x31 from
// x + 8 by 8, 0x32 at x + 0x80, at y + 0x40).
extern "C" void __cdecl ItemTrade_DrawNeedsFrame(int x, int y) {
    const U xs = static_cast<U>(x), ys = static_cast<U>(y);
    SH_CALL(Menu_DrawBox)(x, static_cast<int>(ys + 1), 0x84, 0x42, 0x80, static_cast<unsigned char>(Config_WindowColour));
    PieceMode(9);
    Piece(0x25, xs, ys);
    for (U i = 0; i < 9; ++i) Piece(0x26, xs + 8 * i + 0x20, ys);
    Piece(0x27, xs + 0x68, ys);
    Piece(0x28, xs + 0x80, ys);
    Quad(xs, ys + 0x18, 0x28, 0);
    Quad(xs + 0x80, ys + 0x18, 0x28, 1);
    Piece(0x30, xs, ys + 0x40);
    for (U i = 0; i < 15; ++i) Piece(0x31, xs + 8 * i + 8, ys + 0x40);
    Piece(0x32, xs + 0x80, ys + 0x40);
}

// original 0x594AD0 (PSX twin 0x800F6C74): the count window of the picked entry
// (its low nibble, read once; the record read again - the row byte - before
// each use). ItemTrade_DrawCountFrame(0x14, 0x64); the entry's name
// (Item_NamePtr(category, item)) at (0x21, 0x68); the s8 quantity through
// Crt_sprintf at (0x84, 0x6C) (Text_DrawFont8); its icon at (0x18, 0x6C), not
// dim; the label (the word at 0x803616) at (0x21, 0x7C) and
// Inventory_Count(category, item, 0)'s low word at (0x84, 0x80); the label
// (the word at 0x803618) at (0x21, 0x88) and Inventory_Count(category, item,
// 1)'s at (0x84, 0x8C). Colour 0 throughout.
extern "C" void __cdecl ItemTrade_DrawCount(void) {
    const U k = static_cast<unsigned char>(Effect1g_TradePick) & 0xFu;
    SH_CALL(ItemTrade_DrawCountFrame)(0x14, 0x64);
    U record = Record(k);
    const unsigned char* const name = SH_CALL(Item_NamePtr)(B(record + 1), B(record));
    SH_CALL(Text_DrawAt)(0x21, 0x68, 0, SH_CALL(Text_CharCount)(name), name);
    SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), Effect1g_TradeQuantity);
    SH_CALL(Text_DrawFont8)(0x84, 0x6C, 0, reinterpret_cast<const unsigned char*>(NumberText()));
    record = Record(k);
    SH_CALL(Item_DrawIcon)(0x18, 0x6C, B(record), B(record + 1), 0);
    SH_CALL(Text_DrawAt)(0x21, 0x7C, 0, 0xFF, Pool(::bof3::Img(effect_1g::at::kPoolWordBag.v)));
    record = Record(k);
    U held = SH_CALL(Inventory_Count)(B(record + 1), B(record), 0) & 0xFFFFu;
    SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), static_cast<int>(held));
    SH_CALL(Text_DrawFont8)(0x84, 0x80, 0, reinterpret_cast<const unsigned char*>(NumberText()));
    SH_CALL(Text_DrawAt)(0x21, 0x88, 0, 0xFF, Pool(::bof3::Img(effect_1g::at::kPoolWordWorn.v)));
    record = Record(k);
    held = SH_CALL(Inventory_Count)(B(record + 1), B(record), 1) & 0xFFFFu;
    SH_CALL(Crt_sprintf)(NumberText(), NumberFormat(), static_cast<int>(held));
    SH_CALL(Text_DrawFont8)(0x84, 0x8C, 0, reinterpret_cast<const unsigned char*>(NumberText()));
}

// original 0x594C90 (PSX twin 0x800F6EF8): the count window's frame at (x, y).
// Menu_DrawBox(x, y, 0x88, 0x36, 0x80, the style byte); E1F's pieces - the
// mode 8, the top row (0x33 at x, sixteen 0x34 from x + 8 by 8, 0x35 at
// x + 0x84, at y), E1B's sides (x and x + 0x84, y + 8, length 0x2C, pieces 2
// and 3), the bottom row (0x36 at x, sixteen 0x37 from x + 8 by 8, 0x38 at
// x + 0x84, at y + 0x34).
extern "C" void __cdecl ItemTrade_DrawCountFrame(int x, int y) {
    const U xs = static_cast<U>(x), ys = static_cast<U>(y);
    SH_CALL(Menu_DrawBox)(x, y, 0x88, 0x36, 0x80, static_cast<unsigned char>(Config_WindowColour));
    PieceMode(8);
    Piece(0x33, xs, ys);
    for (U i = 0; i < 16; ++i) Piece(0x34, xs + 8 * i + 8, ys);
    Piece(0x35, xs + 0x84, ys);
    Quad(xs, ys + 8, 0x2C, 2);
    Quad(xs + 0x84, ys + 8, 0x2C, 3);
    Piece(0x36, xs, ys + 0x34);
    for (U i = 0; i < 16; ++i) Piece(0x37, xs + 8 * i + 8, ys + 0x34);
    Piece(0x38, xs + 0x84, ys + 0x34);
}

// original 0x594D50 (PSX twin 0x800F70AC): an item's 8 x 8 icon at (x, y).
// Nothing when the item's byte is 0; else Item_IconKind(category, item) (a
// third word, `dim`, pushed and unread), its byte through Item_IconByKind
// 0x66A49C (16 entries: the kind is a nibble - past them ours aborts), and
// Menu_DrawIcon8(x, y, that byte, dim). Called by the trade screen's draws and
// by E1B's 0x469210 (eight sites).
extern "C" void __cdecl Item_DrawIcon(int x, int y, unsigned item, unsigned category, int dim) {
    if ((item & 0xFF) == 0) return;
    const U kind = SH_CALL(Item_IconKind)(category, item) & 0xFFu;
    if (kind >= effect_1g::at::kIconKinds)
        bof3::Fatal("Item_DrawIcon: Item_IconKind answered %u, past Item_IconByKind's %u entries", (unsigned)kind, effect_1g::at::kIconKinds);
    SH_CALL(Menu_DrawIcon8)(x, y, Item_IconByKind[kind], dim);
}

// original 0x594D90: called by ItemTrade_Confirm - the trade's needs taken: for
// the trade kind 0x66AD10[the s8 0x6BE08C + 10 * spot 0x905B88], up to three
// item bytes of 0x66AB5A + 8 kind (0xFF ends) each removed from category 0 as
// item byte + 0x38, count (u16) the s8 product of its count byte (+3) and the
// s8 0x6BE08E - Inventory_Remove(0, item, count). The removal's kind is looked
// up again with 0x6BE08C's low nibble.
extern "C" void __cdecl ItemTrade_TakeNeeds(void) {
    for (U k = 0; k < 3; ++k) {
        const int c = Effect1g_TradePick;
        const U row = 10u * AreaEntry_PendingFlags;
        const U kind = BRest2f(::bof3::Img(kTradeKinds.v) + static_cast<U>(c) + row);
        if (BRest2f(::bof3::Img(kTradeNeeds.v) + 8 * kind + k) == 0xFF) return;
        const auto q = static_cast<signed char>(BRest2f(::bof3::Img(kTradeNeeds.v) + 8 * kind + 3 + k));
        const U count = static_cast<U>(static_cast<std::uint16_t>(q * Effect1g_TradeQuantity));
        const U kind2 = BRest2f(::bof3::Img(kTradeKinds.v) + (static_cast<U>(c) & 0xF) + row);
        const U item = static_cast<unsigned char>(BRest2f(::bof3::Img(kTradeNeeds.v) + k + 8 * kind2) + 0x38);
        SH_CALL(Inventory_Remove)(0, item, count);
    }
}

// original 0x594E00: the single funnel every area change goes through (PSX
// Field_ChangeArea 0x801A0A30, the sibling's docs/loader_records/AREA.md
// section 4). Stashes the four arguments, classifies the pending area, picks
// the track the new area wants, and asks the field for mode 5.
//
// As the original has it: the area is stored as a word and read back out of
// memory for Area_PickMusic (not kept in a register); x and z are arithmetic
// shifts of the whole 32-bit argument, so their integer parts reach the zone
// lookup, which then keeps only the low byte of each; the flags argument is
// stored as a byte. Where the PSX keeps all five cells in one block at
// 0x80143F10..1F, the port scattered them.
extern "C" void __cdecl Field_ChangeArea(unsigned area, int x, int z, unsigned flags) {
    AreaW0b_Place = static_cast<std::uint16_t>(area);
    window_task::at::kPendingX = x;
    AreaEntry_PendingZ = z;
    AreaEntry_PendingFlags = static_cast<unsigned char>(flags);
    window_task::g.classify();
    window_task::g.pick_music(static_cast<unsigned>(x >> 16), static_cast<unsigned>(z >> 16), AreaW0b_Place);
    Field_Request = 5;
}

// original 0x594E60: entering an area, once per area change from
// GameMode_Enter (PSX 0x801A0AA8, callers pairing). docs/mode-flow.md
// section 4 has it step by step.
//
// As the original has it: flag 0x80 of the entry flags sets or clears bit 15
// of Field_ScriptFlags2 at once and is kept (the original writes it over the
// flags' own argument slot) for the end; the area's DAT is loaded only when
// the area number differs AND Field_Request is 5; the flags go on whole to
// Field_PartySetUp; Area_Descriptors[Game_AreaNumber] is read afresh after
// every call; an area whose descriptor has no colour matrix at +0x38 gets
// the default one at 0x6BE090 and the light pair at 0x663B20; the scenario
// chapter steps by one with 0x10 skipped (to 0x11) when Field_StatusBits has
// bit 7; the descriptor's +0x40, when not null, is called with nothing.
extern "C" void __cdecl Area_Enter(unsigned area, int x, int z, unsigned flags) {
    const bool drop_in = (flags & 0x80) != 0;
    if (drop_in) Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 | 0x8000);
    else Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0x7FFF);
    mode_flow::g.effect_clear();
    mode_flow::g.tint_reset();
    const std::uint16_t previous = Game_AreaNumber;
    const unsigned char zone = Cond_ByteFD;
    const long kind2_x = Field_Kind2X, kind2_z = Field_Kind2Z;
    AreaW2c_LastArea = previous;
    AreaW1a_EntryZone = zone;
    mode_flow::SetLong(mode_flow::At(::bof3::Img(MoveScript_TintRecords_addr) + 0x220), kind2_z);
    mode_flow::SetLong(mode_flow::At(::bof3::Img(MoveScript_TintRecords_addr) + 0x21c), kind2_x);
    const unsigned char track = AreaW1f_MusicCurrent;
    if (track != 0xFF) mode_flow::g.music_play(track, 8);
    else Music_Track = 0xFF;
    if (Game_AreaNumber != static_cast<std::uint16_t>(area) && Field_Request == 5) {
        Game_AreaNumber = static_cast<std::uint16_t>(area);
        mode_flow::g.load_dat(static_cast<int>((area & 0xFFFF) + 3));
        while (mode_flow::g.load_done() == 0) mode_flow::g.sleep(1);
        AreaDataReload_OnLoad(area & 0xFFFF);   // DIV-0182: the area's own .data as the disc holds it
        Field_InputFlags = Area_Descriptors[Game_AreaNumber][0x32];
        mode_flow::g.clut_restore();
        Gfx_ClutStripDirty = 1;
    }
    mode_flow::g.party_set_up(x, z, flags);
    Cond_ByteFD = mode_flow::g.zone_id(mode_flow::Word(mode_flow::At(::bof3::Img(mode_flow::at::kKind2XHigh))), mode_flow::Word(mode_flow::At(::bof3::Img(mode_flow::at::kKind2ZHigh))));
    mode_flow::g.view_reset();
    const auto colour = static_cast<std::uint32_t>(mode_flow::Long(Area_Descriptors[Game_AreaNumber] + 0x38));
    if (colour != 0) {
        mode_flow::g.colour_matrix(Fn<const unsigned long*>(colour));
        const unsigned char* const own = Fn<const unsigned char*>(
            static_cast<std::uint32_t>(mode_flow::Long(Area_Descriptors[Game_AreaNumber] + 0x38)));
        mode_flow::SetLong(mode_flow::At(::bof3::Img(0x903598)), mode_flow::Long(own + 0x20));   // Light_Angles
        mode_flow::SetLong(mode_flow::At(::bof3::Img(0x90359C)), mode_flow::Long(own + 0x24));
    } else {
        mode_flow::g.colour_matrix(Fn<const unsigned long*>(::bof3::Img(mode_flow::at::kDefaultColour)));
        mode_flow::SetLong(mode_flow::At(::bof3::Img(0x903598)), static_cast<std::int32_t>(ModeFlow_DefaultLight));
        mode_flow::SetLong(mode_flow::At(::bof3::Img(0x90359C)), mode_flow::Long(mode_flow::At(::bof3::Img(ModeFlow_DefaultLight_addr) + 4)));
    }
    if (Field_StatusBits & 0x80) {
        auto chapter = static_cast<unsigned char>(Cond_ByteFA + 1);
        Cond_ByteFA = static_cast<signed char>(chapter);
        if (chapter == 0x10) {
            chapter = 0x11;
            Cond_ByteFA = 0x11;
        }
        mode_flow::g.scenario_start(chapter);
    }
    mode_flow::g.run_placement(Fn<const unsigned char*>(static_cast<std::uint32_t>(mode_flow::Long(Area_Descriptors[Game_AreaNumber]))));
    const auto hook = static_cast<std::uint32_t>(mode_flow::Long(Area_Descriptors[Game_AreaNumber] + 0x40));
    if (hook != 0) Fn<mode_flow::Handler>(hook)();
    mode_flow::g.flags_clear(mode_flow::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C);
    mode_flow::At(::bof3::Img(0x8034E2))[0] = 1;   // Field_StatusBits + 1
    mode_flow::g.mode_dispatch();
    mode_flow::g.first_frame();

    const std::uint16_t bits = Field_ScriptFlags2;
    if (bits & 0x8000) {
        if (drop_in) {
            EventLeader_PendingFlag = 0;
            mode_flow::g.place_party(flags & ~0x80u);
            Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xFFEF);
            mode_flow::g.members_frame();
        }
    } else if (!(Field_ScriptFlags & 0x800)) {
        bool walked = true;
        if (bits & 0x80) mode_flow::g.entry_walk(1, x, z);
        else if (bits & 0x100) mode_flow::g.entry_walk(2, x, z);
        else if (bits & 0x200) mode_flow::g.entry_walk(3, x, z);
        else if (Field_MemberCount > 1) {
            mode_flow::g.entry_walk(0, x, z);
            if (!(Field_ScriptFlags & 0x800) && !(Field_ScriptFlags2 & 0x8000))
                mode_flow::g.test_fb(static_cast<short>(mode_flow::Word(mode_flow::At(::bof3::Img(mode_flow::at::kKind2XHigh)))), static_cast<short>(mode_flow::Word(mode_flow::At(::bof3::Img(mode_flow::at::kKind2ZHigh)))));
        } else {
            walked = false;
        }
        if (walked) {
            const unsigned char members = Field_MemberCount;
            if (members > 1) {
                auto v = static_cast<std::uint16_t>(Field_ScriptFlags2 | 0x12);
                if (members == 3) v = static_cast<std::uint16_t>(v | 4);
                Field_ScriptFlags2 = static_cast<std::uint16_t>(v & 0xEE7F);
                return;
            }
        }
    }
    Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xEE7F);
}

// original 0x595160 (PSX 0x801A0F90, gap pairing): the party walks in from
// the area's edge - the walk on, its direction, a zero, the point (x, z);
// with a direction bit 3 of Field_ScriptFlags2 set; each of Field_MemberCount
// members' two walk bytes: the direction's pose from 0x66ADBC, and bit 1.
//
// As the original has it: the direction is the argument's low byte; the pose
// table is indexed by it unchecked (callers pass 0..3); the member count is
// read once, as a byte.
extern "C" void __cdecl Area_EntryWalk(unsigned direction, int x, int z) {
    const auto dir = static_cast<unsigned char>(direction);
    if (dir != 0) Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 | 8);
    EventLeader_PendingX = x;
    const unsigned members = Field_MemberCount;
    EventLeader_PendingFlag = 1;
    mode_flow::At(::bof3::Img(EventLeader_PendingFlag_addr))[1] = dir;
    mode_flow::At(::bof3::Img(EventLeader_PendingFlag_addr))[2] = 0;
    EventLeader_PendingZ = z;
    if (members == 0) return;
    const unsigned char pose = mode_flow::At(::bof3::Img(mode_flow::at::kWalkPoses))[dir];
    for (unsigned i = 0; i < members; ++i) {
        unsigned char* const m = mode_flow::At(::bof3::Img(ObjTrio_addr) + 0x137 + i * 0x14C);
        m[0] = pose;
        m[1] = static_cast<unsigned char>(m[1] | 2);
    }
}

// original 0x5951D0 (PSX 0x801A1050, gap pairing; read, the same): the area's
// link at cell (x, z), made the pending area change. The descriptor's +0x20
// holds 12-byte links, +0x30 the index of the last: each link is a run of
// +9 cells from (+0, +1) along z when +8 is set, along x when it is not. The
// first link found - searched from the last link down and each run from its
// far end - gives the destination: area word +2, x and z words +4 / +6 and
// the flags byte +0xA; or, when +2 is 0xFFFF, the word +4 picks a list of
// 10-byte alternatives (descriptor +0x24's pointers, +0x28's counts), the
// first whose story flag Flags_Test(Cond_Flags + [6] * 8, [7]) holds or the
// one after the last when none does (area +0, x +2, z +4, flags +8). The
// area, (x & 0xFF80) << 8, (z & 0xFF80) << 8 and flags & 0x8F go where
// Field_ChangeArea puts them, bits 4-6 of the flags to 0x903851, then
// Area_ClassifyPending and Area_PickMusic of what is now pending - but not
// Field_Request 5: the caller decides whether to go. 0 when a link was
// found, 1 when none.
//
// As the original has it: x and z are compared as bytes, the run's start
// plus its index as an int (a run past 0xFF never matches there); the
// descriptor pointer is read once; Area_PickMusic's x and z are the pending
// dwords shifted arithmetically, read back after Area_ClassifyPending, and
// its area is the pending word (the original pushes it with a stale upper
// half, which Area_PickMusic masks). The result is `al` alone - six of the
// twelve call sites test al and the other six (GameMode_Enter among them)
// ignore it; the upper 24 bits are an address in the link list, or whatever
// Area_PickMusic left.
extern "C" unsigned char __cdecl Area_LinkAt(unsigned x_arg, unsigned z_arg) {
    const unsigned x = x_arg & 0xFFu, z = z_arg & 0xFFu;
    unsigned char* const descriptor = Descriptor();
    int n = descriptor[0x30];
    const unsigned char* link = Ptr(descriptor + 0x20) + n * 12;
    for (; n >= 0; --n, link -= 12) {
        for (int i = static_cast<int>(link[9]) - 1; i >= 0; --i) {
            if (link[8] != 0) {
                if (x == link[0] && z == link[1] + static_cast<unsigned>(i)) goto found;
            } else if (x == link[0] + static_cast<unsigned>(i) && z == link[1]) {
                goto found;
            }
        }
    }
    return 1;
found:
    unsigned char flags;
    if (move_script::Word(link + 2) != 0xFFFF) {
        AreaW0b_Place = move_script::Word(link + 2);
        kPendingX = static_cast<std::int32_t>((move_script::Word(link + 4) & 0xFF80u) << 8);
        AreaEntry_PendingZ = static_cast<std::int32_t>((move_script::Word(link + 6) & 0xFF80u) << 8);
        flags = link[0xA];
    } else {
        const unsigned index = move_script::Word(link + 4);
        const unsigned count = Ptr(descriptor + 0x28)[index];
        const unsigned char* choice = Ptr(Ptr(descriptor + 0x24) + index * 4);
        for (unsigned i = 0; i < count; ++i, choice += 10)
            if (area_entry::g.flags_test(Cond_Flags + choice[6] * 8u, choice[7]) != 0) break;
        AreaW0b_Place = move_script::Word(choice);
        kPendingX = static_cast<std::int32_t>((move_script::Word(choice + 2) & 0xFF80u) << 8);
        AreaEntry_PendingZ = static_cast<std::int32_t>((move_script::Word(choice + 4) & 0xFF80u) << 8);
        flags = choice[8];
    }
    AreaEntry_PendingFlags = static_cast<unsigned char>(flags & 0x8F);
    kLinkHigh = static_cast<unsigned char>((flags >> 4) & 7);
    area_entry::g.classify();
    area_entry::g.pick_music(static_cast<unsigned>(kPendingX >> 16), static_cast<unsigned>(static_cast<std::int32_t>(AreaEntry_PendingZ) >> 16),
                 AreaW0b_Place);
    return 0;
}

// original 0x595350 (PSX 0x801A12F0): 0x937F98 is 0xB when the pending area
// is one of the eleven at 0x66ADC0 - 0x10, 0x21, 0x2D, 0x41, 0x57, 0x58,
// 0x68, 0x73, 0x79, 0x97, 0x98 - and 1 when it is not. What reads it is not
// established (docs/window-task.md section 2).
//
// As the original has it: the "not found" store is under `cmp ecx, 0xB`, a
// test the loop can only leave true - eleven entries, the counter at eleven -
// so it is unconditional in effect. The scan stops at the FIRST match; every
// entry is distinct, so that is unobservable.
extern "C" void __cdecl Area_ClassifyPending(void) {
    const std::uint16_t pending = AreaW0b_Place;
    unsigned n = 0;
    for (std::uint32_t p = ::bof3::Img(window_task::at::kAreaKinds.v); p < ::bof3::Img(window_task::at::kAreaKindsEnd.v); p += 2, ++n) {
        if (window_task::Word(window_task::At(p)) == pending) {
            Area_EnterTransition = 0xB;
            return;
        }
    }
    if (n == 0xB) Area_EnterTransition = 1;
}

// original 0x595390 (PSX 0x801A1350): the zone byte +5 at (x, z) in the
// current area. Its one caller is 0x594E60, the area change's mode-5 step,
// which is still Capcom's.
//
// As the original has it: the area is read as a word with the upper half of
// the register left stale, which the lookup masks away; the result is `al`
// alone - the upper 24 bits of eax are the zone record's address, which no
// caller reads.
extern "C" unsigned char __cdecl Area_ZoneIdAt(unsigned x, unsigned z) {
    return window_task::g.zone_at(x, z, Game_AreaNumber)[5];
}

extern "C" void __cdecl Area_PickMusic(unsigned x, unsigned z, unsigned area) {
    if ((Field_ScriptFlags & 0x80u) != 0) {
        const bool stopping = Music_FadeStops != 0 && Music_FadeCount != 0;
        AreaW1f_MusicCurrent = g_keep_stopping_none && stopping ? 0xFF : Music_Track;
        return;
    }
    AreaW1f_MusicCurrent = Area_ZoneTrack(window_task::g.zone_at(x, z, area));
}

// original 0x595450 (PSX 0x801A1498): the first of area `area`'s 8-byte zone
// records whose box [r0, r2] x [r1, r3] holds (x, z) - Area_ZoneAt 0x52FFD0
// with the area as an argument instead of Game_AreaNumber, instruction for
// instruction otherwise.
//
// As the original has it: there is no end test - a list with no record that
// matches loops for ever. x and z are masked to a byte and the area to a
// word; the comparisons are 16-bit signed on zero-extended bytes, the same as
// unsigned.
//
// NOT the PSX Window_Task that symbols.toml guessed at here until 2026-09-22:
// pe_funcs.py had merged 0x595450..0x595C48 into one 0x7F9-byte "function"
// (the fourteen between are all pointer-reached), and the 613 instructions
// that guess rested on are those fourteen.
extern "C" const unsigned char* __cdecl Area_ZoneAtIn(unsigned x_arg, unsigned z_arg, unsigned area) {
    const unsigned x = x_arg & 0xFFu, z = z_arg & 0xFFu;
    const unsigned char* record = window_task::At(static_cast<std::uint32_t>(window_task::Long(window_task::At(::bof3::Img(Area_StateRecords_addr)) + (area & 0xFFFFu) * 4u)));   // a 32-bit word
    for (;; record += 8) {
        if (x < record[0] || z < record[1] || x > record[2] || z > record[3]) continue;
        return record;
    }
}

void ItemTrade_Inject() {
    BOF3_INJECT(ItemTrade_Dispatch);
    BOF3_INJECT(ItemTrade_Open);
    BOF3_INJECT(ItemTrade_OpenStart);
    BOF3_INJECT(ItemTrade_OpenWait);
    BOF3_INJECT(ItemTrade_Run);
    BOF3_INJECT(ItemTrade_PickItem);
    BOF3_INJECT(ItemTrade_PickCount);
    BOF3_INJECT(ItemTrade_Confirm);
    BOF3_INJECT(ItemTrade_FullMessage);
    BOF3_INJECT(ItemTrade_Leave);
    BOF3_INJECT(ItemTrade_LeaveAsk);
    BOF3_INJECT(ItemTrade_LeaveWait);
    BOF3_INJECT(ItemTrade_DrawBackground);
    BOF3_INJECT(ItemTrade_DrawList);
    BOF3_INJECT(ItemTrade_DrawListFrame);
    BOF3_INJECT(ItemTrade_Lacks);
    BOF3_INJECT(ItemTrade_RowCount);
    BOF3_INJECT(ItemTrade_DrawNeeds);
    BOF3_INJECT(ItemTrade_DrawNeedsFrame);
    BOF3_INJECT(ItemTrade_DrawCount);
    BOF3_INJECT(ItemTrade_DrawCountFrame);
    BOF3_INJECT(Item_DrawIcon);
    BOF3_INJECT(ItemTrade_TakeNeeds);
    BOF3_INJECT(Field_ChangeArea);
    BOF3_INJECT(Area_Enter);
    BOF3_INJECT(Area_EntryWalk);
    BOF3_INJECT(Area_LinkAt);
    BOF3_INJECT(Area_ClassifyPending);
    BOF3_INJECT(Area_ZoneIdAt);
    BOF3_INJECT(Area_PickMusic);
    BOF3_INJECT(Area_ZoneAtIn);
}

void ItemTradeFromFieldE2_Inject() {
    // DIVERGENCE DIV-0027 (amended 2026-10-03): after the self-test, which
    // compares the original's prompt; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("TradeConfirmLayout", Low32(&g_trade_confirm_layout),
                         &was, &is, 1);
        bof3::Log("DIV-0027    Manillo's \"Is <item> OK?\": answers re-spaced, the hand two units left of each");
    }
}
