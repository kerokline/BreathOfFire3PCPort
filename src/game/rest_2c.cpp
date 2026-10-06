// The inn's, save point's and rest's last states, the save block's builder,
// the shop's browse and sell modes, the master's talk and its panels, the
// figure record's moves - round fourteen, wave two, group R2C: the 60
// functions 0x57F340..0x58699F of the cut (analysis/round14_cut.tsv) and
// ShopMode_States[9] 0x583350, which no list had; each read with capstone to
// its last instruction (docs/rest_2c.md section 1).
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a table's handler is called by the address the table holds. No divergence:
// each is a faithful replacement. Where the original jumps through a table by
// a byte past the table's own count, or writes a slot's summary past the
// sixteen, ours aborts with a message (section 5); no state or caller writes
// such a byte. Reads past a table (a scale, a face, a master's message, a
// record by an id byte) read the image in place, as the original does.
#include "game/rest_2c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2c_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_2c::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return *At(address); }
U L(U address) { return static_cast<U>(Long(At(address))); }
void PutL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }
void PutW(U address, unsigned v) { SetWord(At(address), v); }
float F(U address) {
    float v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutF(unsigned char* at, float v) { std::memcpy(at, &v, sizeof v); }
U Bits(float v) {
    U u;
    std::memcpy(&u, &v, sizeof u);
    return u;
}
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

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
                    static_cast<unsigned>(table), static_cast<unsigned>(table + 4 * index));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * index)))();
}

// The screen title's box the inn and save states draw first: (0x14, 0x12),
// 0x118 by 0x13, in the window colour (the original pushes the byte in a
// register whose upper bytes it never cleared; Menu_DrawTitleBox reads the
// byte - shop_states.cpp's TitleBox).
void TitleBox(int y) { SH_CALL(Menu_DrawTitleBox)(0x14, y, 0x118, 0x13, B(at::kColour)); }

// System message `id` at (0x1C, 0x16), colour 0, to its end.
void Message(unsigned id) {
    const unsigned char* const text = SH_CALL(Msg_SystemPtr)(id);
    SH_CALL(Text_DrawAt)(0x1C, 0x16, 0, 0xFF, text);
}

// A slot's summary: 0x905BC0 + 0x1C * slot, for the slot the original takes
// unchecked from 0x9036D4. Every writer of that dword keeps it below 16
// (FieldSave_Choose, Save_QuickWrite, the load menu); past it ours aborts,
// where the original writes past the sixteen summaries.
U SummaryOf(U slot, const char* who) {
    if (slot >= at::kSlotCount)
        bof3::Fatal("%s: save slot %u is past the %u summaries; the original writes at 0x%X", who, static_cast<unsigned>(slot),
                    at::kSlotCount, static_cast<unsigned>(at::kSummaries + slot * at::kSummaryBytes));
    return at::kSummaries + slot * at::kSummaryBytes;
}

// The seven dwords Save_BuildBlock filled at 0x904680 into the slot's summary.
void CopySummary(U slot, const char* who) {
    const U to = SummaryOf(slot, who);
    for (U i = 0; i < 7; ++i) PutL(to + 4 * i, L(at::kSummary + 4 * i));
}

// The window record n's byte / word at +off.
U Win(unsigned n, unsigned off) { return at::kWindows + n * at::kWindowStride + off; }

// A party id's form: id 4 is drawn and saved as 0xB unless the story byte's
// low seven bits are 5 or 0xC (Save_BuildBlock's and MasterPanel_DrawFace's
// test, the same instructions).
unsigned char FormOf(unsigned char id) {
    if (id != 4) return id;
    const unsigned story = B(at::kStoryByte) & 0x7F;
    return story == 5 || story == 0xC ? id : static_cast<unsigned char>(0xB);
}

// A master's script message: the u16 at 0x6644E8 + 2 * master (read in place
// for any master byte, as the original reads it) plus k, as a word.
unsigned MasterMessage(unsigned char master, unsigned k) {
    return static_cast<std::uint16_t>(Word(At(at::kMasterMessages + 2u * master)) + k);
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ===========================================================================
// The figure record 0x9398E0 (MasterFigure_States 0x663E28, by +1)
// ===========================================================================

// original 0x57F340: the record drawn by R2B's 0x57EEF0 with its colour
// (+0x5D, +0x5E, +0x5F) faded for the draw and put back after it. The scale
// +0x40 is the dword the scale index +0x10B names (0x663D7C, read in place).
// With s the fade step +0x107 and t = 3 * s: the first byte s * 3 lower when
// it is above t (signed int compare), else 0; the second s * 3 higher when
// 0xFF less it is above t, else 0xFF; the third as the first. Each is a byte
// operation (imul's low byte added).
extern "C" void __cdecl MasterFigure_DrawFaded(void) {
    const unsigned char r = B(at::kFigureRgb), g = B(at::kFigureRgb + 1), b = B(at::kFigureRgb + 2);
    PutL(at::kFigureScale, L(at::kScaleTable + 4u * B(at::kFigureScaleIndex)));
    const unsigned char step = B(at::kFigureFade);
    const int t = 3 * step;
    const auto down = static_cast<unsigned char>(step * 0xFDu);   // imul 0xFD's low byte
    B(at::kFigureRgb) = static_cast<int>(r) > t ? static_cast<unsigned char>(r + down) : 0;
    B(at::kFigureRgb + 1) = 0xFF - static_cast<int>(B(at::kFigureRgb + 1)) > t
                                ? static_cast<unsigned char>(B(at::kFigureRgb + 1) + static_cast<unsigned char>(step * 3u))
                                : 0xFF;
    B(at::kFigureRgb + 2) = static_cast<int>(B(at::kFigureRgb + 2)) > t ? static_cast<unsigned char>(B(at::kFigureRgb + 2) + down)
                                                                         : 0;
    SH_AT(void (__cdecl*)(U), at::kFigureDraw)(at::kFigure);
    B(at::kFigureRgb) = r;
    B(at::kFigureRgb + 1) = g;
    B(at::kFigureRgb + 2) = b;
}

// original 0x57F420, MasterFigure_States[2]: the angle +0x6C, when its low
// twelve bits are not 0, 0x40 less and kept to 0xFC0 (+6 = 0); else +6 = 1.
// Then the faded draw (a tail jump in the original).
extern "C" void __cdecl MasterFigure_TurnHome(void) {
    const U angle = L(at::kFigureAngle);
    if ((angle & 0xFFF) != 0) {
        B(at::kFigureDone) = 0;
        PutL(at::kFigureAngle, (angle - 0x40) & 0xFC0);
    } else {
        B(at::kFigureDone) = 1;
    }
    SH_CALL(MasterFigure_DrawFaded)();
}

// original 0x57F450, MasterFigure_States[3]: the target height is
// AreaMap_Elevation(Field_Kind2X, Field_Kind2Z) as a short in the high half
// plus +0xC0 * 0xA00; the height +0x3C moves by ((the elevation again, as a
// short, + 0x200) << 16 less the target) / 16 (signed, toward 0) downward.
// Below the target: set to it and +6 = 1; else +6 = 0. Then the faded draw.
// The second call reads both coordinates again.
extern "C" void __cdecl MasterFigure_Settle(void) {
    const long first = SH_CALL(AreaMap_Elevation)(static_cast<long>(L(at::kKind2X)), static_cast<long>(L(at::kKind2Z)));
    const U target = (static_cast<U>(static_cast<int>(static_cast<short>(first))) << 16) + L(at::kFigureLift) * 0xA00u;
    const long second = SH_CALL(AreaMap_Elevation)(static_cast<long>(L(at::kKind2X)), static_cast<long>(L(at::kKind2Z)));
    const U top = (static_cast<U>(static_cast<int>(static_cast<short>(second))) + 0x200u) << 16;
    const int fall = static_cast<int>(top - target) / 16;
    const U y = L(at::kFigureY) - static_cast<U>(fall);
    PutL(at::kFigureY, y);
    if (static_cast<int>(y) < static_cast<int>(target)) {
        PutL(at::kFigureY, target);
        B(at::kFigureDone) = 1;
    } else {
        B(at::kFigureDone) = 0;
    }
    SH_CALL(MasterFigure_DrawFaded)();
}

// original 0x57F4E0, MasterFigure_States[4]: the faded draw alone (a tail jump).
extern "C" void __cdecl MasterFigure_Hold(void) { SH_CALL(MasterFigure_DrawFaded)(); }

// original 0x57F4F0, MasterFigure_States[5]: the angle + 0x20 (R2B's
// 0x57F330, entry 1, takes 0x20 off), then the faded draw.
extern "C" void __cdecl MasterFigure_TurnOn(void) {
    PutL(at::kFigureAngle, L(at::kFigureAngle) + 0x20);
    SH_CALL(MasterFigure_DrawFaded)();
}

// ===========================================================================
// The inn and the save point (shop_states.cpp has their other states)
// ===========================================================================

// original 0x57FA40, InnPrompt_States[4] ("not enough": InnPrompt_Choose's
// sub-state + 1 when the zenny is short): the choices at (0x6E, 0x4C), the
// title box, with an object the zenny box, system message 0xA2; any button
// pressed (Input_Pressed's low word) takes the sub-state back one.
extern "C" void __cdecl InnPrompt_NotEnough(void) {
    SH_CALL(SaveMenu_DrawChoices)(0x6E, 0x4C);
    TitleBox(0x12);
    if (B(at::kObject) != 0xFF) SH_CALL(Menu_DrawMoneyBox)(0x62, 0x28, 0, L(at::kZenny));
    Message(0xA2);
    if (Word(At(at::kPressed)) != 0) --B(at::kStep);
}

// original 0x580010, FieldSave_States[4] (FieldSave_Confirm's yes): the title
// box, system message 0x99, the slots at (0x20, 0x30) unlit; the block built
// (Save_BuildBlock), the file's name made from the slot (0x9036D4) and the
// file written, 0x12B0 bytes. Unless Save_WriteFile answered -1 (DIV-0003's
// failure path: a bare return): the sub-state up, the timer 0x1E, the slot's
// summary (the slot read again) and 0x6BC881 = 2.
extern "C" void __cdecl FieldSave_Write(void) {
    TitleBox(0x12);
    Message(0x99);
    SH_CALL(SaveMenu_DrawSlots)(0x20, 0x30, 0);
    SH_CALL(Save_BuildBlock)();
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kPath)), reinterpret_cast<const char*>(At(at::kPathFormat)), L(at::kSlot));
    const int written = SH_CALL(Save_WriteFile)(reinterpret_cast<const char*>(At(at::kPath)), at::kSaveSize);
    if (written == -1) return;
    const U slot = L(at::kSlot);
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    B(at::kTimer) = 0x1E;
    B(at::kStep) = step;
    CopySummary(slot, "FieldSave_Write");
    B(at::kInnCursor) = 2;
}

