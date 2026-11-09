// Moved here by tools/move_funcs.py from rest_4b.cpp, rest_4c.cpp, rest_4d.cpp (docs/source-layout-round.md).
#include "game/community/commu_board.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/community/commu_board_callees.h"
#include "game/faerie_name.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/rest_4b.h"
#include "game/rest_4b_callees.h"
#include "game/rest_4c_callees.h"
#include "game/rest_4d.h"
#include "game/rest_4d_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = commu_board::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(U address) { return *At(address); }

unsigned W(U address) { return Word(At(address)); }

U L(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

void PutW(U address, unsigned v) { SetWord(At(address), v); }

void PutL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }

short S16(U address) { return static_cast<short>(Word(At(address))); }

void PutF(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

void PutFBits(unsigned char* p, U bits) { std::memcpy(p, &bits, sizeof bits); }

void PutW(unsigned char* p, unsigned v) { SetWord(p, v); }

unsigned char* Packet() { return Gfx_PacketNext; }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (the run of
// code words from the reader's cell, docs/rest_4b.md section 3). Past it the
// original jumps through whatever follows; no state writes such an index, so
// ours aborts (section 5). The entry is called as the cell holds it: Capcom's
// address in the game (Inject's jmp to ours where it is ours), a recorder
// while the fuzz runs.
using Handler = void (__cdecl*)();

void Jump(U table, unsigned count, int index, const char* who) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: state %d is outside its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * static_cast<U>(index))));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * static_cast<U>(index))))();
}

// The community record k (0x9046D0 + 8k): ours writes only the 60; past them
// the original writes on into the save block (section 5).
U Member(unsigned k, const char* who) {
    if (k >= 60)
        bof3::Fatal("%s: community record %u is past the 60 at 0x9046D0; the original writes at 0x%X", who, k,
                    static_cast<unsigned>(Community_Records_addr + 8 * k));
    return ::bof3::Img(Community_Records_addr) + 8 * k;
}

// A slot's bytes (0x9048A8 + 8 * slot, the slots 1..8 at 0x9048B0): read in
// place for any slot byte, as the original reads them.
U Slot(unsigned slot) { return ::bof3::Img(at::kSlots.v) + 8 * slot; }

// ... and written only for the eight (section 5).
U SlotToWrite(unsigned slot, const char* who) {
    if (slot - 1 >= 8)
        bof3::Fatal("%s: slot %u is outside the eight at 0x9048B0; the original writes at 0x%X", who, slot,
                    static_cast<unsigned>((at::kSlots + 8 * slot).v));
    return ::bof3::Img(at::kSlots.v) + 8 * slot;
}

// The field tail's state byte, moved by one as the original's inc / dec do.
void StateBy(int d) { AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + d); }

// The flashing intensity three draws share: from Frame_Counter's bits 1..2,
// counted up while bit 3 is set and down while it is clear, +0x3F.
unsigned char Flash() {
    unsigned char f = B(::bof3::Img(at::kFrame.v));
    if (!(f & 8)) f = static_cast<unsigned char>(~f);
    return static_cast<unsigned char>(((f & 6) << 5) + 0x3F);
}

// A slot's column offset: (slot - 1) % 4 as the original's signed remainder
// (slot 0 gives -1), times 52.
int Column(unsigned char slot) { return ((static_cast<int>(slot) - 1) % 4) * 52; }

void R4EHand(int x, int y, unsigned flash) {
    SH_AT(void (__cdecl*)(int, int, unsigned), at::kR4EHand.v)(x, y, flash);
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

namespace {

// The hand and the card at the list cursor of the slot 0x9039F5 (slot 0: a row
// at the panel's top; else a 5-wide grid), as the Steps states draw them: R4E's
// 0x45ECC0 and the card of the cursor's record - beside the hand when steady
// (flash 0), at (0xA0, 0x3E) when flashing.
void CursorAndCard(unsigned char slot, unsigned flash) {
    const int c = Rest4b_ListCursor;
    int x, y, cx, cy;
    if (slot == 0) {
        x = static_cast<short>(c) * 8 + 0x28;
        y = 0x39;
        cx = static_cast<short>(c) * 8 + 0x30;
        cy = 0x42;
    } else {
        const int row = (c / 5) * 7 + (static_cast<int>(slot) - 9) * 54;
        x = (c % 5) * 8 + 0xFA;
        y = row + 0x44;
        cx = (c % 5) * 8 + 0xA8;
        cy = row + 0x4C;
    }
    R4EHand(x, y, flash);
    const unsigned char cursor = static_cast<unsigned char>(Rest4b_ListCursor);
    const unsigned char arg = AreaW0c_TailSub;
    const unsigned idx = SH_CALL(Commu_NthInSlot)(arg, cursor);
    if (flash == 0)
        SH_CALL(CommuBoard_DrawRecordCard)(cx, cy, idx, 0);
    else
        SH_CALL(CommuBoard_DrawRecordCard)(0xA0, 0x3E, idx, 1);
}

// The lists' hand: at the slot's first record position, flashing (the three
// list steps and the confirm step draw it first).
void HandAtSlot() {
    short x = 0, y = 0;
    const unsigned char arg = AreaW0c_TailSub;
    SH_CALL(CommuBoard_SlotRecordXY)(&x, &y, arg, 0);
    R4EHand(x, y, 1);
}

}  // namespace

// DIV-0064 group 13: the list boxes are 0x30 wide, three Chinese glyphs and a
// margin, and their frames four 8-px columns. Once the overlay has written
// the lists the box is the widest line plus 16, rounded up to 8 - what the
// discs did: the US and German overlays size it 0x50 for their eight-letter
// words, the French 0x60 for its ten (the `addiu $a2` constants of their
// COMMU01.EMI, 2026-10-10) - and the frame follows, a column per 8.
int ListBoxWidth() {
    if (!Labels_Written(13)) return 0x30;
    const unsigned w = (Labels_MaxWidth(13) + 16 + 7) & ~7u;
    return w > 0x30 ? static_cast<int>(w) : 0x30;
}

#pragma clang attribute pop

namespace {

unsigned char& BRest4c(U address) { return At(address)[0]; }

void SetL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }

U Key(const void* p) { return Low32(p); }

void PutFloat(unsigned char* p, unsigned offset, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p + offset, &f, sizeof f);
}

void PutWord(unsigned char* p, unsigned offset, unsigned v) { SetWord(p + offset, v); }

// A word argument read as movsx word.
int S16Rest4c(U v) { return static_cast<std::int16_t>(v & 0xFFFF); }

// jmp [table + 4 * byte]: the table's `entries` handlers, read in place (the
// fuzz swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after.
void Dispatch(const char* who, const unsigned long* table, unsigned entries, U byte_address) {
    const unsigned index = BRest4c(byte_address);
    if (index >= entries)
        bof3::Fatal("%s: the byte 0x%X is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4c.md section 7)",
                    who, (unsigned)::bof3::VaOf(byte_address), index, entries, (unsigned)::bof3::VaOf(Key(table)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

// An index into a run of bytes or a table outside its room: the original reads
// or writes what lies past it (docs/rest_4c.md section 7).
void Room(const char* who, const char* what, int index, int lo, int hi) {
    if (index < lo || index > hi)
        bof3::Fatal("%s: %s is %d, outside %d..%d - the original reads past its room (docs/rest_4c.md section 7)", who,
                    what, index, lo, hi);
}

// --- the callees -----------------------------------------------------------------------
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

void Message(unsigned id) { SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id)); }

void Card(int x, int y, unsigned digit) { SH_CALL(Commu_DrawCard)(x, y, digit); }

void Piece(int x, int y, unsigned piece) { SH_CALL(Commu_DrawPiece)(x, y, piece); }

void ZennyBox(int x, int y) { SH_CALL(Commu_DrawZennyBox)(x, y); }

void StakeBox(int x, int y, unsigned digits) { SH_CALL(Commu_DrawStakeBox)(x, y, digits); }

void Row(unsigned count, unsigned shown, unsigned picks) { SH_CALL(CommuHiLo_DrawRow)(count, shown, picks); }

void Choice(int x, int y, unsigned label, unsigned mode) { SH_CALL(CommuHiLo_DrawChoice)(x, y, label, mode); }

void Choices(int x, int y, unsigned selected) { SH_CALL(CommuHiLo_DrawChoices)(x, y, selected); }

void GuessPanel(int x, int y, unsigned flag) {
    SH_AT(void (__cdecl*)(int, int, unsigned), at::kGuessPanel.v)(x, y, flag);
}

void GuessRow(int x, int y, unsigned row, unsigned flag) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kGuessRow.v)(x, y, row, flag);
}

void Secret(int x, int y, unsigned show) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kSecret.v)(x, y, show); }

unsigned Rnd() { return static_cast<unsigned>(SH_CALL(Rand)()); }

int Request() { return Field_Request; }

const unsigned char* Text(U address) { return At(address); }

char* Buffer() { return reinterpret_cast<char*>(At(::bof3::Img(bof3::addr::Text_Records) - 0x140)); }   // 0x904BA0

// The first game's two boxes at their resting places (the money box at 0x26,
// 0x10, the stake box at 0xA8, 0x10, plain) - the tail of most of its states.
void HiLoBoxes() {
    ZennyBox(0x26, 0x10);
    StakeBox(0xA8, 0x10, 0);
}

// The second game's panels at rest: the guess panel (flag), the secret (show),
// the row (its flag), then the two boxes at 0xCC.
void HitBlowBoxes() {
    ZennyBox(0xCC, 0x10);
    StakeBox(0xCC, 0x3C, 0);
}

// The phases' music: the field's track kept and faded (10 frames).
void KeepMusicAndFade() {
    Rest4d_KeptTrack = Music_Track;
    SH_CALL(Music_FadeOutStop)(10);
    Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
}

// The two games' close phase (0x45AF00, 0x45C3A0): state 0 fades the music
// once no message is up; then, once the load is done, the kept track plays (8
// frames) and the field hook's tail state moves on.
void ClosePhase() {
    if (Rest4b_BoardStep == 0) {
        if (Request() == 2) return;
        SH_CALL(Music_FadeOutStop)(10);
        Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
        return;
    }
    if (SH_CALL(File_LoadDone)() == 0) return;
    SH_CALL(Music_Play)(L(::bof3::Img(at::kMusic.v)) & 0xFF, 8);
    AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
}

}  // namespace

// The shown values (0x675F8D's bits 0..7, re-read each time) at (0x30 + 32 i,
// 0x6C): value i + 1.
static void ShownValues() {
    for (int i = 0; i < 8; ++i)
        if (Rest4c_Cursor & (1 << i)) Card(32 * i + 0x30, 0x6C, BRest4c(::bof3::Img(at::kCards.v) + 1 + i));
}

namespace {

void HitBlowPanels(unsigned panel, unsigned show, unsigned row_flag) {
    GuessPanel(0x10, 0x18, panel);
    Secret(0x68, 0x40, show);
    GuessRow(0x68, 0x78, static_cast<unsigned char>(Rest4c_Count), row_flag);
    HitBlowBoxes();
}

// --- the callees of other groups of this wave, by address ---------------------------
void BoardCell(int x, int y, unsigned value) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kBoardCell.v)(x, y, value); }

void BoardPiece(int x, int y, unsigned piece) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kBoardPiece.v)(x, y, piece); }

char* Scratch() { return reinterpret_cast<char*>(At(::bof3::Img(at::kTextScratch.v))); }

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x457640: CommuTail14_States[3]: jmp [CommuBoard_States + 4 * the u8
// 0x939A3E], unchecked (ours aborts past 14).
extern "C" void __cdecl CommuBoard_Dispatch(void) {
    Jump(::bof3::Img(CommuBoard_States_addr), 14, Rest4b_BoardState, "CommuBoard_Dispatch");
}

// original 0x457650: CommuBoard_States[0]: the grid cursor at mode 0 with its
// bytes 0, 1; the help line 0xB5; the next board state.
extern "C" void __cdecl CommuBoard_Init(void) {
    Rest4b_CursorB = 1;
    Rest4b_CursorA = 0;
    Rest4b_CursorMode = 0;
    Rest4b_Help = 0xB5;
    Rest4b_BoardState = static_cast<unsigned char>(Rest4b_BoardState + 1);
}

// original 0x457680: CommuBoard_States[1]: the grid. Cancel: the help line
// 0xFFFF, the tail's next state. Confirm: the cursor kept (0x675F84..86), the
// board's step and the list cursor 0, then by the cursor's mode - 0 and 2: the
// slot 0x9039F5 must hold a record (Commu_CountInSlot), 1: the slot must not
// pass the area's limit (0x653601 by Game_AreaNumber); allowed, sound 0x104,
// the slot's help line (CommuBoard_SlotHelp) and the next board state; else
// sound 0x107 (any other mode too). Neither: the grid cursor moved on
// 0x9039F5. Then the panel and the slot's lines, flashing.
extern "C" void __cdecl CommuBoard_Grid(void) {
    const unsigned pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        Rest4b_Help = 0xFFFF;
        StateBy(1);
    } else if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        Rest4b_SavedB = Rest4b_CursorB;
        const unsigned char a = Rest4b_CursorA;
        const unsigned char mode = Rest4b_CursorMode;
        Rest4b_SavedMode = mode;
        Rest4b_SavedA = a;
        Rest4b_BoardStep = 0;
        Rest4b_ListCursor = 0;
        int allowed = -1;
        if (mode == 0 || mode == 2) {
            const unsigned char count = SH_CALL(Commu_CountInSlot)(AreaW0c_TailSub);
            allowed = count != 0;
        } else if (mode == 1) {
            const unsigned char arg = AreaW0c_TailSub;
            allowed = arg <= B(::bof3::Img(at::kAreaLimits.v) + static_cast<std::uint32_t>(Game_AreaNumber));
        }
        if (allowed == 1) {
            SH_CALL(Sound_PlayEffect)(0x104);
            SH_CALL(CommuBoard_SlotHelp)();
            Rest4b_BoardState = static_cast<unsigned char>(Rest4b_BoardState + 1);
        } else {
            SH_CALL(Sound_PlayEffect)(0x107);
        }
    } else {
        SH_CALL(CommuBoard_MoveGridCursor)(At(::bof3::Img(AreaW0c_TailSub_addr)));
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(AreaW0c_TailSub, 0);
}

// original 0x457780: CommuBoard_States[2]: jmp [CommuBoard_ModeStates + 4 * the
// u8 0x675F86] (the kept cursor mode), unchecked (ours aborts past 11).
extern "C" void __cdecl CommuBoard_ModeDispatch(void) {
    Jump(::bof3::Img(CommuBoard_ModeStates_addr), 11, Rest4b_SavedMode, "CommuBoard_ModeDispatch");
}

// original 0x457790: CommuBoard_States[3], [5] (ModeStates[0], [2]): jmp
// [CommuBoard_Steps + 4 * the u8 0x939A40], unchecked (ours aborts past 8).
extern "C" void __cdecl CommuBoard_StepDispatch(void) {
    Jump(::bof3::Img(CommuBoard_Steps_addr), 8, Rest4b_BoardStep, "CommuBoard_StepDispatch");
}

// original 0x4577A0: CommuBoard_Steps[0]: a record of the slot picked. Cancel:
// sound 0x106, the board's state back, help 0xB5. Confirm: sound 0x104, the
// picked slot 0x675F7F = 0x9039F5, the next step, help 0x44. Neither: the list
// cursor stepped over the slot's records by the auto-repeated keys (0x2000 up,
// back to 0 at the count; 0x8000 down, to count - 1 below 0; sound 0x100 when
// it moved). Then the panel, the slot's lines (steady), the hand and the card.
extern "C" void __cdecl CommuBoard_PickRecord(void) {
    const unsigned pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        Rest4b_BoardState = static_cast<unsigned char>(Rest4b_BoardState - 1);
        Rest4b_Help = 0xB5;
    } else if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x104);
        Rest4b_Pick = AreaW0c_TailSub;
        Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
        Rest4b_Help = 0x44;
    } else {
        const unsigned char count = SH_CALL(Commu_CountInSlot)(AreaW0c_TailSub);
        const unsigned keys = SH_CALL(Input_AutoRepeat)(static_cast<std::uint32_t>(Input_Pressed) & 0xA000);
        const unsigned char old = static_cast<unsigned char>(Rest4b_ListCursor);
        unsigned char c = old;
        if (keys & 0x2000) {
            ++c;
            Rest4b_ListCursor = c;
            if (static_cast<signed char>(c) == static_cast<int>(count)) {
                c = 0;
                Rest4b_ListCursor = c;
            }
        } else if (keys & 0x8000) {
            --c;
            Rest4b_ListCursor = c;
            if (static_cast<signed char>(c) < 0) {
                c = static_cast<unsigned char>(count - 1);
                Rest4b_ListCursor = c;
            }
        }
        if (static_cast<signed char>(c) != static_cast<int>(old)) SH_CALL(Sound_PlayEffect)(0x100);
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(AreaW0c_TailSub, 1);
    CursorAndCard(AreaW0c_TailSub, 0);
}

// original 0x457990: CommuBoard_Steps[1]: the slot the record moves to. Cancel:
// sound 0x106, the slot's help, the step back (CommuBoard_CancelStep). Confirm:
// CommuBoard_PlaceRecord(0); placed, help 0xB5. Neither: the grid cursor moved
// on 0x675F7F and its help line. Then the panel, both slots' lines, and unless
// placed the hand (flashing) and the card.
extern "C" void __cdecl CommuBoard_MoveRecord(void) {
    const unsigned pressed = Input_Pressed;
    unsigned char placed = 0;
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(CommuBoard_SlotHelp)();
        SH_CALL(CommuBoard_CancelStep)();
    } else if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        placed = SH_CALL(CommuBoard_PlaceRecord)(0);
        if (placed) Rest4b_Help = 0xB5;
    } else {
        SH_CALL(CommuBoard_MoveGridCursor)(At(::bof3::Img(Rest4b_Pick_addr)));
        SH_CALL(CommuBoard_PickHelp)(At(::bof3::Img(Rest4b_Pick_addr)));
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(AreaW0c_TailSub, 1);
    SH_CALL(CommuBoard_DrawSlotLines)(B(::bof3::Img(at::kPick.v)), 0);
    if (placed) return;
    CursorAndCard(AreaW0c_TailSub, 1);
}

// original 0x457AD0: CommuBoard_States[4] (ModeStates[1]): jmp
// [CommuBoard_StepsB + 4 * the u8 0x939A40], unchecked (ours aborts past 6).
extern "C" void __cdecl CommuBoard_StepDispatchB(void) {
    Jump(::bof3::Img(CommuBoard_StepsB_addr), 6, Rest4b_BoardStep, "CommuBoard_StepDispatchB");
}

// original 0x457AE0: CommuBoard_StepsB[0]: the slot's records and its kind.
// Cancel: sound 0x106, the board's state back, help 0xB5. Confirm: sound
// 0x103; at a record (list cursor not 0) help 0x44, 0x675F7F = 0x9039F5, the
// next step; at 0 the slot's kind picked: 0x675F7F = the kind - 4 (0 for kind
// 0), 0x675F7E, 0x675F7D 0, 0x675F83, 0x675F82 0xFF, step 2. Neither: the list
// cursor stepped over 0..count (0x8000 down, to the count below 0; 0x2000 up,
// to 0 past the count). Then the panel, the slot's lines (steady) and the hand
// at the record position (CommuBoard_SlotRecordXY): at 0 alone, else beside it
// with the card of the record before it.
extern "C" void __cdecl CommuBoard_PickRecordB(void) {
    const unsigned pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        Rest4b_BoardState = static_cast<unsigned char>(Rest4b_BoardState - 1);
        Rest4b_Help = 0xB5;
    } else if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x103);
        if (static_cast<unsigned char>(Rest4b_ListCursor) != 0) {
            Rest4b_Help = 0x44;
            Rest4b_Pick = AreaW0c_TailSub;
            Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
        } else {
            const unsigned char kind = B(Slot(AreaW0c_TailSub));
            Rest4b_Pick = kind == 0 ? 0 : static_cast<unsigned char>(kind - 4);
            Rest4b_PickB = 0;
            Rest4b_PickC = 0;
            Rest4b_PickD = 0xFF;
            Rest4b_PickE = 0xFF;
            Rest4b_BoardStep = 2;
        }
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
        const unsigned char old = static_cast<unsigned char>(Rest4b_ListCursor);
        const unsigned char count = SH_CALL(Commu_CountInSlot)(AreaW0c_TailSub);
        if (keys & 0x8000) {
            const unsigned char c = static_cast<unsigned char>(static_cast<unsigned char>(Rest4b_ListCursor) - 1);
            Rest4b_ListCursor = c;
            if (static_cast<signed char>(c) < 0) Rest4b_ListCursor = count;
        } else if (keys & 0x2000) {
            const unsigned char c = static_cast<unsigned char>(static_cast<unsigned char>(Rest4b_ListCursor) + 1);
            Rest4b_ListCursor = c;
            if (static_cast<signed char>(c) > static_cast<int>(count)) Rest4b_ListCursor = 0;
        }
        if (Rest4b_ListCursor != static_cast<int>(old)) SH_CALL(Sound_PlayEffect)(0x100);
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(AreaW0c_TailSub, 1);
    short x = 0, y = 0;
    {
        const unsigned char cursor = static_cast<unsigned char>(Rest4b_ListCursor);
        const unsigned char arg = AreaW0c_TailSub;
        SH_CALL(CommuBoard_SlotRecordXY)(&x, &y, arg, cursor);
    }
    if (static_cast<unsigned char>(Rest4b_ListCursor) == 0) {
        R4EHand(x, y, 0);
        return;
    }
    R4EHand(x + 4, y, 0);
    const unsigned char before = static_cast<unsigned char>(static_cast<unsigned char>(Rest4b_ListCursor) - 1);
    const unsigned char arg = AreaW0c_TailSub;
    const unsigned idx = SH_CALL(Commu_NthInSlot)(arg, before);
    SH_CALL(CommuBoard_DrawRecordCard)(x + 0x10, y - 8, idx, 0);
}

// original 0x457CE0: CommuBoard_StepsB[1]: as CommuBoard_MoveRecord, with
// CommuBoard_PlaceRecord(1), no sound of its own on cancel (CancelStep first,
// then the slot's help), and the hand beside the record position.
extern "C" void __cdecl CommuBoard_MoveRecordB(void) {
    const unsigned pressed = Input_Pressed;
    unsigned char placed = 0;
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        SH_CALL(CommuBoard_CancelStep)();
        SH_CALL(CommuBoard_SlotHelp)();
    } else if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        placed = SH_CALL(CommuBoard_PlaceRecord)(1);
        if (placed) Rest4b_Help = 0xB5;
    } else {
        SH_CALL(CommuBoard_MoveGridCursor)(At(::bof3::Img(Rest4b_Pick_addr)));
        SH_CALL(CommuBoard_PickHelp)(At(::bof3::Img(Rest4b_Pick_addr)));
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(AreaW0c_TailSub, 1);
    SH_CALL(CommuBoard_DrawSlotLines)(B(::bof3::Img(at::kPick.v)), 0);
    if (placed) return;
    short x = 0, y = 0;
    {
        const unsigned char cursor = static_cast<unsigned char>(Rest4b_ListCursor);
        const unsigned char arg = AreaW0c_TailSub;
        SH_CALL(CommuBoard_SlotRecordXY)(&x, &y, arg, cursor);
    }
    R4EHand(x + 4, y, 1);
    const unsigned char before = static_cast<unsigned char>(static_cast<unsigned char>(Rest4b_ListCursor) - 1);
    const unsigned char arg = AreaW0c_TailSub;
    const unsigned idx = SH_CALL(Commu_NthInSlot)(arg, before);
    SH_CALL(CommuBoard_DrawRecordCard)(0xA0, 0x3E, idx, 1);
}

// original 0x457DD0: CommuBoard_Steps[4], StepsB[2]: the first list (the
// slot's kind, 0x675F7F, over 0x9046CB rows). Cancel: sound 0x106, the slot's
// help, step 0. Confirm: the row's next list 0x652B44[row] (s8 row; read in
// place); 0xFF: R4C's 0x459EE0 (the yes / no row); else 0x675F7E = the slot's
// +1 when the row is the slot's kind - 4, else 0; 0x675F83 = the next list,
// sound 0x103, the next step, help 0x652B50[next list]. Neither: the row
// stepped by 0x1000 / 0x4000 (wrapping at 0x9046CB), sound 0x100 when it
// moved, help 0x652AF8[row]. Then the panel, the slot's lines, the hand, the
// list box (CommuBoard_ListY's y) and the hand at the row.
extern "C" void __cdecl CommuBoard_PickList(void) {
    const unsigned pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(CommuBoard_SlotHelp)();
        Rest4b_BoardStep = 0;
    } else if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        const int row = Rest4b_Pick;
        const unsigned char next = B(::bof3::Img(at::kPickNext.v) + static_cast<U>(row));
        if (next == 0xFF) {
            SH_AT(void (__cdecl*)(), at::kR4CAsk.v)();
        } else {
            const U slot = Slot(AreaW0c_TailSub);
            Rest4b_PickB = row == static_cast<int>(B(slot)) - 4 ? B(slot + 1) : 0;
            Rest4b_PickD = next;
            SH_CALL(Sound_PlayEffect)(0x103);
            Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
            Rest4b_Help = W(::bof3::Img(at::kPickNextHelp.v) + 2 * Rest4b_PickD);
        }
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0x5000);
        const unsigned char old = static_cast<unsigned char>(Rest4b_Pick);
        unsigned char c = old;
        if (keys & 0x1000) {
            --c;
            Rest4b_Pick = c;
            if (static_cast<signed char>(c) < 0) {
                c = static_cast<unsigned char>(FieldE2_LevelA - 1);
                Rest4b_Pick = c;
            }
        } else if (keys & 0x4000) {
            ++c;
            Rest4b_Pick = c;
            if (static_cast<signed char>(c) >= FieldE2_LevelA) {
                c = 0;
                Rest4b_Pick = c;
            }
        }
        if (static_cast<signed char>(c) != static_cast<int>(old)) {
            SH_CALL(Sound_PlayEffect)(0x100);
            c = static_cast<unsigned char>(Rest4b_Pick);
        }
        Rest4b_Help = W(::bof3::Img(at::kPickHelpB.v) + 2 * static_cast<U>(static_cast<signed char>(c)));
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(AreaW0c_TailSub, 1);
    HandAtSlot();
    const int y = static_cast<short>(SH_CALL(CommuBoard_ListY)());   // its callers read the low word only
    SH_CALL(CommuBoard_DrawListBox)(Column(AreaW0c_TailSub) + 0x42, y, 0);
    const int hy = Rest4b_Pick * 16 + y + 4;
    SH_CALL(Menu_DrawHand)(Column(AreaW0c_TailSub) + 0x44, hy, 0);
}

// original 0x457FE0: CommuBoard_Steps[5], StepsB[3]: the second list
// (0x675F7E over the next list's count, 0x652B58[2 * 0x675F83]). Cancel: sound
// 0x106, the step back, 0x675F83 0xFF. Confirm: the list's next 0x652B59[2 *
// 0x675F83]; 0xFF: R4C's 0x459EE0; else 0x675F7D = the slot's +2 when the
// first row is the slot's kind - 4, else 0; 0x675F82 = the next, sound 0x103,
// the next step, help 0x46. Neither: the row stepped (wrapping at the count),
// sound 0x100 when it moved. Then the panel, the slot's lines, the hand, the
// first list (steady); unless 0x675F83 is 0xFF the second list
// (CommuBoard_DrawListBoxB) and the hand at its row.
extern "C" void __cdecl CommuBoard_PickListB(void) {
    const unsigned pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep - 1);
        Rest4b_PickD = 0xFF;
    } else if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        const unsigned char next = B(::bof3::Img(at::kPickPairs.v) + 1 + 2 * Rest4b_PickD);
        if (next == 0xFF) {
            SH_AT(void (__cdecl*)(), at::kR4CAsk.v)();
        } else {
            const U slot = Slot(AreaW0c_TailSub);
            Rest4b_PickC = Rest4b_Pick == static_cast<int>(B(slot)) - 4 ? B(slot + 2) : 0;
            Rest4b_PickE = next;
            SH_CALL(Sound_PlayEffect)(0x103);
            Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
            Rest4b_Help = 0x46;
        }
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0x5000);
        const unsigned char old = static_cast<unsigned char>(Rest4b_PickB);
        unsigned char c = old;
        if (keys & 0x1000) {
            --c;
            Rest4b_PickB = c;
            if (static_cast<signed char>(c) < 0) {
                c = static_cast<unsigned char>(B(::bof3::Img(at::kPickPairs.v) + 2 * Rest4b_PickD) - 1);
                Rest4b_PickB = c;
            }
        } else if (keys & 0x4000) {
            ++c;
            Rest4b_PickB = c;
            if (static_cast<signed char>(c) >= static_cast<int>(B(::bof3::Img(at::kPickPairs.v) + 2 * Rest4b_PickD))) {
                c = 0;
                Rest4b_PickB = c;
            }
        }
        if (static_cast<signed char>(c) != static_cast<int>(old)) SH_CALL(Sound_PlayEffect)(0x100);
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(AreaW0c_TailSub, 1);
    HandAtSlot();
    const int y = static_cast<short>(SH_CALL(CommuBoard_ListY)());   // its callers read the low word only
    SH_CALL(CommuBoard_DrawListBox)(Column(AreaW0c_TailSub) + 0x42, y, 1);
    const unsigned char list = Rest4b_PickD;
    if (list == 0xFF) return;
    const int y2 = y + Rest4b_Pick * 16 + 0xC;
    const int yb = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(AreaW0c_TailSub) + 0x4A, y2, list, 0));
    const int hy = Rest4b_PickB * 16 + yb + 4;
    SH_CALL(Menu_DrawHand)(Column(AreaW0c_TailSub) + 0x4C, hy, 0);
}

// original 0x458240: CommuBoard_Steps[6], StepsB[4]: the toggle 0x675F7D.
// Cancel: sound 0x106, the step back, help 0x45, 0x675F82 0xFF. Confirm: R4C's
// 0x459EE0. Neither: 0x1000 or 0x4000 (auto-repeated) flips 0x675F7D with sound
// 0x100. Then the panel, the slot's lines, the hand, both lists (steady) and,
// unless 0x675F82 is 0xFF, the third list and the hand at the toggle.
extern "C" void __cdecl CommuBoard_PickListC(void) {
    const unsigned pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep - 1);
        Rest4b_Help = 0x45;
        Rest4b_PickE = 0xFF;
    } else if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        SH_AT(void (__cdecl*)(), at::kR4CAsk.v)();
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0x5000);
        if (keys & 0x5000) {
            SH_CALL(Sound_PlayEffect)(0x100);
            Rest4b_PickC ^= 1;
        }
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(AreaW0c_TailSub, 1);
    HandAtSlot();
    const int y = static_cast<short>(SH_CALL(CommuBoard_ListY)());   // its callers read the low word only
    SH_CALL(CommuBoard_DrawListBox)(Column(AreaW0c_TailSub) + 0x42, y, 1);
    const int y2 = y + Rest4b_Pick * 16 + 0xC;
    const unsigned char list = Rest4b_PickD;
    const int yb = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(AreaW0c_TailSub) + 0x4A, y2, list, 1));
    const unsigned char list2 = Rest4b_PickE;
    if (list2 == 0xFF) return;
    const int y3 = yb + Rest4b_PickB * 16 + 0xC;
    const int yc = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(AreaW0c_TailSub) + 0x52, y3, list2, 0));
    const int hy = Rest4b_PickC * 16 + yc + 4;
    SH_CALL(Menu_DrawHand)(Column(AreaW0c_TailSub) + 0x54, hy, 0);
}

// original 0x458410: CommuBoard_Steps[7], StepsB[5]: the yes / no row
// (0x675F7C). Confirm on yes (0): unless the slot already holds the three
// picks (its kind = 0x675F7F + 4, +1 = 0x675F7E, +2 = 0x675F7D), every record
// whose +1 is the slot (used or not) restarts (+4 the clock, +2 and +3 0) and
// so does the slot (+3 0, +4 the clock); then the slot takes the picks (kind
// 0x675F7F + 4, +1 the first list's row, +2 the toggle), the grid cursor comes
// back (0x675F84..86), sound 0x104, the board's state back, help 0xB5.
// Confirm on no, or cancel: sound 0x106, the help and the step R4C's
// 0x459EE0 kept. Neither: 0x2000 or 0x8000 flips the row with sound 0x100.
// Then the panel, the slot's lines, the hand, the lists open and the hand at
// the deepest one. The slot written is 1..8; past them ours aborts (section 5).
extern "C" void __cdecl CommuBoard_Confirm(void) {
    const unsigned pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        if (Rest4b_Toggle != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            Rest4b_Help = static_cast<std::uint32_t>(Rest4b_KeptHelp);
            Rest4b_BoardStep = Rest4b_KeptStep;
        } else {
            unsigned char arg = AreaW0c_TailSub;
            const unsigned char first = static_cast<unsigned char>(Rest4b_PickB);
            const U slot = Slot(arg);
            const bool same = static_cast<int>(B(slot)) == Rest4b_Pick + 4 && static_cast<int>(B(slot + 1)) == Rest4b_PickB &&
                              static_cast<int>(B(slot + 2)) == Rest4b_PickC;
            if (!same) {
                for (U r = ::bof3::Img(Community_Records_addr); r < ::bof3::Img(at::kMembersEnd); r += at::kMemberStride) {
                    if (B(r + 1) != arg) continue;
                    PutL(r + 4, Game_BattleCount);
                    B(r + 2) = 0;
                    B(r + 3) = 0;
                    arg = AreaW0c_TailSub;
                }
                B(SlotToWrite(arg, "CommuBoard_Confirm") + 3) = 0;
                PutL(SlotToWrite(AreaW0c_TailSub, "CommuBoard_Confirm") + 4, Game_BattleCount);
                arg = AreaW0c_TailSub;
            }
            B(SlotToWrite(arg, "CommuBoard_Confirm")) = static_cast<unsigned char>(static_cast<unsigned char>(Rest4b_Pick) + 4);
            B(SlotToWrite(AreaW0c_TailSub, "CommuBoard_Confirm") + 1) = first;
            const unsigned char toggle = static_cast<unsigned char>(Rest4b_PickC);
            Rest4b_CursorB = Rest4b_SavedB;
            B(SlotToWrite(AreaW0c_TailSub, "CommuBoard_Confirm") + 2) = toggle;
            const unsigned char a = Rest4b_SavedA;
            const unsigned char mode = Rest4b_SavedMode;
            Rest4b_CursorA = a;
            Rest4b_CursorMode = mode;
            SH_CALL(Sound_PlayEffect)(0x104);
            Rest4b_BoardState = static_cast<unsigned char>(Rest4b_BoardState - 1);
            Rest4b_Help = 0xB5;
        }
    } else if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        Rest4b_Help = static_cast<std::uint32_t>(Rest4b_KeptHelp);
        Rest4b_BoardStep = Rest4b_KeptStep;
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
        if (keys & 0xA000) {
            SH_CALL(Sound_PlayEffect)(0x100);
            Rest4b_Toggle ^= 1;
        }
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(AreaW0c_TailSub, 1);
    HandAtSlot();
    const int y = static_cast<short>(SH_CALL(CommuBoard_ListY)());   // its callers read the low word only
    if (Rest4b_PickD == 0xFF) {
        SH_CALL(CommuBoard_DrawListBox)(Column(AreaW0c_TailSub) + 0x42, y, 0);
        const int hy = Rest4b_Pick * 16 + y + 4;
        SH_CALL(Menu_DrawHand)(Column(AreaW0c_TailSub) + 0x44, hy, 0);
        return;
    }
    SH_CALL(CommuBoard_DrawListBox)(Column(AreaW0c_TailSub) + 0x42, y, 1);
    const int y2 = y + Rest4b_Pick * 16 + 0xC;
    if (Rest4b_PickE == 0xFF) {
        const unsigned char list = Rest4b_PickD;
        const int yb = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(AreaW0c_TailSub) + 0x4A, y2, list, 0));
        const int hy = Rest4b_PickB * 16 + yb + 4;
        SH_CALL(Menu_DrawHand)(Column(AreaW0c_TailSub) + 0x4C, hy, 0);
        return;
    }
    const unsigned char list = Rest4b_PickD;
    const int yb = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(AreaW0c_TailSub) + 0x4A, y2, list, 1));
    const int y3 = yb + Rest4b_PickB * 16 + 0xC;
    const unsigned char list2 = Rest4b_PickE;
    const int yc = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(AreaW0c_TailSub) + 0x52, y3, list2, 0));
    const int hy = Rest4b_PickC * 16 + yc + 4;
    SH_CALL(Menu_DrawHand)(Column(AreaW0c_TailSub) + 0x54, hy, 0);
}