// original 0x5800D0, FieldSave_States[5]: the title box, system message 0x8F,
// the slots unlit; 0x6BC880 = 1 (FieldSave_End reads it). With no button
// pressed the timer runs down and nothing more until it reaches 0; then (or
// at once on a button) the timer 0, Sound_PlayEffect(0x104) and the sub-state
// up (read after the sound).
extern "C" void __cdecl FieldSave_Written(void) {
    TitleBox(0x12);
    Message(0x8F);
    SH_CALL(SaveMenu_DrawSlots)(0x20, 0x30, 0);
    B(at::kSaveBack) = 1;
    if (Word(At(at::kPressed)) == 0) {
        const auto timer = static_cast<unsigned char>(B(at::kTimer) - 1);
        B(at::kTimer) = timer;
        if (timer != 0) return;
    }
    B(at::kTimer) = 0;
    SH_CALL(Sound_PlayEffect)(0x104);
    ++B(at::kStep);
}

// original 0x580230, FieldSave_States[8] (FieldSave_End's question, system
// message 0xD0): the title box; once the message is done (0x7DEE44 bit 1):
// the answer 0x929F0B 0 - the timer 0 and the step up (the way out); else
// the step back to 3 (the save menu) and the sub-state 0.
extern "C" void __cdecl FieldSave_PromptAnswer(void) {
    TitleBox(0x12);
    if ((B(at::kMsgFlags) & 2) == 0) return;
    if (B(at::kAnswer) == 0) {
        B(at::kTimer) = 0;
        ++B(at::kState);
        return;
    }
    B(at::kState) = 3;
    B(at::kStep) = 0;
}

// original 0x580280, Inn_Steps[4] (the way out): the title box at 0x12 - 20 *
// timer (the timer read before the draw; the original's upper half of the
// register is the entry's, and only the low 16 bits reach the callee); the
// timer up, at 5: mode 1 (ShopMode_End), the state 0, and with
// Field_InputFlags 0x40 or area 0xBC, 0x85 or 0xC1 (Inn_Begin's test) the
// object 0x929F0C back from 0x929F0F.
extern "C" void __cdecl Inn_TitleOut(void) {
    const int y = 0x12 - 20 * static_cast<int>(B(at::kTimer));
    TitleBox(y);
    const auto timer = static_cast<unsigned char>(B(at::kTimer) + 1);
    B(at::kTimer) = timer;
    if (timer != 5) return;
    B(at::kMode) = 1;
    B(at::kState) = 0;
    const unsigned area = Word(At(at::kArea));
    if (B(at::kInputFlags) == 0x40 || area == 0xBC || area == 0x85 || area == 0xC1) B(at::kObject) = B(at::kKeptObject);
}

// ===========================================================================
// The rest (ShopMode_States[10], Rest_States)
// ===========================================================================

// original 0x580300: `jmp [Rest_States + 4 * 0x929F01]`.
extern "C" void __cdecl Rest_Dispatch(void) { Jump(at::kRestStates, 7, B(at::kState), "Rest_Dispatch"); }

// original 0x580560, Rest_States[2]: black, Sound_LoadStream(0) (the jingle),
// the state up, the timer 0x96.
extern "C" void __cdecl Rest_LoadJingle(void) {
    SH_CALL(Menu_DrawBlackScreen)();
    SH_CALL(Sound_LoadStream)(0);
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(at::kTimer) = 0x96;
    B(at::kState) = state;
}

// original 0x580590, Rest_States[3]: black; a timer above 0 runs down and
// waits; at 0, once Sound_StreamDone answers not 0: the state up and
// Transition_Start(1).
extern "C" void __cdecl Rest_WaitJingle(void) {
    SH_CALL(Menu_DrawBlackScreen)();
    const unsigned char timer = B(at::kTimer);
    if (timer != 0) {
        B(at::kTimer) = static_cast<unsigned char>(timer - 1);
        if (timer - 1 != 0) return;
    }
    if (SH_CALL(Sound_StreamDone)() == 0) return;
    ++B(at::kState);
    SH_CALL(Transition_Start)(1);
}

// original 0x5805D0, Rest_States[4]: when the wait word 0x66C810 is 0:
// Party_RestoreAll(0) and the state up (read after the call).
extern "C" void __cdecl Rest_Restore(void) {
    if (Word(At(at::kWait)) != 0) return;
    SH_CALL(Party_RestoreAll)(0);
    ++B(at::kState);
}

// original 0x580600, Rest_States[5]: mode 1 (ShopMode_End), the state 0.
extern "C" void __cdecl Rest_End(void) {
    B(at::kMode) = 1;
    B(at::kState) = 0;
}

// original 0x580610, Rest_States[6] (Rest_Begin's, with its message 0xEF):
// once the message is done (0x7DEE44 bit 1), mode 1 and the state 0.
extern "C" void __cdecl Rest_EndAfterMessage(void) {
    if ((B(at::kMsgFlags) & 2) == 0) return;
    B(at::kMode) = 1;
    B(at::kState) = 0;
}

// ===========================================================================
// The save block (FieldSave_Write's and Save_QuickWrite's)
// ===========================================================================