// original 0x458830: a record's card at (x, y): two boxes (Menu_DrawBox: the
// shadow at +4 0x81, the card 0x80, in the window style), the frame
// (CommuBoard_DrawCardFrame), the record's 5-byte name (0x9048F0 + 5 * record)
// at (x + 6, y + 4), a black tile (x + 6, y + 0x12) 60 by 16, and four bars
// (CommuBoard_DrawBar) at y + 0x12, 0x16, 0x1A, 0x1E, 12 times the record's
// four bytes at 0x653210 + 20 * record long. With `highlight` 0 none flashes;
// else the bar the byte 0x652B60[k] names (0..3) flashes, k the slot 0x675F7F
// picks: 0 for slot 0, the slot - 8 from 9, else the slot's kind (s8 slot, the
// slot's bytes read in place).
extern "C" void __cdecl CommuBoard_DrawRecordCard(int x, int y, unsigned record, unsigned highlight) {
    SH_CALL(Menu_DrawBox)(x + 4, y + 4, 0x48, 0x28, 0x81, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(Menu_DrawBox)(x, y, 0x48, 0x26, 0x80, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(CommuBoard_DrawCardFrame)(x, y);
    const unsigned r = record & 0xFF;
    // DIVERGENCE DIV-0200 (faerie_name.h): the slot NUL-ended at its last
    // whole glyph; the original draws five characters from the table itself.
    unsigned char ended[6];
    const unsigned char* const slot = At(::bof3::Img(at::kNames.v) + 5 * r);
    SH_CALL(Text_DrawAt)(x + 6, y + 4, 0, 5, rest_4b::g_faerie_name_end ? faerie_name::Ended(slot, ended) : slot);
    unsigned char* const p = Packet();
    SH_CALL(Gpu_SetTile)(p);
    PutF(p + 8, static_cast<float>(static_cast<short>(x) + 6));
    PutFBits(p + 0x14, 0x42700000u);   // 60.0
    PutFBits(p + 0x18, 0x41800000u);   // 16.0
    p[4] = 0;
    p[5] = 0;
    p[6] = 0;
    PutF(p + 0xC, static_cast<float>(static_cast<short>(y) + 0x12));
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
    const U bars = ::bof3::Img(at::kBars.v) + 20 * r;
    const int bx = x + 6;
    if ((highlight & 0xFF) == 0) {
        SH_CALL(CommuBoard_DrawBar)(bx, y + 0x12, B(bars) * 12, 0, 0);
        SH_CALL(CommuBoard_DrawBar)(bx, y + 0x16, B(bars + 1) * 12, 1, 0);
        SH_CALL(CommuBoard_DrawBar)(bx, y + 0x1A, B(bars + 2) * 12, 2, 0);
        SH_CALL(CommuBoard_DrawBar)(bx, y + 0x1E, B(bars + 3) * 12, 3, 0);
        return;
    }
    const signed char pick = Rest4b_Pick;
    unsigned char k;
    if (pick == 0)
        k = 0;
    else if (pick >= 9)
        k = static_cast<unsigned char>(pick - 8);
    else
        k = B(::bof3::Img(at::kSlots.v) + 8 * static_cast<U>(static_cast<int>(pick)));
    const U lit = ::bof3::Img(at::kBarHighlight.v) + k;
    SH_CALL(CommuBoard_DrawBar)(bx, y + 0x12, B(bars) * 12, 0, B(lit) == 0 ? 1 : 0);
    SH_CALL(CommuBoard_DrawBar)(bx, y + 0x16, B(bars + 1) * 12, 1, B(lit) == 1 ? 1 : 0);
    SH_CALL(CommuBoard_DrawBar)(bx, y + 0x1A, B(bars + 2) * 12, 2, B(lit) == 2 ? 1 : 0);
    SH_CALL(CommuBoard_DrawBar)(bx, y + 0x1E, B(bars + 3) * 12, 3, B(lit) == 3 ? 1 : 0);
}

// original 0x458AE0: the card's frame of sprites (CommuBoard_DrawSprite): the
// corners 6, 8, 0xB, 0xD at (x, y), (x + 0x40, y), (x, y + 0x22), (x + 0x40,
// y + 0x22), seven of 7 along the top and of 0xC along the bottom from x + 8,
// four of 9 and of 0xA down the sides from y + 8 by 6.
extern "C" void __cdecl CommuBoard_DrawCardFrame(int x, int y) {
    SH_CALL(CommuBoard_DrawSprite)(x, y, 6);
    const int yb = y + 0x22;
    for (int i = 0; i < 7; ++i) {
        SH_CALL(CommuBoard_DrawSprite)(x + 8 * i + 8, y, 7);
        SH_CALL(CommuBoard_DrawSprite)(x + 8 * i + 8, yb, 0xC);
    }
    const int xr = x + 0x40;
    SH_CALL(CommuBoard_DrawSprite)(xr, y, 8);
    for (int i = 0; i < 4; ++i) {
        SH_CALL(CommuBoard_DrawSprite)(x, y + 6 * i + 8, 9);
        SH_CALL(CommuBoard_DrawSprite)(xr, y + 6 * i + 8, 0xA);
    }
    SH_CALL(CommuBoard_DrawSprite)(x, yb, 0xB);
    SH_CALL(CommuBoard_DrawSprite)(xr, yb, 0xD);
}

// original 0x458B90: a bar of two shaded quads (Gpu_SetPolyG4), x .. x + w
// (s16 each), y .. y + 2 dim above and full below, then y + 2 .. y + 4 full
// above and dim below; the colour is row `row` of a stack table of four
// {1,0,0} {0,1,0} {0,0,1} {0,1,1} times the intensity (an 8-bit signed
// multiply's low byte): 0x80, or Frame_Counter's flash when `flash` is not 0;
// dim is the full colour >> 2. A row past the four reads the frame above the
// table; ours aborts (section 5).
extern "C" void __cdecl CommuBoard_DrawBar(int x, int y, int w, unsigned row, unsigned flash) {
    static const unsigned char kRgb[4][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 1, 1}};
    const unsigned char intensity = (flash & 0xFF) != 0 ? Flash() : 0x80;
    const unsigned r = row & 0xFF;
    if (r >= 4)
        bof3::Fatal("CommuBoard_DrawBar: colour row %u is past the four of its stack table; the original reads the frame "
                    "above it",
                    r);
    unsigned char* p = Packet();
    SH_CALL(Gpu_SetPolyG4)(p);
    const int x0 = static_cast<short>(x);
    const int x1 = x0 + static_cast<short>(w);
    const int y0 = static_cast<short>(y);
    const float fx0 = static_cast<float>(x0), fx1 = static_cast<float>(x1);
    const float fy0 = static_cast<float>(y0), fy2 = static_cast<float>(y0 + 2), fy4 = static_cast<float>(y0 + 4);
    const auto times = [intensity](unsigned char c) { return static_cast<unsigned char>(static_cast<signed char>(c) * static_cast<signed char>(intensity)); };
    const unsigned char cr = times(kRgb[r][0]), cg = times(kRgb[r][1]), cb = times(kRgb[r][2]);
    const unsigned char dr = static_cast<unsigned char>(cr >> 2), dg = static_cast<unsigned char>(cg >> 2),
                        db = static_cast<unsigned char>(cb >> 2);
    PutF(p + 8, fx0);
    PutF(p + 0x18, fx1);
    PutF(p + 0x28, fx0);
    PutF(p + 0x38, fx1);
    PutF(p + 0xC, fy0);
    PutF(p + 0x1C, fy0);
    PutF(p + 0x2C, fy2);
    PutF(p + 0x3C, fy2);
    p[4] = dr;
    p[5] = dg;
    p[6] = db;
    p[0x14] = dr;
    p[0x15] = dg;
    p[0x16] = db;
    p[0x24] = cr;
    p[0x25] = cg;
    p[0x26] = cb;
    p[0x34] = cr;
    p[0x35] = cg;
    p[0x36] = cb;
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
    p = Packet();
    SH_CALL(Gpu_SetPolyG4)(p);
    PutF(p + 8, fx0);
    PutF(p + 0x18, fx1);
    PutF(p + 0x28, fx0);
    PutF(p + 0x38, fx1);
    PutF(p + 0xC, fy2);
    PutF(p + 0x1C, fy2);
    PutF(p + 0x2C, fy4);
    PutF(p + 0x3C, fy4);
    p[4] = cr;
    p[5] = cg;
    p[6] = cb;
    p[0x14] = cr;
    p[0x15] = cg;
    p[0x16] = cb;
    p[0x24] = dr;
    p[0x25] = dg;
    p[0x26] = db;
    p[0x34] = dr;
    p[0x35] = dg;
    p[0x36] = db;
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
}

// original 0x458D70: the board's panel. The title box (0x14, 0x12) 0x118 by
// 0x13 and its notched outline; the help line's message (0x675F7A, unless
// 0xFFFF: MessagePools by its offset word) at (0x1D, 0x15) and, for help 0x3D,
// a flashing translucent tile over the yes / no row (0x675F7C: x 0xD0 + 36 *
// it, y 0x16, 12 * it + 0x19 by 12). The panel box (0x15, 0x2E) 0x114 by 0xAA,
// its frame (CommuBoard_DrawPanelFrame), the slot-0 row: an outline and a
// sprite 1 per record in slot 0. The three slots 9..11 down the right: an
// outline, the slot's sprite (r + 3) and a sprite 1 per record, five to a row.
// The eight slots (2 rows of 4): an outline each; past the area's limit a box,
// else the slot's sprite 0, its kind's digit sprite (0x652B6C[kind], kind 5
// adds the slot's +1) unless kind 0, and a sprite 1 at each record's place
// (CommuBoard_SlotRecordXY). Then the counters: sprite 2, the s8 0x9046CA and
// 0x9046CC printed (Crt_sprintf, Text_DrawFont12) beside the label 0x669E10.
extern "C" void __cdecl CommuBoard_DrawPanel(void) {
    SH_CALL(Menu_DrawBox)(0x14, 0x12, 0x118, 0x13, 0xF2, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(Menu_DrawOutlineNotched)(0x16, 0x14, 0x113, 0xF, 0);
    const unsigned help = Rest4b_Help;
    if (help != 0xFFFF) {
        const unsigned char* const text = At(::bof3::Img(at::kMessagePools.v) + W(::bof3::Img(at::kMessagePools.v) + 2 * help));
        const bool fitted = g_board_yes_no_layout && help == 0x3D;   // DIV-0027 (amended 2026-10-26)
        YesNoTail tail = {text, {0, 0}, {0, 0}};
        if (fitted) tail = YesNoLayout_Tail(text, 0x1D, "the faerie board's accept prompt");
        SH_CALL(Text_DrawAt)(0x1D, 0x15, 0, 0xFF, tail.line);
        if (Rest4b_Help == 0x3D) {
            SH_CALL(Gpu_SetDrawMode)(Packet(), 0, 0, 0, 0);
            SH_CALL(Gfx_CommitPrim)(1, 0xC);
            const unsigned char intensity = Flash();
            unsigned char* const q = Packet();
            SH_CALL(Gpu_SetTile)(q);
            SH_CALL(Gpu_SetSemiTrans)(q, 1);
            PutFBits(q + 0xC, 0x41B00000u);   // 22.0
            const unsigned t = Rest4b_Toggle;
            if (fitted && t > 1) bof3::Fatal("CommuBoard_DrawPanel: the yes / no row holds %u, not an answer", t);
            PutF(q + 8, static_cast<float>(fitted ? tail.stop[t] + 1 : static_cast<int>(t * 36 + 0xD0)));   // the stop is the word less two: the tile one unit before the word
            PutFBits(q + 0x18, 0x41400000u);   // 12.0
            q[4] = intensity;
            q[5] = intensity;
            q[6] = 0;
            const unsigned t2 = Rest4b_Toggle;
            PutF(q + 0x14, static_cast<float>(fitted ? tail.width[t2] + 1 : static_cast<int>(t2 * 12 + 0x19)));
            SH_CALL(Gfx_CommitPrim)(1, 0x1C);
        }
    }
    SH_CALL(Menu_DrawBox)(0x15, 0x2E, 0x114, 0xAA, 0x80, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(CommuBoard_DrawPanelFrame)(0x14, 0x2D);
    SH_CALL(Menu_DrawOutlineNotched)(0x20, 0x38, 0xA8, 0x10, 1);
    const unsigned char top = SH_CALL(Commu_CountInSlot)(0);
    for (unsigned char i = 0; i < top; ++i) SH_CALL(CommuBoard_DrawSprite)(i * 8 + 0x24, 0x3A, 1);
    for (unsigned char r = 0; r < 3; ++r) {
        SH_CALL(Menu_DrawOutlineNotched)(0xF2, (r * 3 + 3) * 18, 0x30, 0x30, 1);
        const int ry = r * 54;
        SH_CALL(CommuBoard_DrawSprite)(0xEF, ry + 0x33, static_cast<unsigned char>(r + 3));
        const unsigned char n = SH_CALL(Commu_CountInSlot)(static_cast<unsigned char>(r + 9));
        for (unsigned j = 0; j < n; ++j)
            SH_CALL(CommuBoard_DrawSprite)(static_cast<int>(j % 5) * 8 + 0xF6, static_cast<int>(j / 5) * 7 + ry + 0x44, 1);
    }
    U slot = ::bof3::Img(at::kSlots.v) + 8;
    for (unsigned char i = 0; i < 8; ++i, slot += 8) {
        const int sy = (i >> 2) * 61;
        const int sx = static_cast<int>(i % 4) * 52;
        SH_CALL(Menu_DrawOutlineNotched)(sx + 0x20, sy + 0x6B, 0x28, 0x28, 1);
        if (i >= B(::bof3::Img(at::kAreaLimits.v) + static_cast<std::uint32_t>(Game_AreaNumber))) {
            SH_CALL(Menu_DrawBox)(sx + 0x20, sy + 0x6B, 0x28, 0x28, 1, static_cast<unsigned char>(Config_WindowColour));
            continue;
        }
        SH_CALL(CommuBoard_DrawSprite)(sx + 0x1A, sy + 0x5B, 0);
        const unsigned char kind = B(slot);
        if (kind != 0) {
            const unsigned char digit = kind == 5 ? static_cast<unsigned char>(B(::bof3::Img(at::kSlotIcons.v) + 5) + B(slot + 1))
                                                  : B(::bof3::Img(at::kSlotIcons.v) + kind);
            SH_CALL(CommuBoard_DrawDigits)(sx + 0x22, sy + 0x6C, digit);
        }
        const unsigned char id = static_cast<unsigned char>(i + 1);
        const unsigned char n = SH_CALL(Commu_CountInSlot)(id);
        for (unsigned char k = 1; n >= 1 && k <= n; ++k) {
            short px = 0, py = 0;
            SH_CALL(CommuBoard_SlotRecordXY)(&px, &py, id, k);
            SH_CALL(CommuBoard_DrawSprite)(px, py, 1);
        }
    }
    SH_CALL(CommuBoard_DrawSprite)(0x30, 0x4C, 2);
    char* const out = reinterpret_cast<char*>(At(::bof3::Img(at::kTextBuffer.v)));
    SH_CALL(Crt_sprintf)(out, reinterpret_cast<const char*>(At(::bof3::Img(Boss26Fx_CountFormat_addr))), Rest4b_CountA);
    SH_CALL(Text_DrawFont12)(0x50, 0x4C, 0, At(::bof3::Img(at::kTextBuffer.v)));
    // DIV-0064 group 14: the label from the overlay's buffer, its count 2 the
    // Chinese word's (the overlay's is longer); the number after it then
    // moves from 0x98 - under the seven letters of `Culture` - to 0xBE, where
    // the US screen has it (the owner's web reference, 2026-10-10).
    const bool words = Labels_Written(14);
    SH_CALL(Text_DrawAt)(0x78, 0x4C, 0, words ? 0xFF : 2, words ? Labels_Slot(14, 0) : At(::bof3::Img(at::kPanelLabel.v)));
    SH_CALL(Crt_sprintf)(out, reinterpret_cast<const char*>(At(::bof3::Img(at::kNumberFormat.v))), static_cast<std::uint32_t>(Rest4a_LevelLit));
    SH_CALL(Text_DrawFont12)(words ? 0xBE : 0x98, 0x4C, 0, At(::bof3::Img(at::kTextBuffer.v)));
}

// original 0x4591A0: the panel's frame of sprites: the corners 6, 8, 0xB, 0xD
// at (x, y), (x + 0x110, y), (x, y + 0xA8), (x + 0x110, y + 0xA8), 33 of 7 and
// of 0xC along the top and bottom from x + 8, 20 of 9 and of 0xA down the
// sides from y + 8, by 8.
extern "C" void __cdecl CommuBoard_DrawPanelFrame(int x, int y) {
    SH_CALL(CommuBoard_DrawSprite)(x, y, 6);
    const int yb = y + 0xA8;
    for (int i = 0; i < 0x21; ++i) {
        SH_CALL(CommuBoard_DrawSprite)(x + 8 * i + 8, y, 7);
        SH_CALL(CommuBoard_DrawSprite)(x + 8 * i + 8, yb, 0xC);
    }
    const int xr = x + 0x110;
    SH_CALL(CommuBoard_DrawSprite)(xr, y, 8);
    for (int i = 0; i < 0x14; ++i) {
        SH_CALL(CommuBoard_DrawSprite)(x, y + 8 * i + 8, 9);
        SH_CALL(CommuBoard_DrawSprite)(xr, y + 8 * i + 8, 0xA);
    }
    SH_CALL(CommuBoard_DrawSprite)(x, yb, 0xB);
    SH_CALL(CommuBoard_DrawSprite)(xr, yb, 0xD);
}

// original 0x459250: the slot's two lines (Gpu_SetLineF3, three points each)
// from its 12-byte row 0x652B7C + 12 * slot {six s16: a0 a1 a2 a3 a4 a5},
// coloured (I, I, 0): I 0xFF when `steady`, else Frame_Counter's flash. A slot
// 1..8 past the area's limit starts the first at a2 and the second at a1;
// otherwise a0 and a3. First (start, a1) (a4, a1) (a4, a5); second (a2, a3 or
// a1) (a2, a5) (a4, a5).
extern "C" void __cdecl CommuBoard_DrawSlotLines(unsigned slot, unsigned steady) {
    const unsigned char intensity = (steady & 0xFF) == 0 ? Flash() : 0xFF;
    const unsigned char s = static_cast<unsigned char>(slot);
    unsigned char* p = Packet();
    SH_CALL(Gpu_SetLineF3)(p);
    const U row = ::bof3::Img(at::kSlotLines.v) + 12 * s;
    const auto locked = [s] { return s != 0 && s < 9 && s > B(::bof3::Img(at::kAreaLimits.v) + static_cast<std::uint32_t>(Game_AreaNumber)); };
    PutF(p + 8, static_cast<float>(locked() ? S16(row + 4) : S16(row)));
    PutF(p + 0xC, static_cast<float>(S16(row + 2)));
    PutF(p + 0x14, static_cast<float>(S16(row + 8)));
    PutF(p + 0x18, static_cast<float>(S16(row + 2)));
    PutF(p + 0x20, static_cast<float>(S16(row + 8)));
    p[4] = intensity;
    p[5] = intensity;
    p[6] = 0;
    PutF(p + 0x24, static_cast<float>(S16(row + 0xA)));
    SH_CALL(Gfx_CommitPrim)(1, 0x2C);
    p = Packet();
    SH_CALL(Gpu_SetLineF3)(p);
    PutF(p + 0xC, static_cast<float>(locked() ? S16(row + 2) : S16(row + 6)));
    PutF(p + 8, static_cast<float>(S16(row + 4)));
    PutF(p + 0x14, static_cast<float>(S16(row + 4)));
    PutF(p + 0x18, static_cast<float>(S16(row + 0xA)));
    PutF(p + 0x20, static_cast<float>(S16(row + 8)));
    p[4] = intensity;
    p[5] = intensity;
    p[6] = 0;
    PutF(p + 0x24, static_cast<float>(S16(row + 0xA)));
    SH_CALL(Gfx_CommitPrim)(1, 0x2C);
}

// original 0x459430: how many of the 60 community records are in use with
// their +1 = `slot` (al).
extern "C" unsigned char __cdecl Commu_CountInSlot(unsigned slot) {
    const unsigned char s = static_cast<unsigned char>(slot);
    unsigned char n = 0;
    for (U r = ::bof3::Img(Community_Records_addr); r < ::bof3::Img(at::kMembersEnd); r += at::kMemberStride)
        if (B(r) != 0 && B(r + 1) == s) ++n;
    return n;
}

// original 0x459460: the index of the `nth` (0-based) record in use in `slot`,
// or 0xFF when there are fewer.
extern "C" unsigned __cdecl Commu_NthInSlot(unsigned slot, unsigned nth) {
    const unsigned char s = static_cast<unsigned char>(slot), want = static_cast<unsigned char>(nth);
    unsigned char seen = 0;
    for (unsigned k = 0; k < 60; ++k) {
        const U r = ::bof3::Img(Community_Records_addr) + 8 * k;
        if (B(r) == 0 || B(r + 1) != s) continue;
        if (seen == want) return k;
        ++seen;
    }
    return 0xFF;
}

// original 0x4594A0: the grid cursor at `cell` (0x9039F5 or 0x675F7F) moved by
// the auto-repeated keys of Input_Pressed & 0xF000, by the cursor's mode
// 0x675F74 (0: the slot-0 row, 1: the eight slots, 0x675F75 / 0x675F76 the
// row and column, 2: the slots 9..11 down the right):
//   0x1000  0: mode 1 at row 1, cell = col + 4. 1: row 0 - mode 0, cell 0;
//           else row 0, cell - 4. 2: cell - 1, 0xB below 9.
//   0x4000  0: mode 1 at row 0, cell = col. 1: row 0 - row 1, cell + 4; else
//           mode 0, cell 0. 2: cell + 1, 9 above 0xB.
//   0x8000  0: mode 2, cell 9. 1: col - 1, cell - 1; at col 0 col 1 and mode
//           2, cell = row + 0xA. 2: from 9 mode 0, cell 0; else mode 1, row =
//           cell - 0xA, cell = (cell + 0x37) * 4, col 4.
//   0x2000  0: mode 2, cell 9. 1: col + 1, cell + 1; past 4 col 4 and mode 2,
//           cell = row + 0xA. 2: from 9 mode 0, cell 0; else mode 1, col 1,
//           row = cell - 0xA, cell = cell * 4 - 0x27.
// Any of the four keys: sound 0x100. All byte arithmetic.
extern "C" void __cdecl CommuBoard_MoveGridCursor(unsigned char* cell) {
    const unsigned keys = SH_CALL(Input_AutoRepeat)(static_cast<std::uint32_t>(Input_Pressed) & 0xF000);
    unsigned char& mode = Rest4b_CursorMode;
    unsigned char& row = Rest4b_CursorA;
    unsigned char& col = Rest4b_CursorB;
    const auto toRight = [&] {
        mode = 2;
        *cell = 9;
    };
    const auto fromColumn = [&] {
        mode = 2;
        *cell = static_cast<unsigned char>(row + 0xA);
    };
    if (keys & 0x1000) {
        switch (mode) {
        case 0:
            row = 1;
            mode = 1;
            *cell = static_cast<unsigned char>(col + 4);
            break;
        case 1:
            if (row == 0) {
                mode = 0;
                *cell = 0;
            } else {
                row = 0;
                *cell = static_cast<unsigned char>(*cell + 0xFC);
            }
            break;
        case 2: {
            const unsigned char v = static_cast<unsigned char>(*cell - 1);
            *cell = v;
            if (v < 9) *cell = 0xB;
            break;
        }
        default: break;
        }
    } else if (keys & 0x4000) {
        switch (mode) {
        case 0: {
            const unsigned char c = col;
            row = 0;
            mode = 1;
            *cell = c;
            break;
        }
        case 1:
            if (row != 0) {
                mode = 0;
                *cell = 0;
            } else {
                row = 1;
                *cell = static_cast<unsigned char>(*cell + 4);
            }
            break;
        case 2: {
            const unsigned char v = static_cast<unsigned char>(*cell + 1);
            *cell = v;
            if (v > 0xB) *cell = 9;
            break;
        }
        default: break;
        }
    } else if (keys & 0x8000) {
        switch (mode) {
        case 0: toRight(); break;
        case 1:
            col = static_cast<unsigned char>(col - 1);
            *cell = static_cast<unsigned char>(*cell - 1);
            if (col == 0) {
                col = 1;
                fromColumn();
            }
            break;
        case 2:
            if (*cell == 9) {
                mode = 0;
                *cell = 0;
            } else {
                mode = 1;
                row = static_cast<unsigned char>(*cell - 0xA);
                *cell = static_cast<unsigned char>((*cell + 0x37) << 2);
                col = 4;
            }
            break;
        default: break;
        }
    } else if (keys & 0x2000) {
        switch (mode) {
        case 0: toRight(); break;
        case 1:
            col = static_cast<unsigned char>(col + 1);
            *cell = static_cast<unsigned char>(*cell + 1);
            if (col > 4) {
                col = 4;
                fromColumn();
            }
            break;
        case 2:
            if (*cell == 9) {
                mode = 0;
                *cell = 0;
            } else {
                mode = 1;
                col = 1;
                row = static_cast<unsigned char>(*cell - 0xA);
                *cell = static_cast<unsigned char>((*cell << 2) - 0x27);
            }
            break;
        default: break;
        }
    }
    if (keys & 0xF000) SH_CALL(Sound_PlayEffect)(0x100);
}

// original 0x459720: a number's 16x16 sprite (Gpu_SetSprt16, after a draw-mode
// packet with texture page 0x1E) at (x, y): u = (n % 10 + 13 * (n / 10) + 5)
// << 4, v = (n / 10 + 0xE) << 4 (bytes), the CLUT word 0x7840 | 0x652C0C[n] >>
// 4, colour 0x80 grey.
extern "C" void __cdecl CommuBoard_DrawDigits(int x, int y, unsigned number) {
    SH_CALL(Gpu_SetDrawMode)(Packet(), 0, 0, 0x1E, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const q = Packet();
    SH_CALL(Gpu_SetSprt16)(q);
    PutF(q + 8, static_cast<float>(static_cast<short>(x)));
    PutF(q + 0xC, static_cast<float>(static_cast<short>(y)));
    const unsigned n = number & 0xFF;
    const unsigned tens = n / 10, ones = n % 10;
    q[0x15] = static_cast<unsigned char>((tens + 0xE) << 4);
    q[0x14] = static_cast<unsigned char>((ones + static_cast<unsigned char>(tens * 0xD) + 5) << 4);
    q[4] = 0x80;
    q[5] = 0x80;
    q[6] = 0x80;
    PutW(q + 0x16, 0x7840u | (B(::bof3::Img(at::kDigitCells.v) + n) >> 4));
    SH_CALL(Gfx_CommitPrim)(1, 0x18);
}

// original 0x4597E0: the board's sprite `id` at (x, y) (Gpu_SetSprt, after a
// draw-mode packet with texture page 0x1E): its 6-byte row 0x652C18 + 6 * id
// {u, v, w, h, clut x, clut y}: the CLUT word ((clut y + 0x1E0) << 6) |
// clut x >> 4; colour 0x80 grey.
extern "C" void __cdecl CommuBoard_DrawSprite(int x, int y, unsigned id) {
    SH_CALL(Gpu_SetDrawMode)(Packet(), 0, 0, 0x1E, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const q = Packet();
    SH_CALL(Gpu_SetSprt)(q);
    PutF(q + 8, static_cast<float>(static_cast<short>(x)));
    PutF(q + 0xC, static_cast<float>(static_cast<short>(y)));
    const U e = ::bof3::Img(at::kSprites.v) + 6 * (id & 0xFF);
    q[0x14] = B(e);
    q[0x15] = B(e + 1);
    PutW(q + 0x18, B(e + 2));
    PutW(q + 0x1A, B(e + 3));
    PutW(q + 0x16, (((B(e + 5) + 0x1E0u) << 6) | (B(e + 4) >> 4)) & 0xFFFF);
    q[4] = 0x80;
    q[5] = 0x80;
    q[6] = 0x80;
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x4598A0: the record list's box at (x, y), 0x9046CB rows of 16: its
// shadow (x + 4, y + 5) 0x81 unless `steady`, the box 0x80 (both in the window
// style), its frame (CommuBoard_DrawListFrame at y - 1) and the rows' lines
// (the pointers at 0x669E68) at (x + 6, y + 2 + 16 * i), colour 7 * steady.
extern "C" void __cdecl CommuBoard_DrawListBox(int x, int y, unsigned steady) {
    const unsigned char flag = static_cast<unsigned char>(steady);
    const int h = FieldE2_LevelA * 16;
    if (flag == 0) SH_CALL(Menu_DrawBox)(x + 4, y + 5, ListBoxWidth(), h, 0x81, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(Menu_DrawBox)(x, y, ListBoxWidth(), h + 1, 0x80, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(CommuBoard_DrawListFrame)(x, y - 1, static_cast<unsigned>(h));
    if (FieldE2_LevelA == 0) return;
    const unsigned char colour = static_cast<unsigned char>(flag * 7);
    for (unsigned i = 0; i < FieldE2_LevelA; ++i) {
        const unsigned char* const line = At(L(::bof3::Img(at::kListTextA.v) + 4 * i));
        SH_CALL(Text_DrawAt)(x + 6, static_cast<int>(i * 16) + y + 2, colour, Labels_Written(13) ? 0xFF : 3, line);   // DIV-0064 group 13
    }
}

// original 0x459960: a list's frame of sprites: the corners 6, 8, 0xB, 0xD at
// (x, y), (x + 0x28, y), (x, y + h), (x + 0x28, y + h) with h the byte `h`,
// four of 7 and of 0xC along the top and bottom from x + 8, and (h >> 3) - 1
// (a byte: 255 for h below 8) of 9 and of 0xA down the sides from y + 8, by 8.
extern "C" void __cdecl CommuBoard_DrawListFrame(int x, int y, unsigned h) {
    SH_CALL(CommuBoard_DrawSprite)(x, y, 6);
    const int yb = y + static_cast<int>(h & 0xFF);
    const int columns = (ListBoxWidth() - 8) / 8 - 1;   // 4 for the shipped 0x30 (DIV-0064 group 13)
    for (int i = 0; i < columns; ++i) {
        SH_CALL(CommuBoard_DrawSprite)(x + 8 * i + 8, y, 7);
        SH_CALL(CommuBoard_DrawSprite)(x + 8 * i + 8, yb, 0xC);
    }
    const int xr = x + ListBoxWidth() - 8;
    SH_CALL(CommuBoard_DrawSprite)(xr, y, 8);
    const unsigned rows = static_cast<unsigned char>(((h & 0xFF) >> 3) - 1);
    for (unsigned i = 0; i < rows; ++i) {
        SH_CALL(CommuBoard_DrawSprite)(x, y + 8 * static_cast<int>(i) + 8, 9);
        SH_CALL(CommuBoard_DrawSprite)(xr, y + 8 * static_cast<int>(i) + 8, 0xA);
    }
    SH_CALL(CommuBoard_DrawSprite)(x, yb, 0xB);
    SH_CALL(CommuBoard_DrawSprite)(xr, yb, 0xD);
}

// original 0x459A30: the first list's y: with 0x675F75 0, 0x6A less 9 per row
// past four of 0x9046CB; else 0xAC - 12 * 0x9046CB.
extern "C" int __cdecl CommuBoard_ListY(void) {
    if (Rest4b_CursorA == 0) {
        const unsigned n = FieldE2_LevelA;
        return n > 4 ? 0x6A - static_cast<int>((n - 4) * 9) : 0x6A;
    }
    return 0xAC - FieldE2_LevelA * 12;
}

// original 0x459A80: list `list`'s box at (x, y): its 2-byte row 0x652C6C + 2 *
// list {first, count}, h = 16 * count; when y + h + 4 passes 0xD8 (s16 y) the
// box moves up to 0xD4 - h. Its shadow unless `steady`, the box, its frame,
// and its lines (the pointers 0x669EE0[first + i]) at (x + 6, y + 2 + 16 * i),
// colour 7 * steady. Answers the y it drew at (ax).
extern "C" unsigned short __cdecl CommuBoard_DrawListBoxB(int x, int y, unsigned list, unsigned steady) {
    const U row = ::bof3::Img(at::kLists.v) + 2 * (list & 0xFF);
    const int h = B(row + 1) * 16;
    if (h + static_cast<short>(y) + 4 > 0xD8) y = 0xD4 - h;
    const unsigned char flag = static_cast<unsigned char>(steady);
    if (flag == 0) SH_CALL(Menu_DrawBox)(x + 4, y + 6, ListBoxWidth(), h, 0x81, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(Menu_DrawBox)(x, y, ListBoxWidth(), h + 1, 0x80, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(CommuBoard_DrawListFrame)(x, y - 1, static_cast<unsigned>(h));
    if (B(row + 1) != 0) {
        const unsigned char colour = static_cast<unsigned char>(flag * 7);
        for (unsigned i = 0; i < B(row + 1); ++i) {
            const unsigned char* const line = At(L(::bof3::Img(at::kListTexts.v) + 4 * (B(row) + i)));
            SH_CALL(Text_DrawAt)(x + 6, static_cast<int>(i * 16) + y + 2, colour, Labels_Written(13) ? 0xFF : 3, line);   // DIV-0064 group 13
        }
    }
    return static_cast<unsigned short>(y);
}

// original 0x459B70: the screen place of record position k of `slot` into two
// words: the slot's corner (52 * ((slot - 1) % 4) + 0x20, 61 * ((slot - 1) >>
// 2) + 0x6B, signed as the original's); k 0: the corner + (0xA, 0); else the
// 4-byte offset 0x652C74 + 4 * (t + k - 1) {s16 x, s16 y} added, t 2 for a
// slot of two records, 1 of three, else 0.
extern "C" void __cdecl CommuBoard_SlotRecordXY(short* x, short* y, unsigned slot, unsigned k) {
    const int s = static_cast<int>(slot & 0xFF) - 1;
    const int cx = (s % 4) * 52 + 0x20;
    const int cy = (s >> 2) * 61 + 0x6B;
    const unsigned char n = SH_CALL(Commu_CountInSlot)(slot);
    const unsigned t = n == 2 ? 2 : n == 3 ? 1 : 0;
    const unsigned char kk = static_cast<unsigned char>(k);
    if (kk == 0) {
        *x = static_cast<short>(cx + 0xA);
        *y = static_cast<short>(cy);
        return;
    }
    const U e = ::bof3::Img(at::kSlotOffsets.v) + 4 * (t + static_cast<unsigned char>(kk - 1));
    *x = static_cast<short>(W(e) + static_cast<unsigned>(cx));
    *y = static_cast<short>(W(e + 2) + static_cast<unsigned>(cy));
}

// original 0x459C40: the step back: the grid cursor put back from 0x675F84..86,
// sound 0x106, 0x939A40 - 1.
extern "C" void __cdecl CommuBoard_CancelStep(void) {
    const unsigned char b = Rest4b_SavedB, a = Rest4b_SavedA, mode = Rest4b_SavedMode;
    Rest4b_CursorB = b;
    Rest4b_CursorA = a;
    Rest4b_CursorMode = mode;
    SH_CALL(Sound_PlayEffect)(0x106);
    Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep - 1);
}

// original 0x459C80: the record at the list cursor (less `back`) of the slot
// 0x9039F5 moved to the slot 0x675F7F, by the cursor's mode: 1 - refused when
// the target holds three or is past the area's limit (s8); 0, 2 - allowed;
// any other refused; and refused onto its own slot. Refused: sound 0x107, al 0.
// Moved: the grid cursor put back, the record (Commu_NthInSlot) gets +1 the
// target, +4 the clock, +2 and +3 0; the board's state back, sound 0x103, al 1.
// A cursor past the slot's records (0xFF) writes record 255 in the original;
// ours aborts (section 5).
extern "C" unsigned char __cdecl CommuBoard_PlaceRecord(unsigned back) {
    const unsigned char mode = Rest4b_CursorMode;
    bool refused;
    if (mode == 0 || mode == 2) {
        refused = false;
    } else if (mode == 1) {
        const unsigned char n = SH_CALL(Commu_CountInSlot)(static_cast<unsigned char>(Rest4b_Pick));
        refused = n == 3 || Rest4b_Pick > static_cast<int>(B(::bof3::Img(at::kAreaLimits.v) + static_cast<std::uint32_t>(Game_AreaNumber)));
    } else {
        refused = true;
    }
    const unsigned char arg = AreaW0c_TailSub;
    if (!refused && Rest4b_Pick == static_cast<int>(arg)) refused = true;
    if (refused) {
        SH_CALL(Sound_PlayEffect)(0x107);
        return 0;
    }
    const unsigned char b = Rest4b_SavedB, a = Rest4b_SavedA, m = Rest4b_SavedMode;
    Rest4b_CursorB = b;
    Rest4b_CursorA = a;
    Rest4b_CursorMode = m;
    const unsigned char nth = static_cast<unsigned char>(static_cast<unsigned char>(Rest4b_ListCursor) - static_cast<unsigned char>(back));
    const unsigned idx = SH_CALL(Commu_NthInSlot)(arg, nth);
    const U r = Member(idx & 0xFF, "CommuBoard_PlaceRecord");
    B(r + 1) = static_cast<unsigned char>(Rest4b_Pick);
    PutL(r + 4, Game_BattleCount);
    const unsigned char state = Rest4b_BoardState;
    B(r + 2) = 0;
    B(r + 3) = 0;
    Rest4b_BoardState = static_cast<unsigned char>(state - 1);
    SH_CALL(Sound_PlayEffect)(0x103);
    return 1;
}

// original 0x459D80: the slot 0x9039F5's help line: 0x6A for slot 0; 2 *
// slot + 0x2C from 9; else 0x6A for kind 0, the slot's +1 + 0x4A for kind 4,
// and for kind 5..13 a stack table of nine {0x47 0x54 0x95 0x9A 0x4E 0x93
// 0x58 0x6C 0x5D}. Any other kind reads past that table (the frame above
// it); ours aborts (section 5).
extern "C" void __cdecl CommuBoard_SlotHelp(void) {
    static const unsigned short kHelps[9] = {0x47, 0x54, 0x95, 0x9A, 0x4E, 0x93, 0x58, 0x6C, 0x5D};
    const unsigned char arg = AreaW0c_TailSub;
    if (arg == 0) {
        Rest4b_Help = 0x6A;
        return;
    }
    if (arg >= 9) {
        Rest4b_Help = (arg * 2u + 0x2C) & 0xFFFF;
        return;
    }
    const U slot = Slot(arg);
    const unsigned char kind = B(slot);
    if (kind == 0) {
        Rest4b_Help = 0x6A;
        return;
    }
    const unsigned char k = static_cast<unsigned char>(kind - 4);
    if (k == 0) {
        Rest4b_Help = B(slot + 1) + 0x4Au;
        return;
    }
    if (k > 9)
        bof3::Fatal("CommuBoard_SlotHelp: slot %u's kind %u is past the nine of its stack table; the original reads the "
                    "frame above it",
                    static_cast<unsigned>(arg), static_cast<unsigned>(kind));
    Rest4b_Help = kHelps[k - 1];
}

// original 0x459E50: the help line for the slot at `cell`: 0x44 on 0x9039F5's
// own slot; 0x69 for slot 0; from 9 the u16 0x652C72[slot]; past the area's
// limit 0x44; for an empty slot (kind 0) 0x69; else the u16 0x652AF0[kind]
// (each read in place).
extern "C" void __cdecl CommuBoard_PickHelp(unsigned char* cell) {
    const unsigned char v = *cell;
    if (v == AreaW0c_TailSub) {
        Rest4b_Help = 0x44;
        return;
    }
    if (v == 0) {
        Rest4b_Help = 0x69;
        return;
    }
    if (v >= 9) {
        Rest4b_Help = W(::bof3::Img(at::kSlotHelp.v) + 2 * v);
        return;
    }
    if (v > B(::bof3::Img(at::kAreaLimits.v) + static_cast<std::uint32_t>(Game_AreaNumber))) {
        Rest4b_Help = 0x44;
        return;
    }
    const unsigned char kind = B(Slot(v));
    Rest4b_Help = kind == 0 ? 0x69 : W(::bof3::Img(at::kPickHelp.v) + 2 * kind);
}

#pragma clang attribute pop

// original 0x459EE0 (0x3D bytes): called by R4B's 0x457DD0, 0x457FE0 and
// 0x458240. 0x675F81 = 0x939A40, the word 0x675F78 = the word 0x675F7A (both
// read first); Sound_PlayEffect(0x103); the word 0x675F7A = 0x3D, 0x675F7C =
// 0, 0x939A40 = 5.
extern "C" void __cdecl Commu_PushSubscreen(void) {
    const unsigned char state = Rest4b_BoardStep;
    const U sub = Rest4b_Help;
    Rest4b_KeptStep = state;
    Rest4b_KeptHelp = sub;
    Sound(0x103);
    Rest4b_Help = 0x3D;
    Rest4b_Toggle = 0;
    Rest4b_BoardStep = 5;
}

// original 0x459F20 (0xE bytes): CommuGame[0] (R4B's 0x652A84) - jmp
// [CommuHiLo_Phases + 0x939A3E * 4], unchecked (ours aborts past its 3).
extern "C" void __cdecl CommuHiLo_Run(void) {
    Dispatch("CommuHiLo_Run (0x459F20)", CommuHiLo_Phases, CommuHiLo_Phases_count, ::bof3::Img(Rest4b_BoardState_addr));
}

// original 0x459F30 (0xE bytes): CommuHiLo_Phases[0] - jmp
// [CommuHiLo_OpenStates + 0x939A40 * 4] (2).
extern "C" void __cdecl CommuHiLo_OpenDispatch(void) {
    Dispatch("CommuHiLo_OpenDispatch (0x459F30)", CommuHiLo_OpenStates, CommuHiLo_OpenStates_count, ::bof3::Img(Rest4b_BoardStep_addr));
}

// original 0x459F40 (0x36 bytes): CommuHiLo_OpenStates[0] - the dwords
// 0x675F98, 0x675F9C = -1 and the word 0x675FA0 = 0xFFFF (the values and
// pick 0 to 0xFF), Music_Track kept in 0x675F90; Music_FadeOutStop(10);
// 0x939A40 (read after) + 1.
extern "C" void __cdecl CommuHiLo_OpenFade(void) {
    const unsigned char track = Music_Track;
    SetL(::bof3::Img(at::kCards.v), 0xFFFFFFFFu);
    SetL(::bof3::Img(at::kCards.v) + 4, 0xFFFFFFFFu);
    Rest4d_KeptTrack = track;
    SetWord(At(::bof3::Img(at::kCards.v) + 8), 0xFFFF);
    SH_CALL(Music_FadeOutStop)(10);
    Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
}

// original 0x459F80 (0x2C bytes): CommuHiLo_OpenStates[1] - once
// File_LoadDone: Music_Play(0x94, 8), 0x939A40 = 0, 0x939A3E (read after) + 1.
extern "C" void __cdecl CommuHiLo_OpenMusic(void) {
    if (SH_CALL(File_LoadDone)() == 0) return;
    SH_CALL(Music_Play)(0x94, 8);
    const auto phase = static_cast<unsigned char>(Rest4b_BoardState + 1);
    Rest4b_BoardStep = 0;
    Rest4b_BoardState = phase;
}

// original 0x459FB0 (0xE bytes): CommuHiLo_Phases[1] - jmp
// [CommuHiLo_PlayStates + 0x939A40 * 4] (13).
extern "C" void __cdecl CommuHiLo_PlayDispatch(void) {
    Dispatch("CommuHiLo_PlayDispatch (0x459FB0)", CommuHiLo_PlayStates, CommuHiLo_PlayStates_count, ::bof3::Img(Rest4b_BoardStep_addr));
}

// original 0x459FC0 (0xB0 bytes): CommuHiLo_PlayStates[0] - step 0: 0x675F95
// = 4, Sound_PlayEffect(0x102), 0x939A3F + 1. Then 0x675F95 down one a frame,
// the money box at (0x6E - 0x28 n, 0x48) and the stake box at (0x6E - 0x28 n,
// 0x74, 1) for n the counter (re-read); at 0 the message 0x71, Field_Request
// = 2, the stake 0, 0x675F8C 0, 0x939A40 + 1, 0x939A3F 0.
extern "C" void __cdecl CommuHiLo_Intro(void) {
    if (Rest4c_Step == 0) {
        Rest4d_Count = 4;
        Sound(0x102);
        Rest4c_Step = static_cast<unsigned char>(Rest4c_Step + 1);
        return;
    }
    const auto n = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = n;
    ZennyBox(0x6E - 0x28 * n, 0x48);
    StakeBox(0x6E - 0x28 * Rest4d_Count, 0x74, 1);
    if (Rest4d_Count != 0) return;
    Message(0x71);
    const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Field_Request = 2;
    SetL(::bof3::Img(at::kBet.v), 0);
    Rest4c_Count = 0;
    Rest4b_BoardStep = state;
    Rest4c_Step = 0;
}

// original 0x45A070 (0x27 bytes): CommuHiLo_PlayStates[1] - the money box at
// (0x6E, 0x48), the stake box at (0x6E, 0x74, 1); 0x939A40 + 1 once
// Field_Request is not 2.
extern "C" void __cdecl CommuHiLo_WaitIntro(void) {
    ZennyBox(0x6E, 0x48);
    StakeBox(0x6E, 0x74, 1);
    if (Request() != 2) Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
}

// original 0x45A0A0 (0xE bytes): CommuHiLo_PlayStates[2] - jmp
// [CommuHiLo_BetSteps + 0x939A3F * 4] (3).
extern "C" void __cdecl CommuHiLo_BetDispatch(void) {
    Dispatch("CommuHiLo_BetDispatch (0x45A0A0)", CommuHiLo_BetSteps, CommuHiLo_BetSteps_count, ::bof3::Img(Rest4c_Step_addr));
}

// original 0x45A0B0 (0x242 bytes): CommuHiLo_BetSteps[0] - the stake's three
// digits from the stake's low byte read signed (0x903852 its hundreds,
// 0x903851 its tens, 0x903850 its ones, each a truncating divide);
// Input_AutoRepeat(Input_Pressed & 0xF000): a press - Sound_PlayEffect(0x100);
// 0x8000 the digit cursor 0x675F8C up (3 and above signed -> 0), else 0x2000
// down (below 0 -> 2), else 0x4000 the digit at the cursor down (below 0 ->
// 9), else 0x1000 up (above 9 -> 0); then the stake = 100 h + 10 t + o (each
// signed): below 0 -> 0; else above 100 -> 100, and above Party_Zenny
// (unsigned) -> Party_Zenny's low word, signed. No press: confirm - Sound 0x102;
// a stake of 0 backs out (0x675F95 = 0, 0x939A3F + 2); else 0x675F95 = 4,
// Sound 0x104, Party_Zenny - the stake, 0x939A3F + 1; cancel - Sound 0x102 and
// the back-out. Last, while 0x939A3E is 1 and 0x939A40 2: the money box at
// (0x6E, 0x48), the stake box at (0x6E, 0x74, the cursor + 1) and the
// underline at (0xBA - 12 * cursor, 0x99). The cursor indexes the three digits:
// ours aborts outside 0..2 where it does.
extern "C" void __cdecl CommuHiLo_BetInput(void) {
    const int low = static_cast<signed char>(BRest4c(::bof3::Img(at::kBet.v)));
    at::kDigits.b_02 = static_cast<unsigned char>(low / 100);
    const int rest = static_cast<signed char>(static_cast<unsigned char>(low % 100));
    at::kDigits.b_01 = static_cast<unsigned char>(rest / 10);
    at::kDigits.b_00 = static_cast<unsigned char>(rest % 10);
    const unsigned pressed = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000u);
    if ((pressed & 0xFFFF) != 0) {
        Sound(0x100);
        if (pressed & 0x8000) {
            const auto c = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) + 1);
            Rest4c_Count = c;
            if (static_cast<signed char>(c) >= 3) Rest4c_Count = 0;
        } else if (pressed & 0x2000) {
            const auto c = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) - 1);
            Rest4c_Count = c;
            if (static_cast<signed char>(c) < 0) Rest4c_Count = 2;
        } else if (pressed & 0x4000) {
            const int i = Rest4c_Count;
            Room("CommuHiLo_BetInput (0x45A0B0)", "the digit cursor 0x675F8C", i, 0, 2);
            const auto d = static_cast<unsigned char>(reinterpret_cast<unsigned char*>(&at::kDigits)[i] - 1);
            reinterpret_cast<unsigned char*>(&at::kDigits)[i] = d;
            if (static_cast<signed char>(d) < 0) reinterpret_cast<unsigned char*>(&at::kDigits)[i] = 9;
        } else if (pressed & 0x1000) {
            const int i = Rest4c_Count;
            Room("CommuHiLo_BetInput (0x45A0B0)", "the digit cursor 0x675F8C", i, 0, 2);
            const auto d = static_cast<unsigned char>(reinterpret_cast<unsigned char*>(&at::kDigits)[i] + 1);
            reinterpret_cast<unsigned char*>(&at::kDigits)[i] = d;
            if (static_cast<signed char>(d) > 9) reinterpret_cast<unsigned char*>(&at::kDigits)[i] = 0;
        }
        const int stake = static_cast<signed char>(at::kDigits.b_02) * 100 + static_cast<signed char>(at::kDigits.b_01) * 10 + static_cast<signed char>(at::kDigits.b_00);
        SetL(::bof3::Img(at::kBet.v), static_cast<U>(stake));
        if (stake < 0) {
            SetL(::bof3::Img(at::kBet.v), 0);
        } else {
            int v = stake;
            if (v > 100) {
                v = 100;
                SetL(::bof3::Img(at::kBet.v), 100);
            }
            const U zenny = Party_Zenny;
            if (static_cast<U>(v) > zenny) SetL(::bof3::Img(at::kBet.v), static_cast<U>(static_cast<int>(static_cast<std::int16_t>(zenny))));
        }
    } else {
        const unsigned buttons = Input_Pressed;
        bool back = false;
        if (Field_ConfirmButtons & buttons) {
            Sound(0x102);
            if (L(::bof3::Img(at::kBet.v)) == 0) {
                back = true;
            } else {
                Rest4d_Count = 4;
                Sound(0x104);
                const U stake = L(::bof3::Img(at::kBet.v));
                const U zenny = Party_Zenny;
                const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
                Party_Zenny = zenny - stake;
                Rest4c_Step = step;
            }
        } else if (Field_CancelButtons & buttons) {
            Sound(0x102);
            back = true;
        }
        if (back) {
            const auto step = static_cast<unsigned char>(Rest4c_Step + 2);
            Rest4d_Count = 0;
            Rest4c_Step = step;
        }
    }
    if (Rest4b_BoardState != 1 || Rest4b_BoardStep != 2) return;
    ZennyBox(0x6E, 0x48);
    StakeBox(0x6E, 0x74, static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) + 1));
    SH_CALL(Commu_DrawUnderline)(0xBA - 12 * Rest4c_Count, 0x99);
}

// original 0x45A300 (0x75 bytes): CommuHiLo_BetSteps[1] - 0x675F95 down one;
// the money box at (0x12 n + 0x26, 0xE n + 0x10) and the stake box at (0xE
// (0xC - n), 0x19 n + 0x10, 0), n the counter (re-read); at 0 0x939A3F = 0,
// 0x939A40 + 1.
extern "C" void __cdecl CommuHiLo_BetSlide(void) {
    const auto n = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = n;
    ZennyBox(0x12 * n + 0x26, 0xE * n + 0x10);
    const int m = Rest4d_Count;
    StakeBox(0xE * (0xC - m), 0x19 * m + 0x10, 0);
    if (Rest4d_Count != 0) return;
    const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Rest4c_Step = 0;
    Rest4b_BoardStep = state;
}

// original 0x45A380 (0x56 bytes): CommuHiLo_BetSteps[2] - 0x675F95 up one; the
// boxes at (0x6E - 0x28 n, 0x48) and (0x6E - 0x28 n, 0x74, 1); at 4 a tail jmp
// to CommuHiLo_Quit.
extern "C" void __cdecl CommuHiLo_BetBack(void) {
    const auto n = static_cast<unsigned char>(Rest4d_Count + 1);
    Rest4d_Count = n;
    ZennyBox(0x6E - 0x28 * n, 0x48);
    StakeBox(0x6E - 0x28 * Rest4d_Count, 0x74, 1);
    if (Rest4d_Count == 4) SH_CALL(CommuHiLo_Quit)();
}

// original 0x45A3E0 (0xE bytes): CommuHiLo_PlayStates[3] - jmp
// [CommuHiLo_DealSteps + 0x939A3F * 4] (5).
extern "C" void __cdecl CommuHiLo_DealDispatch(void) {
    Dispatch("CommuHiLo_DealDispatch (0x45A3E0)", CommuHiLo_DealSteps, CommuHiLo_DealSteps_count, ::bof3::Img(Rest4c_Step_addr));
}

// original 0x45A3F0 (0x62 bytes): CommuHiLo_DealSteps[0] - the two boxes; once
// Field_Request is not 2: the picks' bytes 0x675FA2..0x675FAB 0, the shuffle,
// Sound_PlayEffect(0x102), 0x675F95 = 3, 0x675F8C = 0, 0x939A3F + 1.
extern "C" void __cdecl CommuHiLo_DealStart(void) {
    HiLoBoxes();
    if (Request() == 2) return;
    SetL(::bof3::Img(at::kPicks.v) + 1, 0);
    SetL(::bof3::Img(at::kPicks.v) + 5, 0);
    SetWord(At(::bof3::Img(at::kPicks.v) + 9), 0);
    SH_CALL(CommuHiLo_Shuffle)();
    Sound(0x102);
    const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
    Rest4d_Count = 3;
    Rest4c_Count = 0;
    Rest4c_Step = step;
}

// original 0x45A460 (0x121 bytes): CommuHiLo_DealSteps[1] - the two boxes;
// 0x675F95 down one; the backs dealt so far at (0x10 + 32 i, 0x6C) for i below
// 0x675F8C (re-read, signed), the next at ((2 c - n + 1) * 16, 0x6C) (c the
// count last read, n the counter); at 0 the count + 1: below 9 Sound 0x102 and
// the counter 2; at 9 the shown bits 0x675F8D = 0 and, when Rand & 3 is 0, one
// (Rand & 0xF below 8), two (below 0xE) or three distinct bits Rand & 7 set,
// then 0x939A3F + 1.
extern "C" void __cdecl CommuHiLo_DealCards(void) {
    HiLoBoxes();
    const auto n = static_cast<unsigned char>(Rest4d_Count - 1);
    unsigned char count = static_cast<unsigned char>(Rest4c_Count);
    Rest4d_Count = n;
    if (static_cast<signed char>(count) > 0) {
        int i = 0;
        do {
            Card(32 * i + 0x10, 0x6C, 0xFF);
            count = static_cast<unsigned char>(Rest4c_Count);
            ++i;
        } while (i < static_cast<signed char>(count));
    }
    const int c = static_cast<signed char>(count);
    Card((2 * c - Rest4d_Count + 1) * 16, 0x6C, 0xFF);
    if (Rest4d_Count != 0) return;
    const auto dealt = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) + 1);
    Rest4c_Count = dealt;
    if (dealt != 9) {
        Sound(0x102);
        Rest4d_Count = 2;
        return;
    }
    Rest4c_Cursor = 0;
    if ((Rnd() & 3) == 0) {
        const unsigned r = Rnd() & 0xF;
        unsigned char left = r < 8 ? 1 : (r < 0xE ? 2 : 3);
        do {
            const unsigned bit = Rnd() & 7;
            if ((Rest4c_Cursor & (1 << bit)) == 0) {
                Rest4c_Cursor = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Cursor) | (1u << bit));
                --left;
            }
        } while (left != 0);
    }
    Rest4c_Step = static_cast<unsigned char>(Rest4c_Step + 1);
}