// original 0x5806F0: the live block 0x9039E0 gathered and copied out. Into
// the block: the chapter dwords 0x8034E0 / E4 / E8 / EC; the counters'
// bytes 0x903848..0x90384B; ObjTrio's leader +0x34 / +0x38 and its +8 byte;
// Game_AreaNumber; 0x903A50 = 0. The summary 0x904680: the buttons' dwords
// 0x903580 / 84 / 88 / 8C and word 0x903590 after it; the leader's name -
// the record of MoveScript_EffectState[party id 0], strncpy 5 bytes, then 4
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
    PutL(at::kBlock, L(0x8034E0));
    PutL(at::kBlock + 4, L(0x8034E4));
    PutL(at::kBlock + 0xC, L(0x8034EC));
    const U counters = L(0x903848);
    PutL(at::kBlock + 8, L(0x8034E8));
    B(0x903A00) = static_cast<unsigned char>(counters);
    B(0x903A01) = static_cast<unsigned char>(counters >> 8);
    B(0x903A02) = B(0x90384A);
    PutL(0x903A08, L(0x802D74));
    PutL(0x903A0C, L(0x802D78));
    B(0x903A07) = B(0x802D48);
    PutL(0x90469C, L(0x903580));
    PutL(0x9046A4, L(0x903588));
    B(0x903A03) = B(0x90384B);
    PutL(0x9046A8, L(0x90358C));
    PutW(0x903A04, Word(At(at::kArea)));
    PutL(0x9046A0, L(0x903584));
    const unsigned record = B(at::kMemberRecord + B(at::kPartyList));
    PutW(0x9046AC, Word(At(0x903590)));
    PutW(0x903A50, 0);
    const U name = at::kRecords + (g_summary_record0 ? 0u : record) * at::kRecordStride;   // DIV-0076
    SH_CALL(Crt_strncpy)(reinterpret_cast<char*>(At(at::kSummary)), reinterpret_cast<const char*>(At(name)), 5);
    SH_CALL(Crt_strncpy)(reinterpret_cast<char*>(At(0x904696)), reinterpret_cast<const char*>(At(name + 5)), 4);
    for (unsigned i = 0; i < 3; ++i) B(at::kPartyList + 0x623 + i) = FormOf(B(at::kPartyList + i));
    B(0x904688) = B(0x903A7A);
    PutL(0x90468C, L(0x9040C8));
    B(0x904689) = B(at::kColour);
    B(0x90468A) = B(0x903A5B);
    PutL(0x904690, L(0x903A7C));
    const unsigned char flag = SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x92);
    B(0x904694) = flag;
    B(0x90412D) = B(0x9039A2);
    U sum = 0;
    for (U i = 0; i < at::kBlockBytes; ++i) {
        const unsigned char v = B(at::kBlock + i);
        B(at::kStaging + i) = v;
        sum += v;
    }
    std::memset(At(at::kStaging + at::kBlockBytes), 0, at::kStagingClear);
    PutW(at::kChecksum, sum);
    CopySummary(L(at::kSlot), "Save_BuildBlock");
}

// original 0x5809C0 (the window procedure's F12, Game_WndProc): slot 0, the
// file's name made with 0 (the slot not yet read), the block built, the name
// made again with the slot (read again), the file written; unless
// Save_WriteFile answered -1, the slot's summary.
extern "C" void __cdecl Save_QuickWrite(void) {
    PutL(at::kSlot, 0);
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kPath)), reinterpret_cast<const char*>(At(at::kPathFormat)), 0u);
    SH_CALL(Save_BuildBlock)();
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kPath)), reinterpret_cast<const char*>(At(at::kPathFormat)), L(at::kSlot));
    const int written = SH_CALL(Save_WriteFile)(reinterpret_cast<const char*>(At(at::kPath)), at::kSaveSize);
    if (written == -1) return;
    CopySummary(L(at::kSlot), "Save_QuickWrite");
}

// ===========================================================================
// The shop's modes: the dispatchers of ShopMode_States 5..9
// ===========================================================================

// original 0x580A40, ShopMode_States[6]: `jmp [PartyForm_States + 4 * 0x929F01]`.
extern "C" void __cdecl PartyForm_Dispatch(void) { Jump(at::kPartyFormStates, 4, B(at::kState), "PartyForm_Dispatch"); }

// original 0x5837E0, ShopMode_States[5]: `jmp [ShopResist_States + 4 * 0x929F01]`.
extern "C" void __cdecl ShopResist_Dispatch(void) { Jump(at::kResistStates, 8, B(at::kState), "ShopResist_Dispatch"); }

// original 0x584180, ShopMode_States[7]: `jmp [SharedList_States + 4 * 0x929F01]`.
extern "C" void __cdecl SharedList_Dispatch(void) { Jump(at::kSharedListStates, 5, B(at::kState), "SharedList_Dispatch"); }

// original 0x582EB0, ShopMode_States[8]: `jmp [ShopSell_States + 4 * 0x929F01]`.
extern "C" void __cdecl ShopSell_Dispatch(void) { Jump(at::kShopSellStates, 4, B(at::kState), "ShopSell_Dispatch"); }

// original 0x582FF0, ShopSell_States[2]: `jmp [ShopSell_SellSteps + 4 * 0x929F02]`.
extern "C" void __cdecl ShopSell_SellStep(void) { Jump(at::kShopSellSellSteps, 5, B(at::kStep), "ShopSell_SellStep"); }

// original 0x582EC0, ShopSell_States[0]: windows 0, 1 and 7 on (+0 1, +3 2,
// +1 7), 0 at (0x14, -0x14) with +2 0 and +0x10 0, 1 at (0x68, -0x14) with
// +2 1; window 21 (the hand) +1 7, +2 3, off; window 7 at (0x140, 0x3E), +2
// 9, +0xA..+0xD 0, 0, 0, 0xFF, +8 3, +9 0, the words +0x10 and +0x12 0;
// 0x6BC8AA = 1, 0x6BC8A4 = 0; the state two on, the step 0.
extern "C" void __cdecl ShopSell_Open(void) {
    B(Win(0, 0)) = 1;
    B(Win(1, 2)) = 1;
    B(Win(1, 0)) = 1;
    B(Win(7, 0)) = 1;
    B(at::kSellOn) = 1;
    const auto state = static_cast<unsigned char>(B(at::kState) + 2);
    B(Win(0, 3)) = 2;
    B(Win(1, 3)) = 2;
    B(Win(7, 3)) = 2;
    B(Win(0, 1)) = 7;
    B(Win(0, 2)) = 0;
    PutW(Win(0, 4), 0x14);
    PutW(Win(0, 6), 0xFFEC);
    PutW(Win(0, 0x10), 0);
    B(Win(1, 1)) = 7;
    PutW(Win(1, 4), 0x68);
    PutW(Win(1, 6), 0xFFEC);
    B(Win(21, 1)) = 7;
    B(Win(21, 2)) = 3;
    B(Win(21, 0)) = 0;
    PutW(Win(7, 4), 0x140);
    PutW(Win(7, 6), 0x3E);
    B(Win(7, 1)) = 7;
    B(Win(7, 2)) = 9;
    B(Win(7, 0xA)) = 0;
    B(Win(7, 0xB)) = 0;
    B(Win(7, 0xC)) = 0;
    B(Win(7, 0xD)) = 0xFF;
    B(Win(7, 8)) = 3;
    B(Win(7, 9)) = 0;
    PutW(Win(7, 0x10), 0);
    PutW(Win(7, 0x12), 0);
    B(at::kSellFlag) = 0;
    B(at::kState) = state;
    B(at::kStep) = 0;
}

// original 0x582FB0, ShopSell_States[1]: windows 0, 1 and 7's +3 = 1 (out),
// the hand off; the timer 5, the state two on (to 3), the step 0.
extern "C" void __cdecl ShopSell_Leave(void) {
    B(Win(0, 3)) = 1;
    B(Win(1, 3)) = 1;
    B(Win(7, 3)) = 1;
    const auto state = static_cast<unsigned char>(B(at::kState) + 2);
    B(Win(21, 0)) = 0;
    B(at::kTimer) = 5;
    B(at::kState) = state;
    B(at::kStep) = 0;
}

// original 0x583000, ShopSell_States[3]: the timer down; at 0, mode 1.
extern "C" void __cdecl ShopSell_LeaveWait(void) {
    const auto timer = static_cast<unsigned char>(B(at::kTimer) - 1);
    B(at::kTimer) = timer;
    if (timer == 0) B(at::kMode) = 1;
}

// ===========================================================================
// ShopMode 9, the browse (ShopBrowse_States 0x664190 and its steps)
// ===========================================================================

// original 0x583350, ShopMode_States[9]: `jmp [ShopBrowse_States + 4 * 0x929F01]`.
extern "C" void __cdecl ShopBrowse_Dispatch(void) { Jump(at::kBrowseStates, 3, B(at::kState), "ShopBrowse_Dispatch"); }

// original 0x583360, state 0: `jmp [ShopBrowse_OpenSteps + 4 * 0x929F02]`.
extern "C" void __cdecl ShopBrowse_OpenStep(void) { Jump(at::kBrowseOpenSteps, 2, B(at::kStep), "ShopBrowse_OpenStep"); }

// original 0x5833D0, state 1: `jmp [ShopBrowse_ChooseSteps + 4 * 0x929F02]`.
extern "C" void __cdecl ShopBrowse_ChooseStep(void) { Jump(at::kBrowseChooseSteps, 3, B(at::kStep), "ShopBrowse_ChooseStep"); }

// original 0x5836A0, state 2: `jmp [ShopBrowse_CloseSteps + 4 * 0x929F02]`.
extern "C" void __cdecl ShopBrowse_CloseStep(void) { Jump(at::kBrowseCloseSteps, 3, B(at::kStep), "ShopBrowse_CloseStep"); }

// original 0x583370, open step 0: ShopBrowse_InitWindows, the step up (read
// after it), the timer 6, Sound_PlayEffect(0x102).
extern "C" void __cdecl ShopBrowse_Open(void) {
    SH_CALL(ShopBrowse_InitWindows)();
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    B(at::kTimer) = 6;
    B(at::kStep) = step;
    SH_CALL(Sound_PlayEffect)(0x102);
}

// original 0x5833A0, open step 1: the timer down; at 0 the step 0 and the
// state up.
extern "C" void __cdecl ShopBrowse_OpenWait(void) {
    const auto timer = static_cast<unsigned char>(B(at::kTimer) - 1);
    B(at::kTimer) = timer;
    if (timer != 0) return;
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(at::kStep) = 0;
    B(at::kState) = state;
}

// original 0x5833E0, choose step 0: the list in window 1 (+0xA the first
// shown, +0xB the cursor, +8 the scroll, +0xD the entry under it). The
// hand (window 21) at window 1's x + 4 and y + 13 * (cursor - first + 2) -
// both 16-bit stores - and on; window 8's +0xA 0, window 0's +0x10 0x32.
// Input_AutoRepeat(Input_Pressed & 0xF00C); the cursor read after it (and kept
// to compare). Up (0x1000): the cursor back one above 0, and the scroll 0xF0
// when it went above the first shown. Else down (0x4000): on one below 0x10,
// the scroll 0x10 when it reached the first + 9 (as ints). Else 0x0004 (a
// page back): from the first at 0 the cursor 0; first below 9 - the cursor
// less it and the first 0; else both less 9. Else 0x0008 (a page on): the
// first at 8 - the cursor 0x10; else the cursor plus 8 - first, the first 8.
// A moved cursor Sound_PlayEffect(0x100). While scrolling (+8 not 0)
// nothing more. Then Input_Pressed read again: a confirm button - with an
// entry (+0xD not 0xFF) 0x103, 0x102, ShopBrowse_OpenDetail, the hand off,
// window 1 +3 = 3, the timer 8 and the step up (read before the stores); no
// entry 0x107. Else a cancel button: 0x106, 0x102, the hand off, windows 0
// and 1 +3 = 1, the timer 0xA, the state up (read before) and the step 0.
extern "C" void __cdecl ShopBrowse_Choose(void) {
    PutW(Win(21, 4), L(Win(1, 4)) + 4);
    B(Win(8, 0xA)) = 0;
    const unsigned rows = static_cast<unsigned>(B(Win(1, 0xB))) - B(Win(1, 0xA)) + 2;
    PutW(Win(0, 0x10), 0x32);
    B(Win(21, 0)) = 1;
    PutW(Win(21, 6), (rows * 13 + Word(At(Win(1, 6)))) & 0xFFFF);
    const unsigned keys = SH_CALL(Input_AutoRepeat)(Word(At(at::kPressed)) & 0xF00Cu);
    unsigned char cursor = B(Win(1, 0xB));
    const unsigned char kept = cursor;
    if (keys & 0x1000) {
        if (cursor > 0) {
            --cursor;
            B(Win(1, 0xB)) = cursor;
        }
        if (cursor < B(Win(1, 0xA))) B(Win(1, 8)) = 0xF0;
    } else if (keys & 0x4000) {
        if (cursor < 0x10) {
            ++cursor;
            B(Win(1, 0xB)) = cursor;
        }
        if (static_cast<int>(cursor) >= static_cast<int>(B(Win(1, 0xA))) + 9) B(Win(1, 8)) = 0x10;
    } else if (keys & 0x0004) {
        const unsigned char first = B(Win(1, 0xA));
        if (first == 0) {
            cursor = 0;
            B(Win(1, 0xB)) = cursor;
        } else if (first < 9) {
            cursor = static_cast<unsigned char>(cursor - first);
            B(Win(1, 0xA)) = 0;
            B(Win(1, 0xB)) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor - 9);
            B(Win(1, 0xB)) = cursor;
            B(Win(1, 0xA)) = static_cast<unsigned char>(first - 9);
        }
    } else if (keys & 0x0008) {
        const unsigned char first = B(Win(1, 0xA));
        if (first == 8) {
            cursor = 0x10;
            B(Win(1, 0xB)) = cursor;
        } else {
            B(Win(1, 0xA)) = 8;
            cursor = static_cast<unsigned char>(cursor + static_cast<unsigned char>(8 - first));
            B(Win(1, 0xB)) = cursor;
        }
    }
    if (kept != cursor) SH_CALL(Sound_PlayEffect)(0x100);
    if (B(Win(1, 8)) != 0) return;
    const unsigned pressed = Word(At(at::kPressed));
    if (Word(At(at::kConfirm)) & pressed) {
        if (B(Win(1, 0xD)) == 0xFF) {
            SH_CALL(Sound_PlayEffect)(0x107);
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x103);
        SH_CALL(Sound_PlayEffect)(0x102);
        SH_CALL(ShopBrowse_OpenDetail)();
        const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
        B(Win(21, 0)) = 0;
        B(Win(1, 3)) = 3;
        B(at::kTimer) = 8;
        B(at::kStep) = step;
        return;
    }
    if ((Word(At(at::kCancel)) & pressed) == 0) return;
    SH_CALL(Sound_PlayEffect)(0x106);
    SH_CALL(Sound_PlayEffect)(0x102);
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(Win(21, 0)) = 0;
    B(Win(0, 3)) = 1;
    B(Win(1, 3)) = 1;
    B(at::kTimer) = 0xA;
    B(at::kState) = state;
    B(at::kStep) = 0;
}

// original 0x5835F0, choose step 1 (the entry's detail, ShopBrowse_OpenDetail's
// windows 2 and 3): window 0's +0x10 = the entry (+0xD) + 0x122 as a word,
// window 1's +0xC = its cursor; a timer above 0 runs down and nothing more.
// At 0, a cancel button: 0x106, 0x102, windows 2 and 3 +3 = 1, window 1 +3 =
// 2 and +0xC = 0x7F, the timer 8, the step up (read before the stores).
extern "C" void __cdecl ShopBrowse_Detail(void) {
    PutW(Win(0, 0x10), (B(Win(1, 0xD)) + 0x122u) & 0xFFFF);
    const unsigned char cursor = B(Win(1, 0xB));
    const unsigned char timer = B(at::kTimer);
    B(Win(1, 0xC)) = cursor;
    if (timer != 0) {
        B(at::kTimer) = static_cast<unsigned char>(timer - 1);
        return;
    }
    if ((Word(At(at::kCancel)) & Word(At(at::kPressed))) == 0) return;
    SH_CALL(Sound_PlayEffect)(0x106);
    SH_CALL(Sound_PlayEffect)(0x102);
    B(Win(2, 3)) = 1;
    B(Win(3, 3)) = 1;
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    B(Win(1, 3)) = 2;
    B(Win(1, 0xC)) = 0x7F;
    B(at::kTimer) = 8;
    B(at::kStep) = step;
}

// original 0x583680, choose step 2: the timer down; at 0 the step two back
// (to the list).
extern "C" void __cdecl ShopBrowse_DetailClose(void) {
    const auto timer = static_cast<unsigned char>(B(at::kTimer) - 1);
    B(at::kTimer) = timer;
    if (timer == 0) B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 0xFE);
}

// original 0x5836B0, close step 1 (step 0 is FS's Menu_StepAfterTimer): the
// step up.
extern "C" void __cdecl ShopBrowse_CloseNext(void) { ++B(at::kStep); }