// original 0x45A590 (0x7B bytes): CommuHiLo_DealSteps[2] - the row (9, 1, 0),
// the shown values, the two boxes; the message 0x73, Field_Request = 2,
// 0x939A3F (read after) + 1.
extern "C" void __cdecl CommuHiLo_DealHints(void) {
    Row(9, 1, 0);
    ShownValues();
    HiLoBoxes();
    Message(0x73);
    const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
    Field_Request = 2;
    Rest4c_Step = step;
}

// original 0x45A610 (0x8A bytes): CommuHiLo_DealSteps[3] - the row (9, 1, 0),
// the shown values, the two boxes; once Field_Request is not 2: Sound 0x102,
// 0x675F95 = 4, 0x939A3F + 1.
extern "C" void __cdecl CommuHiLo_DealWait(void) {
    Row(9, 1, 0);
    ShownValues();
    HiLoBoxes();
    if (Request() == 2) return;
    Sound(0x102);
    const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
    Rest4d_Count = 4;
    Rest4c_Step = step;
}

// original 0x45A6A0 (0xCA bytes): CommuHiLo_DealSteps[4] - the row (9, 1, 0);
// 0x675F95 down one; the three choices sliding in at (0x58, 0x88, 0xB8; 0x44
// - 0x1E n, mode 0) with n re-read for each; the two boxes; at 0 0x675F8C =
// 0x675F8D = 0, 0x939A40 + 1, 0x939A3F = 0.
extern "C" void __cdecl CommuHiLo_DealChoices(void) {
    Row(9, 1, 0);
    const auto n = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = n;
    Choice(0x58, 0x44 - 0x1E * n, 0, 0);
    Choice(0x88, 0x44 - 0x1E * Rest4d_Count, 1, 0);
    Choice(0xB8, 0x44 - 0x1E * Rest4d_Count, 2, 0);
    HiLoBoxes();
    if (Rest4d_Count != 0) return;
    const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Rest4c_Count = 0;
    Rest4c_Cursor = 0;
    Rest4b_BoardStep = state;
    Rest4c_Step = 0;
}