// original 0x5836C0, close step 2: mode 1, the state and the step 0.
extern "C" void __cdecl ShopBrowse_End(void) {
    B(at::kMode) = 1;
    B(at::kState) = 0;
    B(at::kStep) = 0;
}

// ===========================================================================
// The master's talk (MasterTalk_States 0x66450C, by 0x9398CF)
// ===========================================================================

// original 0x585A00 (FieldTail_LoadBank's step 1): the talk's mode, step,
// 0x9398D0, 0x9398CD and the slide 0.
extern "C" void __cdecl MasterTalk_Reset(void) {
    B(at::kTalkMode) = 0;
    B(at::kTalkStep) = 0;
    B(at::kTalkD0) = 0;
    B(at::kTalkCD) = 0;
    B(at::kTalkSlide) = 0;
}

// original 0x586670 (FieldTail_LoadBank's step 2, a tail jump):
// `jmp [MasterTalk_States + 4 * 0x9398CF]`.
extern "C" void __cdecl MasterTalk_Dispatch(void) { Jump(at::kTalkStates, 7, B(at::kTalkMode), "MasterTalk_Dispatch"); }

// original 0x5866D0, mode 1: `jmp [MasterTalk_IntroSteps + 4 * 0x9398D1]`.
extern "C" void __cdecl MasterTalk_IntroStep(void) { Jump(at::kTalkIntroSteps, 2, B(at::kTalkStep), "MasterTalk_IntroStep"); }

// original 0x586750, mode 2: `jmp [MasterTalk_AskSteps + 4 * 0x9398D1]`.
extern "C" void __cdecl MasterTalk_AskStep(void) { Jump(at::kTalkAskSteps, 6, B(at::kTalkStep), "MasterTalk_AskStep"); }

// original 0x586970, mode 3: `jmp [MasterTalk_PickSteps + 4 * 0x9398D1]`.
extern "C" void __cdecl MasterTalk_PickStep(void) { Jump(at::kTalkPickSteps, 9, B(at::kTalkStep), "MasterTalk_PickStep"); }

// original 0x586680, mode 0: the master's introduction flag (Flags_Test of
// 0x904657 by the master 0x9039F5) set - mode 5; else master 9 - script
// message 0x2E, Field_Request 2, mode 6; else mode 1.
extern "C" void __cdecl MasterTalk_Begin(void) {
    if (SH_CALL(Flags_Test)(At(at::kIntroFlags), B(at::kMaster)) != 0) {
        B(at::kTalkMode) = 5;
        return;
    }
    if (B(at::kMaster) == 9) {
        SH_CALL(Msg_OpenScript)(0x2E);
        B(at::kRequest) = 2;
        B(at::kTalkMode) = 6;
        return;
    }
    B(at::kTalkMode) = 1;
}

// original 0x5866E0, intro step 0: Flags_Set(0x904654, master), the master's
// message (its base, the master read again), Field_Request 2, the step up.
extern "C" void __cdecl MasterTalk_IntroSay(void) {
    SH_CALL(Flags_Set)(At(at::kMetFlags), B(at::kMaster));
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(B(at::kMaster), 0)));
    const auto step = static_cast<unsigned char>(B(at::kTalkStep) + 1);
    B(at::kRequest) = 2;
    B(at::kTalkStep) = step;
}

// original 0x586720, intro step 1: once Field_Request is not 2 (the message
// closed): Flags_Set(0x904657, master), mode 3, the step 0.
extern "C" void __cdecl MasterTalk_IntroWait(void) {
    if (B(at::kRequest) == 2) return;
    SH_CALL(Flags_Set)(At(at::kIntroFlags), B(at::kMaster));
    B(at::kTalkMode) = 3;
    B(at::kTalkStep) = 0;
}

namespace {

// The ask steps' message: the master's base + k, Field_Request 2, the step up
// (read after the call).
void Say(unsigned k) {
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(B(at::kMaster), k)));
    const auto step = static_cast<unsigned char>(B(at::kTalkStep) + 1);
    B(at::kRequest) = 2;
    B(at::kTalkStep) = step;
}

// The members (Field_MemberCount, ObjTrio's +0x148 record index) scanned for
// the first whose record's +0x1F (its master) is (`equal`) or is not the
// master byte: its slot, or the count when none is (signed compares).
unsigned FirstMember(unsigned char master, bool equal) {
    const int count = B(at::kMemberCount);
    int i = 0;
    for (; i < count; ++i) {
        const unsigned record = B(at::kMembers + static_cast<U>(i) * at::kObjStride);
        const bool same = B(at::kRecords + 0x1F + record * at::kRecordStride) == master;
        if (same == equal) break;
    }
    return static_cast<unsigned>(i);
}

}  // namespace

// original 0x586760, ask step 0: the master's message + 5.
extern "C" void __cdecl MasterTalk_Say5(void) { Say(5); }

// original 0x586790, ask step 1: once Field_Request is not 2, message + 6.
extern "C" void __cdecl MasterTalk_Say6(void) {
    if (B(at::kRequest) != 2) Say(6);
}

// original 0x5867D0, ask step 2: once Field_Request is not 2 - a member whose
// master is not this one: mode 3, the step 0; every member's (or none): the
// master's message + 7 (the master byte read once, before the scan).
extern "C" void __cdecl MasterTalk_CheckAllPupils(void) {
    if (B(at::kRequest) == 2) return;
    const unsigned char master = B(at::kMaster);
    if (static_cast<int>(FirstMember(master, false)) < static_cast<int>(B(at::kMemberCount))) {
        B(at::kTalkMode) = 3;
        B(at::kTalkStep) = 0;
        return;
    }
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(master, 7)));
    const auto step = static_cast<unsigned char>(B(at::kTalkStep) + 1);
    B(at::kRequest) = 2;
    B(at::kTalkStep) = step;
}

// original 0x586860, ask step 3: once Field_Request is not 2, message + 8.
extern "C" void __cdecl MasterTalk_Say8(void) {
    if (B(at::kRequest) != 2) Say(8);
}

// original 0x5868A0, ask step 4: once Field_Request is not 2 - a member whose
// master is this one: mode 4, the step 0; none: message + 9.
extern "C" void __cdecl MasterTalk_CheckAnyPupil(void) {
    if (B(at::kRequest) == 2) return;
    const unsigned char master = B(at::kMaster);
    if (static_cast<int>(FirstMember(master, true)) < static_cast<int>(B(at::kMemberCount))) {
        B(at::kTalkMode) = 4;
        B(at::kTalkStep) = 0;
        return;
    }
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(master, 9)));
    const auto step = static_cast<unsigned char>(B(at::kTalkStep) + 1);
    B(at::kRequest) = 2;
    B(at::kTalkStep) = step;
}

// original 0x586930, ask step 5: once Field_Request is not 2, message + 4,
// Field_Request 2, mode 6 and the step 0.
extern "C" void __cdecl MasterTalk_SayFarewell(void) {
    if (B(at::kRequest) == 2) return;
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(MasterMessage(B(at::kMaster), 4)));
    B(at::kRequest) = 2;
    B(at::kTalkMode) = 6;
    B(at::kTalkStep) = 0;
}

// original 0x585A20, pick step 0 (and R2D's mode 4's): Sound_PlayEffect(0x102),
// the slide 4, the step up (read after the sound).
extern "C" void __cdecl MasterTalk_PanelsOpen(void) {
    SH_CALL(Sound_PlayEffect)(0x102);
    const auto step = static_cast<unsigned char>(B(at::kTalkStep) + 1);
    B(at::kTalkSlide) = 4;
    B(at::kTalkStep) = step;
}

namespace {

// The panels at the slide 0x9398D3 (read again at each draw): per member
// (Field_MemberCount read again each turn) MasterPanel_DrawMember at (0x11 -
// 36 * slide, 0x3E + 54 * slot) and MasterPanel_DrawStats at (0x89 + 40 *
// slide, the same y), each of the member's record index; then the title box
// Menu_DrawPanelBox(0x14, 0x10 - 10 * slide, 0x118, 0x13, the window colour).
void Panels() {
    for (unsigned slot = 0; slot < B(at::kMemberCount); ++slot) {
        const unsigned char* const member = At(at::kMembers + slot * at::kObjStride);
        const int y = static_cast<int>(54 * slot + 0x3E);
        SH_CALL(MasterPanel_DrawMember)(0x11 - 36 * static_cast<int>(B(at::kTalkSlide)), y, member[0], slot);
        SH_CALL(MasterPanel_DrawStats)(0x89 + 40 * static_cast<int>(B(at::kTalkSlide)), y, member[0]);
    }
    SH_CALL(Menu_DrawPanelBox)(0x14, 0x10 - 10 * static_cast<int>(B(at::kTalkSlide)), 0x118, 0x13, B(at::kColour));
}

}  // namespace

// original 0x585A50, pick step 1 (the panels sliding in): the slide down one,
// the panels; at 0 the pick 0x9398CE = 0 and the step up.
extern "C" void __cdecl MasterTalk_PanelsIn(void) {
    B(at::kTalkSlide) = static_cast<unsigned char>(B(at::kTalkSlide) - 1);
    Panels();
    if (B(at::kTalkSlide) != 0) return;
    const auto step = static_cast<unsigned char>(B(at::kTalkStep) + 1);
    B(at::kTalkPick) = 0;
    B(at::kTalkStep) = step;
}

// original 0x585B20, pick step 4 (sliding out): the slide up one, the panels;
// at 4 the step up.
extern "C" void __cdecl MasterTalk_PanelsOut(void) {
    B(at::kTalkSlide) = static_cast<unsigned char>(B(at::kTalkSlide) + 1);
    Panels();
    if (B(at::kTalkSlide) == 4) ++B(at::kTalkStep);
}

// original 0x586980, pick step 2: R2D's 0x5869A0 (the pick) with the master's
// message + 0xF and 0.
extern "C" void __cdecl MasterTalk_PickAsk(void) {
    SH_AT(void (__cdecl*)(unsigned, unsigned), at::kPickAsk)(MasterMessage(B(at::kMaster), 0xF), 0);
}

// ===========================================================================
// The panels
// ===========================================================================

// original 0x585BE0: the stats box of record r (its byte; 0x903A70 + 0xA4 r,
// read in place for any byte): Menu_DrawBox(x + 5, y + 3, 0x94, 0x2D, 0, the
// window colour); the four labels (DIV-0064's slots, read where they lie) with
// their words +0x24, +0x26, +0x2A, +0x28 through the number format in the 8
// px font; with a master (+0x1F not 0xFF) text record 0 = system message 0x111
// + it (8 long) and system message 0x34 at (x + 0xB, y + 0x22); the box's
// pieces: 0x664468, sixteen of 0x664494 8 apart, 0x6644A0.
extern "C" void __cdecl MasterPanel_DrawStats(int x, int y, unsigned record) {
    SH_CALL(Menu_DrawBox)(x + 5, y + 3, 0x94, 0x2D, 0, B(at::kColour));
    const U r = at::kRecords + (record & 0xFF) * at::kRecordStride;
    char* const buffer = reinterpret_cast<char*>(At(at::kPath));
    const auto* const text = reinterpret_cast<const unsigned char*>(buffer);
    const auto* const format = reinterpret_cast<const char*>(At(at::kFmtNumber));
    struct Stat { int lx, ly, nx, ny; U label, word; };
    const Stat stats[4] = {
        {x + 0xB, y + 8, x + 0x30, y + 0xA, at::kStatLabels, 0x24},
        {x + 0xB, y + 0x15, x + 0x30, y + 0x17, at::kStatLabels + 8, 0x26},
        {x + 0x53, y + 8, x + 0x78, y + 0xA, at::kStatLabels + 0x10, 0x2A},
        {x + 0x53, y + 0x15, x + 0x78, y + 0x17, at::kStatLabels + 0x18, 0x28},
    };
    for (const Stat& s : stats) {
        SH_CALL(Text_DrawAt)(s.lx, s.ly, 0, 0xFF, At(s.label));
        SH_CALL(Crt_sprintf)(buffer, format, static_cast<unsigned>(Word(At(r + s.word))));
        SH_CALL(Text_DrawFont8)(s.nx, s.ny, 0, text);
    }
    const unsigned char master = B(r + 0x1F);
    if (master != 0xFF) {
        const unsigned char* const name = SH_CALL(Msg_SystemPtr)(master + 0x111u);
        SH_CALL(TextRecord_Set)(0, 8, name);
        const unsigned char* const line = SH_CALL(Msg_SystemPtr)(0x34);
        SH_CALL(Text_DrawAt)(x + 0xB, y + 0x22, 0, 0xFF, line);
    }
    SH_CALL(Menu_DrawPieces)(x, y, At(at::kStatsPieces), 0);
    for (int k = 0; k < 16; ++k) SH_CALL(Menu_DrawPieces)(x + 8 * k, y, At(at::kStatsRow), 0);
    SH_CALL(Menu_DrawPieces)(x, y, At(at::kStatsEnd), 0);
}

// original 0x585DC0: the member panel of record r (its byte) in party slot s:
// Menu_DrawBox(x + 3, y + 3, 0x7D, 0x30, 0, the window colour); the face
// (MasterPanel_DrawFace at (x + 0x56, y + 2), the record's id +9, shade 2 when
// its status +0x10 has bit 7); the name (5 characters, Text_DrawAt at (x +
// 0x14, y + 3)); the level +0xA (the number format, 8 px font, (x + 0x16, y +
// 0x17)); the status words by +0x10 (bit 7 with bit 5: 0x66A0E8 on
// Frame_Counter's bit 5, else 0x66A0F0; bit 7 alone 0x66A0E8; bit 5 alone
// 0x66A0F0) in the small font at (x + 0x2E, y + 0x17); HP +0x18 / +0x20 at y
// + 0x1F, coloured 4 with the status's 0x2000, 2 at 1 or less; AP +0x1A /
// +0x22 at y + 0x27, coloured 4 at a quarter of the maximum or less, 2 at 0;
// the pieces 0x6643D8; the experience bar (MasterPanel_DrawExpBar at (x +
// 0x14, y + 0x12) of the record's level and +0xC); a draw mode (0, 0, 0xF, 0)
// committed; R2H's 0x59DB70 at (x + 9, y + 5): the cell (s + 3, 0x1E), the
// CLUT Gpu_GetClut(0x10, 0x1E0), shade 0x80.
extern "C" void __cdecl MasterPanel_DrawMember(int x, int y, unsigned record, unsigned slot) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x7D, 0x30, 0, B(at::kColour));
    const U r = at::kRecords + (record & 0xFF) * at::kRecordStride;
    const unsigned char id = B(r + 9);
    const unsigned shade = (B(r + 0x10) >> 6) & 2;
    SH_CALL(MasterPanel_DrawFace)(x + 0x56, y + 2, id, shade);
    SH_CALL(Text_DrawAt)(x + 0x14, y + 3, 0, 5, At(r));
    char* const buffer = reinterpret_cast<char*>(At(at::kPath));
    const auto* const text = reinterpret_cast<const unsigned char*>(buffer);
    SH_CALL(Crt_sprintf)(buffer, reinterpret_cast<const char*>(At(at::kFmtNumber)), static_cast<unsigned>(B(r + 0xA)));
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x17, 0, text);
    const unsigned status = Word(At(r + 0x10));
    U words = 0;
    if (status & 0x80) words = (status & 0x20) && (B(at::kFrameCounter) & 0x20) == 0 ? at::kStatusB : at::kStatusA;
    else if (status & 0x20) words = at::kStatusB;
    if (words != 0) SH_CALL(Text_DrawSmall)(x + 0x2E, y + 0x17, 1, 0x10, At(words));
    const auto* const left = reinterpret_cast<const char*>(At(at::kFmtLeft));
    const auto* const right = reinterpret_cast<const char*>(At(at::kFmtRight));
    int colour = (B(r + 0x11) & 0x20) ? 4 : 0;
    const unsigned hp = Word(At(r + 0x18));
    if (hp <= 1) colour = 2;
    SH_CALL(Crt_sprintf)(buffer, left, hp);
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x1F, colour, text);
    SH_CALL(Crt_sprintf)(buffer, right, static_cast<unsigned>(Word(At(r + 0x20))));
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x1F, 0, text);
    const unsigned ap = Word(At(r + 0x1A));
    colour = ap <= static_cast<unsigned>(Word(At(r + 0x22)) >> 2) ? 4 : 0;
    if (ap == 0) colour = 2;
    SH_CALL(Crt_sprintf)(buffer, left, ap);
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x27, colour, text);
    SH_CALL(Crt_sprintf)(buffer, right, static_cast<unsigned>(Word(At(r + 0x22))));
    SH_CALL(Text_DrawFont8)(x + 0x16, y + 0x27, 0, text);
    SH_CALL(Menu_DrawPieces)(x, y, At(at::kMemberPieces), 0);
    const U exp = L(r + 0xC);
    const unsigned level = B(r + 0xA);
    SH_CALL(MasterPanel_DrawExpBar)(x + 0x14, y + 0x12, record, level, exp);
    SH_CALL(Gpu_SetDrawMode)(*reinterpret_cast<unsigned char**>(At(at::kPacketNext)), 0, 0, 0xF, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned clut = SH_CALL(Gpu_GetClut)(0x10, 0x1E0);
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned, unsigned, unsigned), at::kGlyph)(
        x + 9, y + 5, static_cast<unsigned char>(slot + 3), 0x1E, clut, 0x80);
}