// original 0x45A770 (0x201 bytes): CommuHiLo_PlayStates[4] - the picks.
// Input_AutoRepeat(Input_Pressed & 0xA000): a press with fewer than 8 picks -
// Sound 0x100; no pick yet: the cursor 0x675F8D ^ 1 and the choices drawn
// selected 2; else 0x8000 the cursor down (below 0 -> 2), 0x2000 up (above 2
// -> 0). No press: confirm on the cursor 2 - Sound 0x104, 0x102, 0x675F95 =
// 0, 0x939A40 + 1; on another with fewer than 8 picks - pick n (0x675FA2 + n)
// = the cursor, n + 1, Sound 0x103, at 8 the cursor 2 and the choices selected
// 3; cancel with a pick - Sound 0x106, n - 1, at 0 the choices selected 2 (the
// cursor 2 -> 0 first). The choices otherwise selected by the count (re-read):
// below 1 -> 2, 8 and above -> 3, else 0xFF. Then the two boxes, the row (9, 1,
// the count), the marker at (32 (count + 2), 0x6E) below 8, and Menu_DrawHand at
// (48 cursor + 0x5E, 0x4A, 0). The pick index is 0..7: ours aborts outside it.
extern "C" void __cdecl CommuHiLo_Pick(void) {
    const unsigned pressed = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xA000u);
    enum { kByCount, kSelect } how = kByCount;
    unsigned selected = 0;
    if ((pressed & 0xFFFF) != 0) {
        if (Rest4c_Count < 8) {
            Sound(0x100);
            if (static_cast<unsigned char>(Rest4c_Count) == 0) {
                Rest4c_Cursor = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Cursor) ^ 1);
                how = kSelect, selected = 2;
            } else if (pressed & 0x8000) {
                const auto c = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Cursor) - 1);
                Rest4c_Cursor = c;
                if (static_cast<signed char>(c) < 0) Rest4c_Cursor = 2;
            } else if (pressed & 0x2000) {
                const auto c = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Cursor) + 1);
                Rest4c_Cursor = c;
                if (static_cast<signed char>(c) > 2) Rest4c_Cursor = 0;
            }
        }
    } else {
        const unsigned buttons = Input_Pressed;
        if (Field_ConfirmButtons & buttons) {
            const unsigned char cursor = static_cast<unsigned char>(Rest4c_Cursor);
            if (cursor == 2) {
                Sound(0x104);
                Sound(0x102);
                const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
                Rest4d_Count = 0;
                Rest4b_BoardStep = state;
            } else {
                const unsigned char n = static_cast<unsigned char>(Rest4c_Count);
                if (static_cast<signed char>(n) < 8) {
                    const int i = static_cast<signed char>(n);
                    Room("CommuHiLo_Pick (0x45A770)", "the pick count 0x675F8C", i, 0, 7);
                    BRest4c(::bof3::Img(at::kPicks.v) + 1 + i) = cursor;
                    Rest4c_Count = static_cast<unsigned char>(n + 1);
                    Sound(0x103);
                    if (static_cast<unsigned char>(Rest4c_Count) == 8) {
                        Rest4c_Cursor = 2;
                        how = kSelect, selected = 3;
                    }
                }
            }
        } else if (Field_CancelButtons & buttons) {
            if (static_cast<unsigned char>(Rest4c_Count) != 0) {
                Sound(0x106);
                const auto n = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) - 1);
                Rest4c_Count = n;
                if (n == 0) {
                    if (static_cast<unsigned char>(Rest4c_Cursor) == 2) Rest4c_Cursor = 0;
                    how = kSelect, selected = 2;
                }
            }
        }
    }
    if (how == kByCount) {
        const signed char n = Rest4c_Count;
        selected = n < 1 ? 2 : (n >= 8 ? 3 : 0xFF);
    }
    Choices(0x58, 0x44, selected);
    HiLoBoxes();
    Row(9, 1, static_cast<unsigned char>(Rest4c_Count));
    const signed char n = Rest4c_Count;
    if (n < 8) SH_CALL(CommuHiLo_DrawMarker)((n + 2) * 32, 0x6E);
    SH_CALL(Menu_DrawHand)(Rest4c_Cursor * 48 + 0x5E, 0x4A, 0);
}

// original 0x45A980 (0xBE bytes): CommuHiLo_PlayStates[5] - the two boxes, the
// row (count + 1, 1, count); 0x675F95 up one; the backs past the count at (0x10
// + 32 i, 0x28 n + 0x6C) for i from count + 1 below 9, n re-read; the choices
// at (0x58, 0x44 - 0x28 n, 2); at 4 0x675F95 = 0x1E, the turn 0x675F8E = 1,
// 0x939A40 + 1.
extern "C" void __cdecl CommuHiLo_PickClose(void) {
    HiLoBoxes();
    const unsigned char count = static_cast<unsigned char>(Rest4c_Count);
    Row(static_cast<unsigned char>(count + 1), 1, count);
    int i = Rest4c_Count;
    Rest4d_Count = static_cast<unsigned char>(Rest4d_Count + 1);
    for (++i; i < 9; ++i) Card(32 * i + 0x10, 0x28 * Rest4d_Count + 0x6C, 0xFF);
    Choices(0x58, 0x44 - 0x28 * Rest4d_Count, 2);
    if (Rest4d_Count != 4) return;
    const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Rest4d_Count = 0x1E;
    Rest4c_Turn = 1;
    Rest4b_BoardStep = state;
}

// original 0x45AA40 (0xF0 bytes): CommuHiLo_PlayStates[6] - the two boxes;
// 0x675F95 down one; at 0 card t (the turn 0x675F8E, 1..8) is judged: pick t
// equal to (value t - 1 >= value t, unsigned) - Sound 0x104, the turn + 1: past
// the picks the turn back one, the lost flag 0x675F8F 0, 0x675F95 = 0x3C,
// 0x939A40 + 1; else 0x675F95 = 0x1E; not equal - Sound 0x107, the lost flag 1,
// 0x675F95 = 0x3C, 0x939A40 + 1. Last the row (count + 1, turn, count), count
// and turn read after the sound. Ours aborts on a turn outside 1..8.
extern "C" void __cdecl CommuHiLo_Reveal(void) {
    HiLoBoxes();
    const auto n = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = n;
    if (n == 0) {
        const int t = Rest4c_Turn;
        Room("CommuHiLo_Reveal (0x45AA40)", "the turn 0x675F8E", t, 1, 8);
        const unsigned char higher = BRest4c(::bof3::Img(at::kCards.v) + t - 1) >= BRest4c(::bof3::Img(at::kCards.v) + t) ? 1 : 0;
        if (BRest4c(::bof3::Img(at::kPicks.v) + t) == higher) {
            Sound(0x104);
            const auto turn = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Turn) + 1);
            const unsigned char count = static_cast<unsigned char>(Rest4c_Count);
            Rest4c_Turn = turn;
            if (static_cast<signed char>(turn) == static_cast<signed char>(count) + 1) {
                const auto back = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Turn) - 1);
                const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
                Rest4c_Turn = back;
                Rest4c_Lost = 0;
                Rest4d_Count = 0x3C;
                Rest4b_BoardStep = state;
            } else {
                Rest4d_Count = 0x1E;
            }
        } else {
            Sound(0x107);
            const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
            Rest4c_Lost = 1;
            Rest4d_Count = 0x3C;
            Rest4b_BoardStep = state;
        }
    }
    const unsigned char count = static_cast<unsigned char>(Rest4c_Count);
    Row(static_cast<unsigned char>(count + 1), static_cast<unsigned char>(Rest4c_Turn), count);
}

// original 0x45AB30 (0x54 bytes): CommuHiLo_PlayStates[7] - the two boxes, the
// row (count + 1, count + 1, count); 0x675F95 down one, at 0 Sound 0x102 and
// 0x939A40 + 1.
extern "C" void __cdecl CommuHiLo_RevealPause(void) {
    HiLoBoxes();
    const unsigned char count = static_cast<unsigned char>(Rest4c_Count);
    const auto all = static_cast<unsigned char>(count + 1);
    Row(all, all, count);
    const auto n = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = n;
    if (n != 0) return;
    Sound(0x102);
    Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
}

// original 0x45AB90 (0xDC bytes): CommuHiLo_PlayStates[8] - 0x675F95 up one;
// values 0..count (re-read, signed) at (0x10 + 32 i, 0x28 n + 0x6C); at 4: lost
// - the stake 0 and the message 0x74; else the stake = (factor * stake + 50) /
// 100 (32 bits, signed; the factor the u16 0x652CF2 + 2 * turn) and the message
// 0x76; Field_Request = 2, 0x939A40 + 1. Last the two boxes. Ours aborts on a
// value index past 8 or a turn outside 1..8.
extern "C" void __cdecl CommuHiLo_Payout(void) {
    const auto n = static_cast<unsigned char>(Rest4d_Count + 1);
    const unsigned char count = static_cast<unsigned char>(Rest4c_Count);
    Rest4d_Count = n;
    if (static_cast<signed char>(count) >= 0) {
        int i = 0;
        do {
            Room("CommuHiLo_Payout (0x45AB90)", "the value", i, 0, 8);
            const unsigned char v = BRest4c(::bof3::Img(at::kCards.v) + i);
            Card(32 * i + 0x10, 0x28 * Rest4d_Count + 0x6C, v);
            ++i;
        } while (i <= Rest4c_Count);
    }
    if (Rest4d_Count == 4) {
        unsigned message;
        if (Rest4c_Lost != 0) {
            SetL(::bof3::Img(at::kBet.v), 0);
            message = 0x74;
        } else {
            const int t = Rest4c_Turn;
            Room("CommuHiLo_Payout (0x45AB90)", "the turn 0x675F8E", t, 1, 8);
            const U factor = Word(At(::bof3::Img(at::kPayouts.v) + 2 * t));
            const auto product = static_cast<std::int32_t>(factor * L(::bof3::Img(at::kBet.v)) + 0x32u);
            SetL(::bof3::Img(at::kBet.v), static_cast<U>(product / 100));
            message = 0x76;
        }
        Message(message);
        const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
        Field_Request = 2;
        Rest4b_BoardStep = state;
    }
    HiLoBoxes();
}

// original 0x45AC70 (0x67 bytes): CommuHiLo_PlayStates[9] - once Field_Request
// is not 2: winnings of 10000 or more (signed) - the message 0x79, Field_Request
// = 2, 0x939A40 + 1; else the message 0x78, Field_Request = 2, 0x939A40 = 3.
// Then the two boxes.
extern "C" void __cdecl CommuHiLo_CheckWinnings(void) {
    if (Request() != 2) {
        if (static_cast<std::int32_t>(L(::bof3::Img(at::kBet.v))) >= 10000) {
            Message(0x79);
            const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
            Field_Request = 2;
            Rest4b_BoardStep = state;
        } else {
            Message(0x78);
            Field_Request = 2;
            Rest4b_BoardStep = 3;
        }
    }
    HiLoBoxes();
}

// original 0x45ACE0 (0x4C bytes): CommuHiLo_PlayStates[10] - once Field_Request
// is not 2: Zenny_Add(the winnings, 0), the message 0x75, the winnings 0,
// Field_Request = 2 (the state is left to the message's choice). Then the two
// boxes.
extern "C" void __cdecl CommuHiLo_CashOut(void) {
    if (Request() != 2) {
        SH_CALL(Zenny_Add)(L(::bof3::Img(at::kBet.v)), 0);
        Message(0x75);
        SetL(::bof3::Img(at::kBet.v), 0);
        Field_Request = 2;
    }
    HiLoBoxes();
}

// original 0x45AD30 (0xFC bytes): CommuHiLo_PlayStates[11] - a message up: the
// two boxes. Step 0: the two boxes, Sound 0x102, 0x675F95 = 4, 0x939A3F + 1.
// Then 0x675F95 down one; the money box at (0x6E - 0x12 n, 0x48 - 0xE n) and
// the stake box at (0xF n + 0x6E, 0x74 - 0x19 n, 0); at 0 the message 0x71,
// Field_Request = 2, the stake 0, the count 0, 0x939A40 = 1, 0x939A3F = 0.
extern "C" void __cdecl CommuHiLo_Restart(void) {
    if (Request() == 2) {
        HiLoBoxes();
        return;
    }
    if (Rest4c_Step == 0) {
        HiLoBoxes();
        Sound(0x102);
        const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
        Rest4d_Count = 4;
        Rest4c_Step = step;
        return;
    }
    const auto n = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = n;
    ZennyBox(0x6E - 0x12 * n, 0x48 - 0xE * n);
    const int m = Rest4d_Count;
    StakeBox(0xF * m + 0x6E, 0x74 - 0x19 * m, 0);
    if (Rest4d_Count != 0) return;
    Message(0x71);
    Field_Request = 2;
    SetL(::bof3::Img(at::kBet.v), 0);
    Rest4c_Count = 0;
    Rest4b_BoardStep = 1;
    Rest4c_Step = 0;
}

// original 0x45AE30 (0xCB bytes): CommuHiLo_PlayStates[12] - a message up: the
// two boxes. Step 0: the two boxes, Sound 0x102, 0x675F95 = 0, 0x939A3F + 1.
// Then 0x675F95 up one; the money box at (0x26, 0x10 - 0x14 n) and the stake
// box at (0xA8, 0x10 - 0x14 n, 0); at 4 0x939A3E = 2, 0x939A40 = 0.
extern "C" void __cdecl CommuHiLo_Leave(void) {
    if (Request() == 2) {
        HiLoBoxes();
        return;
    }
    if (Rest4c_Step == 0) {
        HiLoBoxes();
        Sound(0x102);
        const auto step = static_cast<unsigned char>(Rest4c_Step + 1);
        Rest4d_Count = 0;
        Rest4c_Step = step;
        return;
    }
    const auto n = static_cast<unsigned char>(Rest4d_Count + 1);
    Rest4d_Count = n;
    ZennyBox(0x26, 0x10 - 0x14 * n);
    StakeBox(0xA8, 0x10 - 0x14 * Rest4d_Count, 0);
    if (Rest4d_Count != 4) return;
    Rest4b_BoardState = 2;
    Rest4b_BoardStep = 0;
}

// original 0x45AF00 (0x54 bytes): CommuHiLo_Phases[2] - the close phase
// (music back, the field hook's tail state + 1).
extern "C" void __cdecl CommuHiLo_Close(void) { ClosePhase(); }

// original 0x45AF60 (0x89 bytes): the pick marker - a POLY_F3 (0x5A7570) at
// the packet cursor read first: (x, y), (x - 6, y - 6), (x + 6, y - 6) as
// floats of the s16 arguments; red and blue 0, green ((Frame_Counter & 8 ?
// Frame_Counter : ~Frame_Counter) & 6) << 5 + 0x3F (a byte); Gfx_CommitPrim(1,
// 0x2C). (The original keeps its temporaries in its own argument slots.)
extern "C" void __cdecl CommuHiLo_DrawMarker(int x, int y) {
    unsigned char frame = static_cast<unsigned char>(Frame_Counter);
    if ((frame & 8) == 0) frame = static_cast<unsigned char>(~frame);
    const auto green = static_cast<unsigned char>(((frame & 6) << 5) + 0x3F);
    unsigned char* const p = Gfx_PacketNext;
    SH_AT(void (__cdecl*)(unsigned char*), at::kSetPolyF3.v)(p);
    const int x0 = S16Rest4c(static_cast<U>(x)), y0 = S16Rest4c(static_cast<U>(y));
    PutFloat(p, 8, x0);
    p[5] = green;
    PutFloat(p, 0xC, y0);
    PutFloat(p, 0x14, x0 - 6);
    p[4] = 0;
    p[6] = 0;
    PutFloat(p, 0x18, y0 - 6);
    PutFloat(p, 0x20, x0 + 6);
    PutFloat(p, 0x24, y0 - 6);
    SH_CALL(Gfx_CommitPrim)(1, 0x2C);
}

// original 0x45AFF0 (0x85 bytes): the row of cards - for i below count (low
// byte; none for 0): y 0x6C, or, for 0 < i <= picks (low byte, not 0), 0x74
// when pick i (0x675FA1 + i) is not 0 and 0x64 when it is; the card at (0x10
// + 32 i, y) is value i (0x675F98 + i) for i below shown (low byte) and the
// back (0xFF) past it. The picks run 1..8 and the values 0..8: ours aborts on
// an index past them.
extern "C" void __cdecl CommuHiLo_DrawRow(unsigned count, unsigned shown, unsigned picks) {
    const int n = static_cast<int>(count & 0xFF);
    const int m = static_cast<int>(shown & 0xFF);
    const int k = static_cast<int>(picks & 0xFF);
    for (int i = 0; i < n; ++i) {
        int y = 0x6C;
        if (i <= k && k != 0 && i != 0) {
            Room("CommuHiLo_DrawRow (0x45AFF0)", "the pick", i, 1, 8);
            y = BRest4c(::bof3::Img(at::kPicks.v) + i) != 0 ? 0x74 : 0x64;
        }
        if (i < m) {
            Room("CommuHiLo_DrawRow (0x45AFF0)", "the value", i, 0, 8);
            Card(0x10 + 32 * i, y, BRest4c(::bof3::Img(at::kCards.v) + i));
        } else {
            Card(0x10 + 32 * i, y, 0xFF);
        }
    }
}

// original 0x45B080 (0x44 bytes): the nine values 0x675F98.. a shuffle of
// 1..9 - each Commu_RandDigit(9) (its al) drawn again while an earlier value
// has it.
extern "C" void __cdecl CommuHiLo_Shuffle(void) {
    BRest4c(::bof3::Img(at::kCards.v)) = static_cast<unsigned char>(SH_CALL(Commu_RandDigit)(9));
    for (int i = 1; i < 9; ++i) {
        unsigned char v;
        int j;
        do {
            v = static_cast<unsigned char>(SH_CALL(Commu_RandDigit)(9));
            for (j = 0; j < i && BRest4c(::bof3::Img(at::kCards.v) + j) != v; ++j) {}
        } while (j != i);
        BRest4c(::bof3::Img(at::kCards.v) + i) = v;
    }
}

// original 0x45B0D0 (0x2B bytes): Rand & 0xF drawn until it is 1..top (top =
// the argument's low byte); answers it in al, Rand's upper bytes above. The
// original writes the draw into its own argument slot (dead after). A top of
// 0 never ends in the original: ours aborts before the loop.
extern "C" unsigned __cdecl Commu_RandDigit(unsigned top) {
    const int limit = static_cast<int>(top & 0xFF) + 1;
    if (limit == 1)
        bof3::Fatal("Commu_RandDigit (0x45B0D0): a top of 0 - the original draws for ever (docs/rest_4c.md section 7)");
    for (;;) {
        const unsigned r = Rnd();
        const unsigned v = r & 0xF;
        if (v != 0 && static_cast<int>(v) < limit) return (r & 0xFFFFFF00u) | v;
    }
}

// original 0x45B100 (0xD4 bytes): the three choices at x, x + 0x30, x + 0x60
// (labels 0, 1, 2), the modes by selected's low byte: 0 -> 1, 2, 0; 1 -> 2, 1,
// 0; 2 -> 0, 0, 2; 3 -> 2, 2, 0; 0xFF -> 0, 0, 0; any other nothing.
extern "C" void __cdecl CommuHiLo_DrawChoices(int x, int y, unsigned selected) {
    unsigned a, b, c;
    switch (selected & 0xFF) {
    case 0: a = 1, b = 2, c = 0; break;
    case 1: a = 2, b = 1, c = 0; break;
    case 2: a = 0, b = 0, c = 2; break;
    case 3: a = 2, b = 2, c = 0; break;
    case 0xFF: a = 0, b = 0, c = 0; break;
    default: return;
    }
    Choice(x, y, 0, a);
    Choice(x + 0x30, y, 1, b);
    Choice(x + 0x60, y, 2, c);
}

// original 0x45B1E0 (0xD1 bytes): one choice at (x, y): Menu_DrawBox(x, y,
// 0x2A, 0x14, 0, the style byte read first); the draw mode
// (Gpu_SetDrawMode(cursor, 0, 0, 0x1D, 0), Gfx_CommitPrim(1, 0xC)); by mode's
// low byte: 0 piece 0xA and the label in colour 0, 1 piece 9 and colour 0, 2
// piece 0xA and colour 7 - the label Text_DrawAt(x + 0xB, y + 3, colour, 2,
// 0x669F54[label's low byte]); any other mode nothing more. The label table
// has 3 entries; ours aborts past them.
//
// DIVERGENCE DIV-0120 (DIV-0064's group 19): once a language overlay has
// written the three choices, the label is the overlay's word (HIGH / LOW /
// SET, Haut / Bas / Mise, Hoch / Tief / Setz) drawn whole and centred in the
// 0x2A box as the discs place it - COMMU02.EMI's routine (US 0x801D2BD0)
// puts a four-letter word at x + 6 and a three-letter one at x + 0xA, the
// box's middle for 8-px letters, here (0x2C - its pen width) / 2.
extern "C" void __cdecl CommuHiLo_DrawChoice(int x, int y, unsigned label, unsigned mode) {
    const unsigned style = static_cast<unsigned char>(Config_WindowColour);
    SH_CALL(Menu_DrawBox)(x, y, 0x2A, 0x14, 0, static_cast<int>(style));
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1D, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned m = mode & 0xFF;
    if (m > 2) return;
    Piece(x, y, m == 1 ? 9 : 0xA);
    const unsigned i = label & 0xFF;
    Room("CommuHiLo_DrawChoice (0x45B1E0)", "the label", static_cast<int>(i), 0, at::kChoiceLabelsCount - 1);
    if (Labels_Written(19)) {
        const unsigned char* const word = Labels_Slot(19, i);
        const int word_x = x + (0x2C - static_cast<int>(TextAdvance_Width(word))) / 2;
        SH_CALL(Text_DrawAt)(word_x, y + 3, m == 2 ? 7 : 0, 0xFF, word);
        return;
    }
    const auto* const text = reinterpret_cast<const unsigned char*>(
        static_cast<std::uintptr_t>(L(::bof3::Img(at::kChoiceLabels.v) + 4 * i)));
    SH_CALL(Text_DrawAt)(x + 0xB, y + 3, m == 2 ? 7 : 0, 2, text);
}

// original 0x45B2C0 (0xB0 bytes): a card. Gpu_SetDrawMode(Gfx_PacketNext, 0,
// 0, 0x1E, 0), Gfx_CommitPrim(1, 0xC); then a SPRT at the new cursor
// (Gpu_SetSprt): shade 0x80, (s16 x, s16 y) as floats, 0x20 x 0x28; digit 0xFF
// the back (u, v 0, 0; the word 0x7BC8), any other u = (d & 7) << 5, v = (d >>
// 3) * 0x28 (a byte), the word 0x7BC9; Gfx_CommitPrim(1, 0x1C).
extern "C" void __cdecl Commu_DrawCard(int x, int y, unsigned digit) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1E, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(p);
    p[4] = p[5] = p[6] = 0x80;
    PutFloat(p, 8, S16Rest4c(static_cast<U>(x)));
    PutFloat(p, 0xC, S16Rest4c(static_cast<U>(y)));
    PutWord(p, 0x18, 0x20);
    PutWord(p, 0x1A, 0x28);
    const auto d = static_cast<unsigned char>(digit);
    if (d == 0xFF) {
        p[0x14] = 0;
        p[0x15] = 0;
        PutWord(p, 0x16, 0x7BC8);
    } else {
        p[0x14] = static_cast<unsigned char>((d & 7) << 5);
        p[0x15] = static_cast<unsigned char>((d >> 3) * 0x28);
        PutWord(p, 0x16, 0x7BC9);
    }
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x45B370 (0x2F bytes): the first game's quit - Sound_PlayEffect(0x106),
// the message 0x72, Field_Request = 2, 0x939A3E (read after) + 1, 0x939A40 = 0.
// Reached by CommuHiLo_BetBack's tail jmp (hidden in Commu_DrawCard's extent).
extern "C" void __cdecl CommuHiLo_Quit(void) {
    Sound(0x106);
    Message(0x72);
    const auto phase = static_cast<unsigned char>(Rest4b_BoardState + 1);
    Field_Request = 2;
    Rest4b_BoardState = phase;
    Rest4b_BoardStep = 0;
}