// original 0x586030: a flat line (Gpu_SetLineF2, Gpu_SetSemiTrans(0)) at the
// packet cursor from (x, y) - each the argument's low 16 bits as a float - to
// (x + 57 * (exp - ExpForLevel(level)) / (ExpForLevel(level + 1) -
// ExpForLevel(level)), y), colour (0x80, 0, 0), committed as (1, 0x20). The
// level + 1 is the byte's increment (level 0xFF gives 0); the product is a
// 32-bit wrap and the division unsigned, the sum the low 16 bits of x plus
// the quotient, made a float through a 64-bit integer. Equal totals (a level
// past 99 gives -1 for both, the table's equal steps 0): the bar's end is x
// itself.
extern "C" void __cdecl MasterPanel_DrawExpBar(int x, int y, unsigned record, unsigned level, unsigned exp) {
    unsigned char* const prim = *reinterpret_cast<unsigned char**>(At(at::kPacketNext));
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    const U x16 = static_cast<U>(x) & 0xFFFF;
    const auto fx = static_cast<float>(static_cast<int>(x16));
    PutF(prim + 8, fx);
    const auto fy = static_cast<float>(static_cast<int>(static_cast<U>(y) & 0xFFFF));
    PutF(prim + 0xC, fy);
    const U here = static_cast<U>(SH_CALL(MasterPanel_ExpForLevel)(record, level));
    const U next = static_cast<U>(SH_CALL(MasterPanel_ExpForLevel)(record, (level & 0xFFFFFF00u) | ((level + 1) & 0xFF)));
    const U span = next - here;
    if (span == 0) {
        PutF(prim + 0x14, fx);
    } else {
        const U end = (57u * exp - 57u * here) / span + x16;
        PutF(prim + 0x14, static_cast<float>(static_cast<double>(static_cast<std::uint64_t>(end))));
    }
    PutF(prim + 0x18, fy);
    prim[4] = 0x80;
    prim[5] = 0;
    prim[6] = 0;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x586110 (Char_ExpForLevel's body, instruction for instruction):
// the experience to reach `level` (a byte): -1 above 99; else the sum of the
// first `level` u16 steps of the member's (a byte) row of Char_ExpTable, 0x318
// bytes a member and 8 a step (read in place for any member byte).
extern "C" int __cdecl MasterPanel_ExpForLevel(unsigned record, unsigned level) {
    const unsigned n = level & 0xFF;
    if (n > 0x63) return -1;
    U sum = 0;
    const U row = at::kExpTable + (record & 0xFF) * 0x318u;
    for (unsigned i = 0; i < n; ++i) sum += Word(At(row + 8 * i));
    return static_cast<int>(sum);
}

namespace {

// One of Menu_DrawPanelBox's four edge quads: Gpu_SetPolyFT4 and
// Gpu_SetSemiTrans(1) at the packet cursor (read again), the corners given as
// float bits and the texture bytes, colour 0xAC grey, the CLUT
// Gpu_GetClut(clut_x, 0x1E1), page 0xF, committed as (1, 0x48).
struct Quad {
    U x[4], y[4];
    unsigned char u[4], v[4];
};
void EdgeQuad(const Quad& q, int clut_x) {
    unsigned char* const p = *reinterpret_cast<unsigned char**>(At(at::kPacketNext));
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    for (unsigned i = 0; i < 4; ++i) {
        std::memcpy(p + 8 + 0x10 * i, &q.x[i], 4);
        std::memcpy(p + 0xC + 0x10 * i, &q.y[i], 4);
        p[0x14 + 0x10 * i] = q.u[i];
        p[0x15 + 0x10 * i] = q.v[i];
    }
    p[4] = 0xAC;
    p[5] = 0xAC;
    p[6] = 0xAC;
    const unsigned clut = SH_CALL(Gpu_GetClut)(clut_x, 0x1E1);
    PutW(Key(p + 0x16), clut);
    PutW(Key(p + 0x26), 0xF);
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// A draw mode with a texture window RECT of its own: 8 bytes taken at the
// packet cursor (x 0, y, w, h), then Gpu_SetDrawMode(cursor, 0, 1, 0xF, the
// RECT) and Gfx_CommitPrim(1, 0xC).
void WindowMode(unsigned y, unsigned size) {
    unsigned char* const rect = *reinterpret_cast<unsigned char**>(At(at::kPacketNext));
    PutL(at::kPacketNext, Key(rect + 8));
    PutW(Key(rect), 0);
    PutW(Key(rect + 2), y);
    PutW(Key(rect + 4), size);
    PutW(Key(rect + 6), size);
    SH_CALL(Gpu_SetDrawMode)(*reinterpret_cast<unsigned char**>(At(at::kPacketNext)), 0, 1, 0xF, Key(rect));
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
}

}  // namespace

// original 0x586160: a panel box at (x, y), w by h, colour c (a byte). A draw
// mode with the texture window (0, 0xF0, 16, 16); four FT4 edge quads from
// the 16 x 16 cell at CLUT (c * 32 + 16, 0x1E1), page 0xF, grey 0xAC - the
// left (x .. x + 2), the middle's two halves (x + 2 .. x + 2 + hw, u to hw's
// byte; then .. x + 2 + 2 hw + odd, u to odd + hw, with hw = ((w + 1) &
// 0xFFFF - 4) >> 1 (arithmetic) as a short and odd = (w + 1) & 1), the right
// (x + w - 1 .. x + w + 1). Menu_DrawTitleBox's body (0x574AB0, ours in
// menu_windows.cpp) to the instruction, a second copy Capcom's compiler
// kept. Each corner's float made as the original makes it (x and y are the
// arguments' low 16 bits; y + 2.0, (y + h + 1) - 3.0 and - 1.0 from the
// constants at 0x5C41C0, 0x5C41BC, 0x5C41B8 read in place); then the window
// (0, 0, 256, 256) and Menu_DrawOutline(x + 2, y + 2, w - 4, h - 4, 0) with
// the arguments' whole words. The original also writes h - 2's byte and
// x + w - 1 / its float over its own argument slots, which no caller reads.
extern "C" void __cdecl Menu_DrawPanelBox(int x, int y, int w, int h, unsigned colour) {
    WindowMode(0xF0, 0x10);
    const U w1 = static_cast<U>(w) + 1, h1 = static_cast<U>(h) + 1;
    const U x16 = static_cast<U>(x) & 0xFFFF, y16 = static_cast<U>(y) & 0xFFFF;
    const float fx = static_cast<float>(static_cast<int>(x16));
    const float fy = static_cast<float>(static_cast<int>(y16));
    const float fx2 = static_cast<float>(static_cast<int>(x16 + 2));
    const float fbottom = static_cast<float>(static_cast<int>((h1 & 0xFFFF) + y16));
    const float ftop2 = static_cast<float>(static_cast<double>(fy) + static_cast<double>(F(at::kBoxAdd)));
    const float fbottom3 = static_cast<float>(static_cast<double>(fbottom) - static_cast<double>(F(at::kBoxSub)));
    const auto vh = static_cast<unsigned char>(h1);
    const int clut_x = static_cast<int>(((colour & 0xFF) << 5) + 0x10);
    // the left edge
    EdgeQuad({{Bits(fx), Bits(fx2), Bits(fx), Bits(fx2)},
              {Bits(ftop2), Bits(fy), Bits(fbottom3), Bits(fbottom)},
              {0, 2, 0, 2},
              {2, 0, static_cast<unsigned char>(vh - 3), vh}},
             clut_x);
    // the middle's first half
    const U w16 = w1 & 0xFFFF;
    const int half = static_cast<int>(w16 - 4) >> 1;
    const unsigned char odd = static_cast<unsigned char>(w1 & 1);
    const int half_s = static_cast<short>(half);
    const float fmid = static_cast<float>(static_cast<int>(x16) + half_s + 2);
    const auto u1 = static_cast<unsigned char>(half);
    EdgeQuad({{Bits(fx2), Bits(fmid), Bits(fx2), Bits(fmid)},
              {Bits(fy), Bits(fy), Bits(fbottom), Bits(fbottom)},
              {0, u1, 0, u1},
              {0, 0, vh, vh}},
             clut_x);
    // the middle's second half
    const float fend = static_cast<float>(static_cast<int>(x16) + static_cast<short>(odd) + 2 * half_s + 2);
    const auto u2 = static_cast<unsigned char>(odd + static_cast<unsigned char>(half));
    EdgeQuad({{Bits(fmid), Bits(fend), Bits(fmid), Bits(fend)},
              {Bits(fy), Bits(fy), Bits(fbottom), Bits(fbottom)},
              {0, u2, 0, u2},
              {0, 0, vh, vh}},
             clut_x);
    // the right edge
    const U right = x16 + w16;
    const float fr0 = static_cast<float>(static_cast<int>(right - 2));
    const float fr1 = static_cast<float>(static_cast<int>(right));
    const float fbottom1 = static_cast<float>(static_cast<double>(fbottom) - static_cast<double>(F(at::kBoxSubBottom)));
    EdgeQuad({{Bits(fr0), Bits(fr1), Bits(fr0), Bits(fr1)},
              {Bits(fy), Bits(ftop2), Bits(fbottom1), Bits(fbottom3)},
              {0, 2, 0, 2},
              {0, 2, static_cast<unsigned char>(vh - 1), static_cast<unsigned char>(vh - 3)}},
             clut_x);
    WindowMode(0, 0x100);
    SH_CALL(Menu_DrawOutline)(x + 2, y + 2, static_cast<int>(w1 - 5), static_cast<int>(h1 - 5), 0);
}

// original 0x586570: a party member's face, 40 x 48, at (x, y) (low 16 bits
// as floats): id 4 as FormOf; a draw mode (0, 1, 0x1E, 0) committed; a sprite
// (Gpu_SetSprt) at the packet cursor (read again), the colour by shade (0
// grey 0x80, 1 dark 0x30, else (0x40, 0x40, 0x80)), u / v the face table's
// first two bytes (0x6644B8 + 4 id, read in place for any id), the CLUT word
// (row + 0x1E0) << 6 | x byte >> 4; committed as (1, 0x1C).
extern "C" void __cdecl MasterPanel_DrawFace(int x, int y, unsigned id, unsigned shade) {
    const unsigned char form = FormOf(static_cast<unsigned char>(id));
    const U entry = at::kFaceTable + 4u * form;
    SH_CALL(Gpu_SetDrawMode)(*reinterpret_cast<unsigned char**>(At(at::kPacketNext)), 0, 1, 0x1E, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = *reinterpret_cast<unsigned char**>(At(at::kPacketNext));
    SH_CALL(Gpu_SetSprt)(p);
    const auto s = static_cast<unsigned char>(shade);
    if (s == 0) {
        p[6] = 0x80;
        p[5] = 0x80;
        p[4] = 0x80;
    } else if (s == 1) {
        p[6] = 0x30;
        p[5] = 0x30;
        p[4] = 0x30;
    } else {
        p[6] = 0x80;
        p[5] = 0x40;
        p[4] = 0x40;
    }
    PutF(p + 8, static_cast<float>(static_cast<int>(static_cast<U>(x) & 0xFFFF)));
    PutF(p + 0xC, static_cast<float>(static_cast<int>(static_cast<U>(y) & 0xFFFF)));
    p[0x14] = B(entry);
    p[0x15] = B(entry + 1);
    const unsigned clut = (((B(entry + 3) + 0x1E0u) << 6) | (B(entry + 2) >> 4)) & 0xFFFF;
    PutW(Key(p + 0x18), 0x28);
    PutW(Key(p + 0x1A), 0x30);
    PutW(Key(p + 0x16), clut);
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

#pragma clang attribute pop

void Rest2C_Inject() {
    if (bof3::WantsShadow("rest_2c")) rest_2c::SelfTest();
    // DIVERGENCE DIV-0076: after the self-test, which compares Capcom's summary.
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("SaveSummaryRecord0",
                         static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_summary_record0)), &was, &is, 1);
        bof3::Log("DIV-0076    the save slot's summary names record 0, whose level it already shows");
    }
    BOF3_INJECT(MasterFigure_DrawFaded);
    BOF3_INJECT(MasterFigure_TurnHome);
    BOF3_INJECT(MasterFigure_Settle);
    BOF3_INJECT(MasterFigure_Hold);
    BOF3_INJECT(MasterFigure_TurnOn);
    BOF3_INJECT(InnPrompt_NotEnough);
    BOF3_INJECT(FieldSave_Write);
    BOF3_INJECT(FieldSave_Written);
    BOF3_INJECT(FieldSave_PromptAnswer);
    BOF3_INJECT(Inn_TitleOut);
    BOF3_INJECT(Rest_Dispatch);
    BOF3_INJECT(Rest_LoadJingle);
    BOF3_INJECT(Rest_WaitJingle);
    BOF3_INJECT(Rest_Restore);
    BOF3_INJECT(Rest_End);
    BOF3_INJECT(Rest_EndAfterMessage);
    BOF3_INJECT(Save_BuildBlock);
    BOF3_INJECT(Save_QuickWrite);
    BOF3_INJECT(PartyForm_Dispatch);
    BOF3_INJECT(ShopSell_Dispatch);
    BOF3_INJECT(ShopSell_Open);
    BOF3_INJECT(ShopSell_Leave);
    BOF3_INJECT(ShopSell_SellStep);
    BOF3_INJECT(ShopSell_LeaveWait);
    BOF3_INJECT(ShopBrowse_Dispatch);
    BOF3_INJECT(ShopBrowse_OpenStep);
    BOF3_INJECT(ShopBrowse_Open);
    BOF3_INJECT(ShopBrowse_OpenWait);
    BOF3_INJECT(ShopBrowse_ChooseStep);
    BOF3_INJECT(ShopBrowse_Choose);
    BOF3_INJECT(ShopBrowse_Detail);
    BOF3_INJECT(ShopBrowse_DetailClose);
    BOF3_INJECT(ShopBrowse_CloseStep);
    BOF3_INJECT(ShopBrowse_CloseNext);
    BOF3_INJECT(ShopBrowse_End);
    BOF3_INJECT(ShopResist_Dispatch);
    BOF3_INJECT(SharedList_Dispatch);
    BOF3_INJECT(MasterTalk_Reset);
    BOF3_INJECT(MasterTalk_PanelsOpen);
    BOF3_INJECT(MasterTalk_PanelsIn);
    BOF3_INJECT(MasterTalk_PanelsOut);
    BOF3_INJECT(MasterPanel_DrawStats);
    BOF3_INJECT(MasterPanel_DrawMember);
    BOF3_INJECT(MasterPanel_DrawExpBar);
    BOF3_INJECT(MasterPanel_ExpForLevel);
    BOF3_INJECT(Menu_DrawPanelBox);
    BOF3_INJECT(MasterPanel_DrawFace);
    BOF3_INJECT(MasterTalk_Dispatch);
    BOF3_INJECT(MasterTalk_Begin);
    BOF3_INJECT(MasterTalk_IntroStep);
    BOF3_INJECT(MasterTalk_IntroSay);
    BOF3_INJECT(MasterTalk_IntroWait);
    BOF3_INJECT(MasterTalk_AskStep);
    BOF3_INJECT(MasterTalk_Say5);
    BOF3_INJECT(MasterTalk_Say6);
    BOF3_INJECT(MasterTalk_CheckAllPupils);
    BOF3_INJECT(MasterTalk_Say8);
    BOF3_INJECT(MasterTalk_CheckAnyPupil);
    BOF3_INJECT(MasterTalk_SayFarewell);
    BOF3_INJECT(MasterTalk_PickStep);
    BOF3_INJECT(MasterTalk_PickAsk);
}