// original 0x45B3A0 (0x57 bytes): a LINE_F2 (Gpu_SetLineF2) at the packet
// cursor from (s16 x, s16 y) to (x + 0xC, y), shade 0x80; Gfx_CommitPrim(1,
// 0x20) - the stake digit's underline.
extern "C" void __cdecl Commu_DrawUnderline(int x, int y) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    const int x0 = S16Rest4c(static_cast<U>(x)), y0 = S16Rest4c(static_cast<U>(y));
    PutFloat(p, 8, x0);
    PutFloat(p, 0x14, x0 + 0xC);
    p[4] = p[5] = p[6] = 0x80;
    PutFloat(p, 0xC, y0);
    PutFloat(p, 0x18, y0);
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x45B400 (0x82 bytes): a menu piece - a SPRT at the packet cursor
// at (s16 x, s16 y), its u, v, w, h the four bytes of 0x652D04 + 4 * (piece's
// low byte), shade 0x80, the word 0x7887; Gfx_CommitPrim(1, 0x1C). The table
// has 18 entries (to the state table 0x652D4C); ours aborts past them.
extern "C" void __cdecl Commu_DrawPiece(int x, int y, unsigned piece) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(p);
    const unsigned i = piece & 0xFF;
    Room("Commu_DrawPiece (0x45B400)", "the piece", static_cast<int>(i), 0, at::kPiecesCount - 1);
    const unsigned char* const e = At(::bof3::Img(at::kPieces.v) + 4 * i);
    PutFloat(p, 8, S16Rest4c(static_cast<U>(x)));
    PutFloat(p, 0xC, S16Rest4c(static_cast<U>(y)));
    PutWord(p, 0x18, e[2]);
    PutWord(p, 0x1A, e[3]);
    p[4] = p[5] = p[6] = 0x80;
    p[0x14] = e[0];
    p[0x15] = e[1];
    PutWord(p, 0x16, 0x7887);
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x45B490 (0x89 bytes): the money box at (x, y): Menu_DrawBox(x + 1,
// y + 1, 0x6A, 0x28, 0x80, the style byte 0x903A5A read first), its frame,
// the title (Text_DrawAt(x + 0x24, y + 7, 0, 3, 0x669F60)), Party_Zenny (read
// after the title) through sprintf(0x904BA0, 0x64ADDC) in the 12-dot font at
// (x + 6, y + 0x18), the unit (Text_DrawAt(x + 0x5A, y + 0x18, 0, 1,
// 0x66A31C)).
extern "C" void __cdecl Commu_DrawZennyBox(int x, int y) {
    const unsigned style = static_cast<unsigned char>(Config_WindowColour);
    SH_CALL(Menu_DrawBox)(x + 1, y + 1, 0x6A, 0x28, 0x80, static_cast<int>(style));
    SH_CALL(Commu_DrawFrame)(x, y);
    SH_CALL(Text_DrawAt)(x + 0x24, y + 7, 0, Labels_Written(14) ? 0xFF : 3,
                         Labels_Written(14) ? Labels_Slot(14, 3) : Text(::bof3::Img(at::kZennyTitle.v)));   // DIV-0064 group 14
    SH_CALL(Crt_sprintf)(Buffer(), reinterpret_cast<const char*>(At(::bof3::Img(at::kFmtWide.v))), Party_Zenny);
    SH_CALL(Text_DrawFont12)(x + 6, y + 0x18, 0, reinterpret_cast<const unsigned char*>(Buffer()));
    SH_CALL(Text_DrawAt)(x + 0x5A, y + 0x18, 0, 1, Text(::bof3::Img(at::kZennyUnit.v)));
}

// original 0x45B520 (0xCF bytes): the boxes' frame of pieces at (x, y): the
// draw mode (Gpu_SetDrawMode(cursor, 0, 0, 0x1D, 0), Gfx_CommitPrim(1, 0xC));
// the top row - piece 0 at x, six of 1 at x + 0x20 + 8 i, 2 at x + 0x50, 3 at
// x + 0x68; the sides at y + 0x18 and y + 0x1E - piece 4 at x, 5 at x + 0x68;
// the bottom row at y + 0x23 - 6 at x, twelve of 7 at x + 8 + 8 i, 8 at x +
// 0x68. (The original keeps x + 0x68 in its own second argument's slot.)
extern "C" void __cdecl Commu_DrawFrame(int x, int y) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1D, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    Piece(x, y, 0);
    for (int i = 0; i < 6; ++i) Piece(x + 8 * i + 0x20, y, 1);
    Piece(x + 0x50, y, 2);
    Piece(x + 0x68, y, 3);
    for (int i = 0; i < 2; ++i) {
        const int side = y + 6 * i + 0x18;
        Piece(x, side, 4);
        Piece(x + 0x68, side, 5);
    }
    const int bottom = y + 0x23;
    Piece(x, bottom, 6);
    for (int i = 0; i < 12; ++i) Piece(x + 8 * i + 8, bottom, 7);
    Piece(x + 0x68, bottom, 8);
}

// original 0x45B5F0 (0x177 bytes): the stake box at (x, y): as the money box
// with the title 0x669F68 and the amount 0x675F88 (read after the title);
// digits' low byte 2, 3, 4 with the amount below 10, 100, 1000 (signed) -
// sprintf by the narrow formats 0x65306C, 0x653074, 0x65307C at x + 0x42,
// + 0x36, + 0x2A; otherwise the wide format 0x64ADDC at x + 6. The unit as the
// money box's.
extern "C" void __cdecl Commu_DrawStakeBox(int x, int y, unsigned digits) {
    const unsigned style = static_cast<unsigned char>(Config_WindowColour);
    SH_CALL(Menu_DrawBox)(x + 1, y + 1, 0x6A, 0x28, 0x80, static_cast<int>(style));
    SH_CALL(Commu_DrawFrame)(x, y);
    SH_CALL(Text_DrawAt)(x + 0x24, y + 7, 0, Labels_Written(14) ? 0xFF : 3,
                         Labels_Written(14) ? Labels_Slot(14, 4) : Text(::bof3::Img(at::kBetTitle.v)));   // DIV-0064 group 14
    const auto amount = static_cast<std::int32_t>(L(::bof3::Img(at::kBet.v)));
    U format = ::bof3::Img(at::kFmtWide.v);
    int left = x + 6;
    switch (digits & 0xFF) {
    case 2:
        if (amount < 10) format = ::bof3::Img(at::kFmt1.v), left = x + 0x42;
        break;
    case 3:
        if (amount < 100) format = ::bof3::Img(at::kFmt2.v), left = x + 0x36;
        break;
    case 4:
        if (amount < 1000) format = ::bof3::Img(at::kFmt3.v), left = x + 0x2A;
        break;
    default:
        break;
    }
    SH_CALL(Crt_sprintf)(Buffer(), reinterpret_cast<const char*>(At(format)), amount);
    SH_CALL(Text_DrawFont12)(left, y + 0x18, 0, reinterpret_cast<const unsigned char*>(Buffer()));
    SH_CALL(Text_DrawAt)(x + 0x5A, y + 0x18, 0, 1, Text(::bof3::Img(at::kZennyUnit.v)));
}

// original 0x45B770 (0xE bytes): CommuGame[1] (R4B's 0x652A84) - jmp
// [CommuHitBlow_Phases + 0x939A3E * 4] (3).
extern "C" void __cdecl CommuHitBlow_Run(void) {
    Dispatch("CommuHitBlow_Run (0x45B770)", CommuHitBlow_Phases, CommuHitBlow_Phases_count, ::bof3::Img(Rest4b_BoardState_addr));
}

// original 0x45B780 (0xE bytes): CommuHitBlow_Phases[0] - jmp
// [CommuHitBlow_OpenStates + 0x939A40 * 4] (4).
extern "C" void __cdecl CommuHitBlow_OpenDispatch(void) {
    Dispatch("CommuHitBlow_OpenDispatch (0x45B780)", CommuHitBlow_OpenStates, CommuHitBlow_OpenStates_count,
             ::bof3::Img(Rest4b_BoardStep_addr));
}

// original 0x45B790 (0x21 bytes): CommuHitBlow_OpenStates[0] - Music_Track kept
// in 0x675F90, Music_FadeOutStop(10), 0x939A40 (read after) + 1.
extern "C" void __cdecl CommuHitBlow_OpenFade(void) { KeepMusicAndFade(); }

// original 0x45B7C0 (0x33 bytes): CommuHitBlow_OpenStates[1] - once
// File_LoadDone: Music_Play(0x94, 8), the message 0x7F, Field_Request = 2,
// 0x939A40 (read after) + 1.
extern "C" void __cdecl CommuHitBlow_OpenMusic(void) {
    if (SH_CALL(File_LoadDone)() == 0) return;
    SH_CALL(Music_Play)(0x94, 8);
    Message(0x7F);
    const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Field_Request = 2;
    Rest4b_BoardStep = state;
}

// original 0x45B800 (0x34 bytes): CommuHitBlow_OpenStates[2] - once
// Field_Request is not 2: Sound 0x102, 0x675F95 = 4, the stake 0, 0x939A40 + 1.
extern "C" void __cdecl CommuHitBlow_OpenWait(void) {
    if (Request() == 2) return;
    Sound(0x102);
    const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Rest4d_Count = 4;
    SetL(::bof3::Img(at::kBet.v), 0);
    Rest4b_BoardStep = state;
}

// original 0x45B840 (0xA9 bytes): CommuHitBlow_OpenStates[3] - 0x675F95 down
// one; the guess panel at (0x10 - 0x1E n, 0x18, 0), the money box at (0x1E n
// + 0xCC, 0x10), the stake box at (0x1E n + 0xCC, 0x3C, 0), a back at (0x68,
// 0x40 - 0x1E n), n re-read for each; at 0 0x939A40 = 0, 0x939A3E + 1.
extern "C" void __cdecl CommuHitBlow_OpenSlide(void) {
    const auto n = static_cast<unsigned char>(Rest4d_Count - 1);
    Rest4d_Count = n;
    GuessPanel(0x10 - 0x1E * n, 0x18, 0);
    ZennyBox(0x1E * Rest4d_Count + 0xCC, 0x10);
    StakeBox(0x1E * Rest4d_Count + 0xCC, 0x3C, 0);
    Card(0x68, 0x40 - 0x1E * Rest4d_Count, 0xFF);
    if (Rest4d_Count != 0) return;
    const auto phase = static_cast<unsigned char>(Rest4b_BoardState + 1);
    Rest4b_BoardStep = 0;
    Rest4b_BoardState = phase;
}

// original 0x45B8F0 (0xE bytes): CommuHitBlow_Phases[1] - jmp
// [CommuHitBlow_PlayStates + 0x939A40 * 4] (9).
extern "C" void __cdecl CommuHitBlow_PlayDispatch(void) {
    Dispatch("CommuHitBlow_PlayDispatch (0x45B8F0)", CommuHitBlow_PlayStates, CommuHitBlow_PlayStates_count,
             ::bof3::Img(Rest4b_BoardStep_addr));
}

// original 0x45B900 (0xE bytes): CommuHitBlow_PlayStates[0] - jmp
// [CommuHitBlow_StartSteps + 0x939A3F * 4] (2).
extern "C" void __cdecl CommuHitBlow_StartDispatch(void) {
    Dispatch("CommuHitBlow_StartDispatch (0x45B900)", CommuHitBlow_StartSteps, CommuHitBlow_StartSteps_count,
             ::bof3::Img(Rest4c_Step_addr));
}

// original 0x45B910 (0xBB bytes): CommuHitBlow_StartSteps[0] - the stake 500 and
// Party_Zenny - 500 (unchecked); three distinct digits 0x675F98.. by
// Commu_RandDigit(9) (each stored, then drawn again while an earlier one has
// it); the eight guess records 0x675F9B.. (40 bytes) all 1; the row 0; the
// guess panel (0x10, 0x18, 0), a back at (0x68, 0x40), the money box (0xCC,
// 0x10), the stake box (0xCC, 0x3C, 0); 0x675F95 = 4, Sound 0x102, 0x939A3F
// (read after) + 1.
extern "C" void __cdecl CommuHitBlow_Start(void) {
    const U zenny = Party_Zenny;
    SetL(::bof3::Img(at::kBet.v), 500);
    Party_Zenny = zenny - 500u;
    for (int i = 0; i < 3;) {
        const auto v = static_cast<unsigned char>(SH_CALL(Commu_RandDigit)(9));
        BRest4c(::bof3::Img(at::kCards.v) + i) = v;
        if (i != 0) {
            int j = 0;
            while (j < i && BRest4c(::bof3::Img(at::kCards.v) + j) != v) ++j;
            if (j != i) continue;
        }
        ++i;
    }
    std::memset(At(::bof3::Img(at::kRecords.v)), 1, 40);
    Rest4c_Count = 0;
    GuessPanel(0x10, 0x18, 0);
    Card(0x68, 0x40, 0xFF);
    ZennyBox(0xCC, 0x10);
    StakeBox(0xCC, 0x3C, 0);
    Rest4d_Count = 4;
    Sound(0x102);
    Rest4c_Step = static_cast<unsigned char>(Rest4c_Step + 1);
}

// original 0x45B9D0 (0x120 bytes): CommuHitBlow_StartSteps[1] - 0x675F95 down
// one; the guess panel (0x10, 0x18, 0), the money and stake boxes at 0xCC;
// three cards 1 at (8 (0x15 - n), 8 (0x11 - n), 0x68; 0x78 - 0xE n) and three
// backs at (8 (0x15 - n), 8 (0x11 - n), 0x68; 0x40), n re-read for each; at 0
// 0x939A3F = 0, 0x939A40 + 1.
extern "C" void __cdecl CommuHitBlow_StartSlide(void) {
    Rest4d_Count = static_cast<unsigned char>(Rest4d_Count - 1);
    GuessPanel(0x10, 0x18, 0);
    HitBlowBoxes();
    int n = Rest4d_Count;
    Card(8 * (0x15 - n), 0x78 - 0xE * n, 1);
    n = Rest4d_Count;
    Card(8 * (0x11 - n), 0x78 - 0xE * n, 1);
    n = Rest4d_Count;
    Card(0x68, 0x78 - 0xE * n, 1);
    Card(8 * (0x15 - Rest4d_Count), 0x40, 0xFF);
    Card(8 * (0x11 - Rest4d_Count), 0x40, 0xFF);
    Card(0x68, 0x40, 0xFF);
    if (Rest4d_Count != 0) return;
    const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Rest4c_Step = 0;
    Rest4b_BoardStep = state;
}

// original 0x45BAF0 (0x64 bytes): CommuHitBlow_PlayStates[1] - the message 0x80,
// Field_Request = 2, 0x939A40 (read after) + 1; the panels (0, 0, row 0).
extern "C" void __cdecl CommuHitBlow_Prompt(void) {
    Message(0x80);
    const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    Field_Request = 2;
    Rest4b_BoardStep = state;
    HitBlowPanels(0, 0, 0);
}

// original 0x45BB60 (0x61 bytes): CommuHitBlow_PlayStates[2] - once
// Field_Request is not 2: the cursor 0x675F8D 0, 0x939A40 + 1; the panels (0,
// 0, row 0).
extern "C" void __cdecl CommuHitBlow_WaitPrompt(void) {
    if (Request() != 2) {
        const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
        Rest4c_Cursor = 0;
        Rest4b_BoardStep = state;
    }
    HitBlowPanels(0, 0, 0);
}

// original 0x45BBD0 (0x14F bytes): CommuHitBlow_PlayStates[3] - the guess.
// Input_AutoRepeat(Input_Pressed & 0xF000): a press - Sound 0x100; the digit
// at 0x675F98 + (5 row + cursor + 3) (a byte sum, row and cursor read after
// the sound): 0x4000 down (below 1 signed -> 9), else 0x1000 up (above 9
// unsigned -> 1), else 0x2000 the cursor up (above 2 -> 0), else 0x8000 down
// (below 0 -> 2). No press: confirm - Sound 0x104, 0x939A40 + 1. Then the
// panels (0, 0, row 1). Ours aborts on a row outside 0..7 or a cursor outside
// 0..2 where the digit is indexed.
extern "C" void __cdecl CommuHitBlow_Input(void) {
    const unsigned pressed = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000u);
    if ((pressed & 0xFFFF) != 0) {
        Sound(0x100);
        const unsigned char row = static_cast<unsigned char>(Rest4c_Count);
        const unsigned char cursor = static_cast<unsigned char>(Rest4c_Cursor);
        const auto index = static_cast<unsigned char>(row * 5 + cursor + 3);
        if (pressed & (0x4000 | 0x1000)) {
            Room("CommuHitBlow_Input (0x45BBD0)", "the row 0x675F8C", static_cast<signed char>(row), 0, 7);
            Room("CommuHitBlow_Input (0x45BBD0)", "the cursor 0x675F8D", static_cast<signed char>(cursor), 0, 2);
        }
        if (pressed & 0x4000) {
            const auto d = static_cast<unsigned char>(BRest4c(::bof3::Img(at::kCards.v) + index) - 1);
            BRest4c(::bof3::Img(at::kCards.v) + index) = d;
            if (static_cast<signed char>(d) < 1) BRest4c(::bof3::Img(at::kCards.v) + index) = 9;
        } else if (pressed & 0x1000) {
            const auto d = static_cast<unsigned char>(BRest4c(::bof3::Img(at::kCards.v) + index) + 1);
            BRest4c(::bof3::Img(at::kCards.v) + index) = d;
            if (d > 9) BRest4c(::bof3::Img(at::kCards.v) + index) = 1;
        } else if (pressed & 0x2000) {
            const auto c = static_cast<unsigned char>(cursor + 1);
            Rest4c_Cursor = c;
            if (static_cast<signed char>(c) > 2) Rest4c_Cursor = 0;
        } else if (pressed & 0x8000) {
            const auto c = static_cast<unsigned char>(cursor - 1);
            Rest4c_Cursor = c;
            if (static_cast<signed char>(c) < 0) Rest4c_Cursor = 2;
        }
    } else if (Field_ConfirmButtons & Input_Pressed) {
        Sound(0x104);
        Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    }
    HitBlowPanels(0, 0, 1);
}

// original 0x45BD20 (0x1B3 bytes): CommuHitBlow_PlayStates[4] - the guess r
// (the row 0x675F8C, signed) scored: hits = its digits equal to the secret's
// in place, blows = equal at another place; record +3 = hits, +4 = blows. Not
// three hits: sprintf(Text_Records, Area08_MessageFormat, hits), likewise
// blows into 0x904D00, the message 0x81, 0x939A40 (read after) + 1. Three:
// sprintf(Text_Records, .., r + 1); the rank 0x675F8D = Rand & 3, a 3 lowered
// by a second Rand & 3 and held to 0..2; the prize (item, category) at
// 0x652D94 + 2 (3 row + rank) (row re-read); Item_NamePtr(category, item)'s 16
// bytes into 0x904D00; the message 0x82; 0x939A40 = 7. Then Field_Request = 2
// and the panels (0, 0, row 0). Ours aborts on a row outside 0..7 or a prize
// past the table's 24.
extern "C" void __cdecl CommuHitBlow_Score(void) {
    const int r = Rest4c_Count;
    Room("CommuHitBlow_Score (0x45BD20)", "the row 0x675F8C", r, 0, 7);
    unsigned char* const rec = At(::bof3::Img(at::kRecords.v) + 5 * r);
    unsigned char hits = 0, blows = 0;
    for (int j = 0; j < 3; ++j)
        if (BRest4c(::bof3::Img(at::kCards.v) + j) == rec[j]) ++hits;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (i != j && rec[i] == BRest4c(::bof3::Img(at::kCards.v) + j)) ++blows;
    rec[3] = hits;
    rec[4] = blows;
    char* const text_a = reinterpret_cast<char*>(At(::bof3::Img(bof3::addr::Text_Records)));
    const auto* const format = reinterpret_cast<const char*>(At(::bof3::Img(bof3::addr::Area08_MessageFormat)));
    if (hits != 3) {
        SH_CALL(Crt_sprintf)(text_a, format, static_cast<unsigned>(hits));
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kTextB.v))), format, static_cast<unsigned>(blows));
        Message(0x81);
        Rest4b_BoardStep = static_cast<unsigned char>(Rest4b_BoardStep + 1);
    } else {
        SH_CALL(Crt_sprintf)(text_a, format, r + 1);
        unsigned char rank = static_cast<unsigned char>(Rnd() & 3);
        Rest4c_Cursor = rank;
        if (static_cast<signed char>(rank) > 2) {
            const unsigned second = Rnd();
            auto v = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Cursor) - (second & 3));
            if (static_cast<signed char>(v) < 0) v = 0;
            else if (static_cast<signed char>(v) > 2) v = 2;
            Rest4c_Cursor = v;
            rank = v;
        }
        const int prize = Rest4c_Count * 3 + static_cast<signed char>(rank);
        Room("CommuHitBlow_Score (0x45BD20)", "the prize", prize, 0, at::kPrizesCount - 1);
        const unsigned item = BRest4c(::bof3::Img(at::kPrizes.v) + 2 * prize);
        const unsigned category = BRest4c(::bof3::Img(at::kPrizes.v) + 2 * prize + 1);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
        std::memcpy(At(::bof3::Img(at::kTextB.v)), name, 16);
        Message(0x82);
        Rest4b_BoardStep = 7;
    }
    Field_Request = 2;
    HitBlowPanels(0, 0, 0);
}

// original 0x45BEE0 (0xBE bytes): CommuHitBlow_PlayStates[5] - a message up:
// the guess panel (1), the row (0x68, 0x78, row, 0), the secret (0), the boxes.
// Else the row + 1: below 8 (signed) the last guess's digits copied into the
// new one and 0x939A40 = 1; else the message 0x85, Field_Request = 2,
// 0x939A40 + 1; then the guess panel (0), the row (0x68, 0x78, row - 1, 0),
// the secret (0), the boxes. Ours aborts on a new row outside 1..7 where it
// copies.
extern "C" void __cdecl CommuHitBlow_NextGuess(void) {
    if (Request() == 2) {
        GuessPanel(0x10, 0x18, 1);
        GuessRow(0x68, 0x78, static_cast<unsigned char>(Rest4c_Count), 0);
    } else {
        const auto row = static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) + 1);
        Rest4c_Count = row;
        if (static_cast<signed char>(row) < 8) {
            const int r = static_cast<signed char>(row);
            Room("CommuHitBlow_NextGuess (0x45BEE0)", "the new row 0x675F8C", r, 1, 7);
            unsigned char* const rec = At(::bof3::Img(at::kRecords.v) + 5 * r);
            for (int j = 0; j < 3; ++j) rec[j] = rec[j - 5];
            Rest4b_BoardStep = 1;
        } else {
            Message(0x85);
            const auto state = static_cast<unsigned char>(Rest4b_BoardStep + 1);
            Field_Request = 2;
            Rest4b_BoardStep = state;
        }
        GuessPanel(0x10, 0x18, 0);
        GuessRow(0x68, 0x78, static_cast<unsigned char>(static_cast<unsigned char>(Rest4c_Count) - 1), 0);
    }
    Secret(0x68, 0x40, 0);
    HitBlowBoxes();
}

// original 0x45BFA0 (0x77 bytes): CommuHitBlow_PlayStates[6] - once
// Field_Request is not 2: the message 0x86, Field_Request = 2, 0x939A40 = 8.
// The guess panel (1), the secret (1), the row (0x68, 0x78, the row or 7 for 8,
// 0), the boxes.
extern "C" void __cdecl CommuHitBlow_Lost(void) {
    if (Request() != 2) {
        Message(0x86);
        Field_Request = 2;
        Rest4b_BoardStep = 8;
    }
    GuessPanel(0x10, 0x18, 1);
    Secret(0x68, 0x40, 1);
    const unsigned char row = static_cast<unsigned char>(Rest4c_Count);
    GuessRow(0x68, 0x78, row == 8 ? 7 : row, 0);
    HitBlowBoxes();
}

// original 0x45C020 (0x9D bytes): CommuHitBlow_PlayStates[7] - once
// Field_Request is not 2: Inventory_Add(category, item, 1) for the prize
// 0x652D94 + 2 (3 row + rank) (a fourth word 0 pushed); al 0 - the message
// 0x83 and Field_Request = 2; 0x939A40 = 6. The guess panel (1), the secret
// (1), the row (0x68, 0x78, row, 0), the boxes. Ours aborts on a prize past
// the table's 24.
extern "C" void __cdecl CommuHitBlow_Prize(void) {
    if (Request() != 2) {
        const int prize = Rest4c_Count * 3 + Rest4c_Cursor;
        Room("CommuHitBlow_Prize (0x45C020)", "the prize", prize, 0, at::kPrizesCount - 1);
        const unsigned item = BRest4c(::bof3::Img(at::kPrizes.v) + 2 * prize);
        const unsigned category = BRest4c(::bof3::Img(at::kPrizes.v) + 2 * prize + 1);
        const unsigned char added = SH_CALL(Inventory_Add)(category, item, 1);
        if (added == 0) {
            Message(0x83);
            Field_Request = 2;
        }
        Rest4b_BoardStep = 6;
    }
    GuessPanel(0x10, 0x18, 1);
    Secret(0x68, 0x40, 1);
    GuessRow(0x68, 0x78, static_cast<unsigned char>(Rest4c_Count), 0);
    HitBlowBoxes();
}

// original 0x45C0C0 (0x2DD bytes): CommuHitBlow_PlayStates[8] - the end; the
// row r is 0x675F8C, 7 for 8. A message up: the secret (1), the row (r, 0), the
// guess panel (1), the boxes. Step 0: 0x675F95 = 0 and the same draws, then
// 0x939A3F + 1. Step 1: 0x675F95 up one; the last guess's three digits rising
// from (0xA8 - 16 n, 8 (0x11 - n), 0x68; 0x78 - 0xE n) and the secret's from
// the same x at 0x40 (third, second, first), n re-read for each; the guess
// panel (1), the boxes; at 4: 0x939A41 0 - 0x939A40 = 0x939A3F = 0 (a new
// game); else Sound 0x102, 0x675F95 = 0, 0x939A3F = 2. Any other step:
// 0x675F95 up one; the guess panel at (0x10 - 0x1E n, 0x18, 0), the boxes at
// 0x1E n + 0xCC, a back at (0x68, 0x40 - 0x1E n); at 5 0x939A40, 0x939A3F,
// 0x939A41 0 and 0x939A3E = 2. Ours aborts on a row outside 0..7 where it
// indexes the records.
extern "C" void __cdecl CommuHitBlow_End(void) {
    const unsigned char count = static_cast<unsigned char>(Rest4c_Count);
    const unsigned char r = count == 8 ? 7 : count;
    if (Request() == 2) {
        Secret(0x68, 0x40, 1);
        GuessRow(0x68, 0x78, r, 0);
        GuessPanel(0x10, 0x18, 1);
        HitBlowBoxes();
        return;
    }
    const unsigned char step = Rest4c_Step;
    if (step == 0) {
        Rest4d_Count = 0;
        Secret(0x68, 0x40, 1);
        GuessRow(0x68, 0x78, r, 0);
        GuessPanel(0x10, 0x18, 1);
        HitBlowBoxes();
        Rest4c_Step = static_cast<unsigned char>(Rest4c_Step + 1);
        return;
    }
    if (step == 1) {
        const auto n = static_cast<unsigned char>(Rest4d_Count + 1);
        Rest4d_Count = n;
        Room("CommuHitBlow_End (0x45C0C0)", "the row", r, 0, 7);
        const U rec = ::bof3::Img(at::kRecords.v) + 5 * r;
        Card(0xA8 - 16 * n, 0x78 - 0xE * n, BRest4c(rec + 2));
        int m = Rest4d_Count;
        Card(8 * (0x11 - m), 0x78 - 0xE * m, BRest4c(rec + 1));
        const unsigned char first = BRest4c(rec);
        m = Rest4d_Count;
        Card(0x68, 0x78 - 0xE * m, first);
        Card(0xA8 - 16 * Rest4d_Count, 0x40, BRest4c(::bof3::Img(at::kCards.v) + 2));
        Card(8 * (0x11 - Rest4d_Count), 0x40, BRest4c(::bof3::Img(at::kCards.v) + 1));
        Card(0x68, 0x40, BRest4c(::bof3::Img(at::kCards.v)));
        GuessPanel(0x10, 0x18, 1);
        HitBlowBoxes();
        if (Rest4d_Count != 4) return;
        if (Rest4c_Again == 0) {
            Rest4b_BoardStep = 0;
            Rest4c_Step = 0;
        } else {
            Sound(0x102);
            Rest4d_Count = 0;
            BRest4c(::bof3::Img(at::kStep.v)) = 2;
        }
        return;
    }
    const auto n = static_cast<unsigned char>(Rest4d_Count + 1);
    Rest4d_Count = n;
    GuessPanel(0x10 - 0x1E * n, 0x18, 0);
    ZennyBox(0x1E * Rest4d_Count + 0xCC, 0x10);
    StakeBox(0x1E * Rest4d_Count + 0xCC, 0x3C, 0);
    Card(0x68, 0x40 - 0x1E * Rest4d_Count, 0xFF);
    if (Rest4d_Count != 5) return;
    Rest4b_BoardStep = 0;
    Rest4c_Step = 0;
    Rest4c_Again = 0;
    Rest4b_BoardState = 2;
}

// original 0x45C3A0 (0x54 bytes): CommuHitBlow_Phases[2] - the close phase, as
// CommuHiLo_Close.
extern "C" void __cdecl CommuHitBlow_Close(void) { ClosePhase(); }

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x45C400: the board's box (x, y) 0x52 by 0x7A, its frame, then eight
// rows 14 apart. A row k's bytes start at 0x675F98 + (5k + 3) & 0xFF. Below the
// cursor row (s8 0x675F8C, read again each row), and at it when `shown`'s byte
// is not 0: the first three bytes as one line (format 0x653088) in colour 0, the
// fourth and the fifth as numbers in colours 2 and 1; at it with `shown` 0: the
// blank formats in colours 0, 2, 1; past it: the blanks in colour 7. Every row
// then the dash 0x653084 in Font8 at (x + 0x3B, row y + 9), colour 7 past the
// cursor, else 0.
extern "C" void __cdecl CommuBoard_DrawRows(int x, int y, unsigned shown) {
    char* const buf = Scratch();
    const auto text = reinterpret_cast<const unsigned char*>(buf);
    SH_CALL(Menu_DrawBox)(x, y, 0x52, 0x7A, 0, static_cast<unsigned char>(Config_WindowColour));
    SH_CALL(CommuBoard_DrawFrame)(x, y);
    for (int row = 0; row < 8; ++row) {
        const unsigned k = static_cast<unsigned char>(row * 5 + 3);
        const int ry = y + 14 * row;
        const int cursor = Rest4c_Count;
        int colour = 0;
        if (row == cursor && (shown & 0xFF) == 0) {
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(::bof3::Img(at::kBoardBlankB.v))));
            SH_CALL(Text_DrawFont12)(x + 6, ry + 7, 0, text);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(::bof3::Img(at::kBoardBlankA.v))));
            SH_CALL(Text_DrawFont12)(x + 0x2F, ry + 7, 2, text);
            SH_CALL(Text_DrawFont12)(x + 0x43, ry + 7, 1, text);
        } else if (row <= cursor) {
            const unsigned a = B(::bof3::Img(at::kName.v) + k), b = B(::bof3::Img(at::kName.v) + k + 1), c = B(::bof3::Img(at::kName.v) + k + 2);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(::bof3::Img(at::kBoardFmt3.v))), a, b, c);
            SH_CALL(Text_DrawFont12)(x + 6, ry + 7, 0, text);
            const unsigned d = B(::bof3::Img(at::kName.v) + k + 3);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(::bof3::Img(at::kNumberFmt.v))), d);
            SH_CALL(Text_DrawFont12)(x + 0x2F, ry + 7, 2, text);
            const unsigned e = B(::bof3::Img(at::kName.v) + k + 4);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(::bof3::Img(at::kNumberFmt.v))), e);
            SH_CALL(Text_DrawFont12)(x + 0x43, ry + 7, 1, text);
        } else {
            colour = 7;
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(::bof3::Img(at::kBoardBlankB.v))));
            SH_CALL(Text_DrawFont12)(x + 6, ry + 7, 7, text);
            SH_CALL(Crt_sprintf)(buf, reinterpret_cast<const char*>(At(::bof3::Img(at::kBoardBlankA.v))));
            SH_CALL(Text_DrawFont12)(x + 0x2F, ry + 7, 7, text);
            SH_CALL(Text_DrawFont12)(x + 0x43, ry + 7, 7, text);
        }
        SH_CALL(Text_DrawFont8)(x + 0x3B, ry + 9, colour, Text(::bof3::Img(at::kBoardText.v)));
    }
}

// original 0x45C700: the board's frame in R4C's pieces (0x45B400) after one
// draw-mode packet (tpage 0x1D) committed: the top-left corner 0xB at (x, y),
// nine top pieces 0xD at y - 1, the top-right 0xC at x + 0x4F; fourteen rows of
// the sides 0xE / 0xF at x and x + 0x50; the bottom-left 0x10 at y + 0x78, nine
// 0xD, the bottom-right 0x11.
extern "C" void __cdecl CommuBoard_DrawFrame(int x, int y) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1D, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    BoardPiece(x, y, 0xB);
    for (int i = 0; i < 9; ++i) BoardPiece(x + i * 8 + 8, y - 1, 0xD);
    BoardPiece(x + 0x4F, y, 0xC);
    for (int i = 0; i < 14; ++i) {
        const int yi = y + i * 8 + 8;
        BoardPiece(x, yi, 0xE);
        BoardPiece(x + 0x50, yi, 0xF);
    }
    const int bottom = y + 0x78;
    BoardPiece(x, bottom, 0x10);
    for (int i = 0; i < 9; ++i) BoardPiece(x + i * 8 + 8, bottom, 0xD);
    BoardPiece(x + 0x50, bottom, 0x11);
}

// original 0x45C7D0: row `row`'s first three bytes (0x675F98 + (5 * row + 3) &
// 0xFF) as R4C's cells 32 apart from x; cell i 8 higher when the column byte
// (s8 0x675F8D, read again each cell) is i and `lift`'s byte is not 0.
extern "C" void __cdecl CommuBoard_DrawRowCells(int x, int y, unsigned row, unsigned lift) {
    const unsigned k = static_cast<unsigned char>(row * 5 + 3);
    for (int i = 0; i < 3; ++i) {
        const bool up = Rest4c_Cursor == i && (lift & 0xFF) != 0;
        BoardCell(x + i * 32, up ? y - 8 : y, B(::bof3::Img(at::kName.v) + k + static_cast<U>(i)));
    }
}

// original 0x45C850: the first row's three bytes as cells 32 apart when
// `shown`'s byte is not 0, else three blank cells (0xFF).
extern "C" void __cdecl CommuBoard_DrawCells(int x, int y, unsigned shown) {
    if ((shown & 0xFF) != 0) {
        for (int i = 0; i < 3; ++i) BoardCell(x + i * 32, y, B(::bof3::Img(at::kName.v) + static_cast<U>(i)));
    } else {
        for (int i = 0; i < 3; ++i) BoardCell(x + i * 32, y, 0xFF);
    }
}

#pragma clang attribute pop

void CommuBoardFromRest4b_Inject() {
    BOF3_INJECT(CommuBoard_Dispatch);
    BOF3_INJECT(CommuBoard_Init);
    BOF3_INJECT(CommuBoard_Grid);
    BOF3_INJECT(CommuBoard_ModeDispatch);
    BOF3_INJECT(CommuBoard_StepDispatch);
    BOF3_INJECT(CommuBoard_PickRecord);
    BOF3_INJECT(CommuBoard_MoveRecord);
    BOF3_INJECT(CommuBoard_StepDispatchB);
    BOF3_INJECT(CommuBoard_PickRecordB);
    BOF3_INJECT(CommuBoard_MoveRecordB);
    BOF3_INJECT(CommuBoard_PickList);
    BOF3_INJECT(CommuBoard_PickListB);
    BOF3_INJECT(CommuBoard_PickListC);
    BOF3_INJECT(CommuBoard_Confirm);
    BOF3_INJECT(CommuBoard_DrawRecordCard);
    BOF3_INJECT(CommuBoard_DrawCardFrame);
    BOF3_INJECT(CommuBoard_DrawBar);
    BOF3_INJECT(CommuBoard_DrawPanel);
    BOF3_INJECT(CommuBoard_DrawPanelFrame);
    BOF3_INJECT(CommuBoard_DrawSlotLines);
    BOF3_INJECT(Commu_CountInSlot);
    BOF3_INJECT(Commu_NthInSlot);
    BOF3_INJECT(CommuBoard_MoveGridCursor);
    BOF3_INJECT(CommuBoard_DrawDigits);
    BOF3_INJECT(CommuBoard_DrawSprite);
    BOF3_INJECT(CommuBoard_DrawListBox);
    BOF3_INJECT(CommuBoard_DrawListFrame);
    BOF3_INJECT(CommuBoard_ListY);
    BOF3_INJECT(CommuBoard_DrawListBoxB);
    BOF3_INJECT(CommuBoard_SlotRecordXY);
    BOF3_INJECT(CommuBoard_CancelStep);
    BOF3_INJECT(CommuBoard_PlaceRecord);
    BOF3_INJECT(CommuBoard_SlotHelp);
    BOF3_INJECT(CommuBoard_PickHelp);
}

void CommuBoardFromRest4c_Inject() {
    BOF3_INJECT(Commu_PushSubscreen);
    BOF3_INJECT(CommuHiLo_Run);
    BOF3_INJECT(CommuHiLo_OpenDispatch);
    BOF3_INJECT(CommuHiLo_OpenFade);
    BOF3_INJECT(CommuHiLo_OpenMusic);
    BOF3_INJECT(CommuHiLo_PlayDispatch);
    BOF3_INJECT(CommuHiLo_Intro);
    BOF3_INJECT(CommuHiLo_WaitIntro);
    BOF3_INJECT(CommuHiLo_BetDispatch);
    BOF3_INJECT(CommuHiLo_BetInput);
    BOF3_INJECT(CommuHiLo_BetSlide);
    BOF3_INJECT(CommuHiLo_BetBack);
    BOF3_INJECT(CommuHiLo_DealDispatch);
    BOF3_INJECT(CommuHiLo_DealStart);
    BOF3_INJECT(CommuHiLo_DealCards);
    BOF3_INJECT(CommuHiLo_DealHints);
    BOF3_INJECT(CommuHiLo_DealWait);
    BOF3_INJECT(CommuHiLo_DealChoices);
    BOF3_INJECT(CommuHiLo_Pick);
    BOF3_INJECT(CommuHiLo_PickClose);
    BOF3_INJECT(CommuHiLo_Reveal);
    BOF3_INJECT(CommuHiLo_RevealPause);
    BOF3_INJECT(CommuHiLo_Payout);
    BOF3_INJECT(CommuHiLo_CheckWinnings);
    BOF3_INJECT(CommuHiLo_CashOut);
    BOF3_INJECT(CommuHiLo_Restart);
    BOF3_INJECT(CommuHiLo_Leave);
    BOF3_INJECT(CommuHiLo_Close);
    BOF3_INJECT(CommuHiLo_DrawMarker);
    BOF3_INJECT(CommuHiLo_DrawRow);
    BOF3_INJECT(CommuHiLo_Shuffle);
    BOF3_INJECT(Commu_RandDigit);
    BOF3_INJECT(CommuHiLo_DrawChoices);
    BOF3_INJECT(CommuHiLo_DrawChoice);
    BOF3_INJECT(Commu_DrawCard);
    BOF3_INJECT(CommuHiLo_Quit);
    BOF3_INJECT(Commu_DrawUnderline);
    BOF3_INJECT(Commu_DrawPiece);
    BOF3_INJECT(Commu_DrawZennyBox);
    BOF3_INJECT(Commu_DrawFrame);
    BOF3_INJECT(Commu_DrawStakeBox);
    BOF3_INJECT(CommuHitBlow_Run);
    BOF3_INJECT(CommuHitBlow_OpenDispatch);
    BOF3_INJECT(CommuHitBlow_OpenFade);
    BOF3_INJECT(CommuHitBlow_OpenMusic);
    BOF3_INJECT(CommuHitBlow_OpenWait);
    BOF3_INJECT(CommuHitBlow_OpenSlide);
    BOF3_INJECT(CommuHitBlow_PlayDispatch);
    BOF3_INJECT(CommuHitBlow_StartDispatch);
    BOF3_INJECT(CommuHitBlow_Start);
    BOF3_INJECT(CommuHitBlow_StartSlide);
    BOF3_INJECT(CommuHitBlow_Prompt);
    BOF3_INJECT(CommuHitBlow_WaitPrompt);
    BOF3_INJECT(CommuHitBlow_Input);
    BOF3_INJECT(CommuHitBlow_Score);
    BOF3_INJECT(CommuHitBlow_NextGuess);
    BOF3_INJECT(CommuHitBlow_Lost);
    BOF3_INJECT(CommuHitBlow_Prize);
    BOF3_INJECT(CommuHitBlow_End);
    BOF3_INJECT(CommuHitBlow_Close);
}

void CommuBoardFromRest4d_Inject() {
    BOF3_INJECT(CommuBoard_DrawRows);
    BOF3_INJECT(CommuBoard_DrawFrame);
    BOF3_INJECT(CommuBoard_DrawRowCells);
    BOF3_INJECT(CommuBoard_DrawCells);
}
