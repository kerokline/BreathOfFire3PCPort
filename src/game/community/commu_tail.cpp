// Moved here by tools/move_funcs.py from rest_4b.cpp (docs/source-layout-round.md).
#include "game/community/commu_tail.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/faerie_name.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/rest_4b.h"
#include "game/rest_4b_callees.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4b::at;

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

void PutW(unsigned char* p, unsigned v) { SetWord(p, v); }

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

// The field tail's state byte, moved by one as the original's inc / dec do.
void StateBy(int d) { AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + d); }

// The 16 bytes of an item's record copied into Text_Records' first four dwords.
void CopyName(const unsigned char* name) {
    PutL(::bof3::Img(at::kTextRecords.v), static_cast<U>(Long(name)));
    PutL(::bof3::Img(at::kTextRecords.v) + 4, static_cast<U>(Long(name + 4)));
    PutL(::bof3::Img(at::kTextRecords.v) + 8, static_cast<U>(Long(name + 8)));
    PutL(::bof3::Img(at::kTextRecords.v) + 0xC, static_cast<U>(Long(name + 0xC)));
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// original 0x456D50: Field_ModeTailKinds[14]: jmp [CommuTail14_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 11).
extern "C" void __cdecl CommuTail14_Dispatch(void) {
    Jump(::bof3::Img(CommuTail14_States_addr), 11, AreaW0c_TailState, "CommuTail14_Dispatch");
}

// original 0x456D60: CommuTail14_States[0]: while a message is open
// (Field_Request 2) nothing; once the message box shows message 0xF9 the next
// state; else message 0xF8 opened and Field_Request 2.
extern "C" void __cdecl CommuTail14_OpenF8(void) {
    if (Field_Request == 2) return;
    if (W(::bof3::Img(at::kMessageIndex.v)) == 0xF9) {
        StateBy(1);
        return;
    }
    SH_CALL(Msg_OpenScript)(0xF8);
    Field_Request = 2;
}

// original 0x456D90: CommuTail14_States[1]: LoadDatFile(0x12B), the next state.
extern "C" void __cdecl CommuTail14_LoadDat(void) {
    SH_CALL(LoadDatFile)(0x12B);
    StateBy(1);
}

// original 0x456DB0: CommuTail14_States[2] (and CommuTail23_States[1], [10],
// [17], CommuTail26_States[1]): once File_LoadDone answers, the CLUT strip put
// back (Gfx_ClutStripRestore, Gfx_ClutStripDirty 1), the board's six bytes
// 0x939A3C..0x939A41 cleared, the next state.
extern "C" void __cdecl CommuTail_WaitLoad(void) {
    if (SH_CALL(File_LoadDone)() == 0) return;
    SH_CALL(Gfx_ClutStripRestore)();
    Gfx_ClutStripDirty = 1;
    B(::bof3::Img(at::kBoardTrack.v) + 1) = 0;   // 0x939A3E, CommuBoard_States' index
    B(::bof3::Img(at::kBoardTrack.v) + 3) = 0;   // 0x939A40
    B(::bof3::Img(at::kBoardTrack.v) + 2) = 0;   // 0x939A3F
    B(::bof3::Img(at::kBoardTrack.v) + 4) = 0;   // 0x939A41
    Rest4b_BoardFlag = 0;         // 0x939A3C
    Rest4d_Again = 0;             // 0x939A3D
    StateBy(1);
}

// original 0x456DF0: CommuTail14_States[4] (and CommuTail23_States[3], [12],
// [19], CommuTail26_States[3]): bit 7 of 0x90412C set, the sound bank its low
// seven bits name + 0x2C2 loaded (Snd_LoadBankFile), the next state.
extern "C" void __cdecl CommuTail_LoadSoundBank(void) {
    const unsigned char bits = static_cast<unsigned char>(AreaW0b_PartySet | 0x80);
    AreaW0b_PartySet = bits;
    SH_CALL(Snd_LoadBankFile)((bits & 0x7Fu) + 0x2C2);
    StateBy(1);
}

// original 0x456E20: CommuTail14_States[5]: once File_LoadDone answers, the tail
// disarmed (Area131_DisarmTail), the kept object's facing put back
// (CommuTail_RestoreFacing), then R4A's 0x456080 and a tail jump to R4A's
// 0x455950.
extern "C" void __cdecl CommuTail14_End(void) {
    if (SH_CALL(File_LoadDone)() == 0) return;
    SH_CALL(Area131_DisarmTail)();
    SH_CALL(CommuTail_RestoreFacing)();
    SH_AT(void (__cdecl*)(), at::kR4ACountSlots.v)();
    SH_AT(void (__cdecl*)(), at::kR4ASettle.v)();
}

// original 0x456E40: Sprite_Current = the object 0x939A38 holds; its +8 (the
// facing) = its +0x85, and Sprite_FaceDirection(+8 read back).
extern "C" void __cdecl CommuTail_RestoreFacing(void) {
    const U object = Rest4d_Pointer;
    PutL(::bof3::Img(at::kSpriteCurrent.v), object);
    unsigned char* const o = At(object);
    o[8] = o[0x85];
    const unsigned char facing = Sprite_Current[8];
    SH_CALL(Sprite_FaceDirection)(facing);
}

// original 0x456E70: Field_ModeTailKinds[21]: jmp [CommuTail21_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 5).
extern "C" void __cdecl CommuTail21_Dispatch(void) {
    Jump(::bof3::Img(CommuTail21_States_addr), 5, AreaW0c_TailState, "CommuTail21_Dispatch");
}

// original 0x456E80: CommuTail14_States[6], CommuTail21_States[0]: the time
// since the record 0x9039F5's +4 (the clock 0x904134 less it, unsigned) looked
// up in a stack table of 20 rows {u16 bound, item, category}: the last row
// whose bound the time reaches. None: message 0x98. Else the item's name into
// Text_Records, message 0x96, Inventory_Add(category, item, 1); added, the
// record's +4 = the clock and the next state; not added, the state after it.
// Field_Request 2 either way.
extern "C" void __cdecl CommuTail_TimedGift(void) {
    struct Row { unsigned short bound; unsigned char item, category; };
    static const Row kRows[20] = {
        {1, 3, 0},       {2, 0xA, 0},     {4, 4, 0},       {7, 0x1E, 0},    {8, 4, 0},
        {0xB, 0xD, 0},   {0x10, 5, 0},    {0x15, 0x15, 0}, {0x1A, 0x16, 0}, {0x1F, 0x19, 0},
        {0x29, 0x1A, 0}, {0x33, 0x1B, 0}, {0x3D, 0x1C, 0}, {0x47, 0x1D, 0}, {0x4D, 0x1B, 3},
        {0x4E, 0x1D, 0}, {0x51, 0x1E, 0}, {0x5B, 8, 0},    {0xC9, 0x19, 3}, {0x1F4, 0x1A, 3},
    };
    const unsigned arg = AreaW0c_TailSub;
    const U elapsed = Game_BattleCount - L(::bof3::Img(at::kMembers.v) + 4 + 8 * arg);
    int k = 0;
    while (k < 20 && elapsed >= kRows[k].bound) ++k;
    --k;
    if (k < 0) {
        SH_CALL(Msg_OpenScript)(0x98);
    } else {
        const unsigned char item = kRows[k].item, category = kRows[k].category;
        const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
        CopyName(name);
        SH_CALL(Msg_OpenScript)(0x96);
        const unsigned char added = SH_CALL(Inventory_Add)(category, item, 1);
        if (added == 0) {
            StateBy(2);
            Field_Request = 2;
            return;
        }
        const U clock = Game_BattleCount;
        PutL(Member(AreaW0c_TailSub, "CommuTail_TimedGift") + 4, clock);
    }
    StateBy(1);
    Field_Request = 2;
}

// original 0x4570C0: CommuTail14_States[7], [10], CommuTail23_States[15]: once
// no message is open (Field_Request 0), the tail disarmed and a tail jump to
// CommuTail_RestoreFacing.
extern "C" void __cdecl CommuTail_EndAfterMessage(void) {
    if (Field_Request != 0) return;
    SH_CALL(Area131_DisarmTail)();
    SH_CALL(CommuTail_RestoreFacing)();
}

// original 0x4570E0: CommuTail14_States[8]: once no message is open, message
// 0x97, the state before, Field_Request 2.
extern "C" void __cdecl CommuTail_Open97(void) {
    if (Field_Request != 0) return;
    SH_CALL(Msg_OpenScript)(0x97);
    StateBy(-1);
    Field_Request = 2;
}

// original 0x457110: Field_ModeTailKinds[22]: jmp [CommuTail22_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 2).
extern "C" void __cdecl CommuTail22_Dispatch(void) {
    Jump(::bof3::Img(CommuTail22_States_addr), 2, AreaW0c_TailState, "CommuTail22_Dispatch");
}

// original 0x457120 (PSX 0x801F1BB8): CommuTail14_States[9], CommuTail22_States[0]: by the
// record 0x9039F5's +2. 3: message 0x52, the record's +4 = the clock, +2 = 0,
// the kept object's word +0x88 = 0x51. 2: a gift drawn - Rand & 0x7F (at most
// 100) less the first byte of the row of a stack table of three {first, two
// steps, unused} that the slot of the record's +1 names by its +1 (a level),
// then counted down the row's two steps to a tier 1..3; Rand & 0xF picks one
// of the tier's 16 pairs {item, category} at 0x652A10. Inventory_Add(category,
// item, 1): not added, message 0x50; added, the name into Text_Records,
// message 0x4F and the record kept as for 3. Then (any +2) the next state,
// Field_Request 2. A level past the table's three reads the frame above it
// (the return address); ours aborts (section 5).
extern "C" void __cdecl CommuTail_RandomGift(void) {
    static const unsigned char kTiers[3][4] = {{0x41, 0x1E, 5, 0x64}, {0x37, 0x19, 0x14, 0x64}, {0x19, 0x1E, 0x1E, 0xF}};
    const unsigned arg = AreaW0c_TailSub;
    const unsigned char mark = B(::bof3::Img(at::kMembers.v) + 2 + 8 * arg);
    if (mark == 3) {
        SH_CALL(Msg_OpenScript)(0x52);
        const unsigned a = AreaW0c_TailSub;
        PutL(Member(a, "CommuTail_RandomGift") + 4, Game_BattleCount);
        B(Member(AreaW0c_TailSub, "CommuTail_RandomGift") + 2) = 0;
        PutW(Rest4d_Pointer + 0x88, 0x51);
    } else if (mark == 2) {
        unsigned char r = static_cast<unsigned char>(SH_CALL(Rand)() & 0x7F);
        if (r > 0x64) r = 0x64;
        const unsigned char slot = B(::bof3::Img(at::kMembers.v) + 1 + 8 * AreaW0c_TailSub);
        const unsigned level = B(Slot(slot) + 1);
        if (level >= 3)
            bof3::Fatal("CommuTail_RandomGift: slot %u's level %u is past the three rows of its stack table; the original "
                        "reads the frame above it",
                        static_cast<unsigned>(slot), level);
        const unsigned char* const row = kTiers[level];
        r = r >= row[0] ? static_cast<unsigned char>(r - row[0]) : 0;
        int tier = 1;
        for (const unsigned char* step = row + 1; tier < 3; ++step) {
            if (r <= *step) break;
            r = static_cast<unsigned char>(r - *step);
            ++tier;
        }
        const unsigned pick = static_cast<unsigned>(SH_CALL(Rand)() & 0xF);
        const U pair = ::bof3::Img(at::kGiftItems.v) + 2 * (pick + static_cast<U>(tier - 1) * 16);
        const unsigned char item = B(pair), category = B(pair + 1);
        const unsigned char added = SH_CALL(Inventory_Add)(category, item, 1);
        if (added == 0) {
            SH_CALL(Msg_OpenScript)(0x50);
        } else {
            const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
            CopyName(name);
            SH_CALL(Msg_OpenScript)(0x4F);
            const U clock = Game_BattleCount;
            PutL(Member(AreaW0c_TailSub, "CommuTail_RandomGift") + 4, clock);
            B(Member(AreaW0c_TailSub, "CommuTail_RandomGift") + 2) = 0;
            PutW(Rest4d_Pointer + 0x88, 0x51);
        }
    }
    StateBy(1);
    Field_Request = 2;
}

// original 0x4572F0: Field_ModeTailKinds[23]: jmp [CommuTail23_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 21).
extern "C" void __cdecl CommuTail23_Dispatch(void) {
    Jump(::bof3::Img(CommuTail23_States_addr), 21, AreaW0c_TailState, "CommuTail23_Dispatch");
}

// original 0x457300: CommuTail23_States[0]: Field_ScriptFlags2's low byte |= 7,
// LoadDatFile(0x12D when 0x9039F5 is 3, else 0x12C), the next state.
extern "C" void __cdecl CommuTail23_LoadDat(void) {
    const unsigned char arg = AreaW0c_TailSub;
    B(::bof3::Img(at::kScriptFlags2.v)) |= 7;
    SH_CALL(LoadDatFile)(arg == 3 ? 0x12D : 0x12C);
    StateBy(1);
}

// original 0x457340: CommuTail23_States[2]: jmp [CommuTail_GameStates + 4 * the
// u8 0x9039F5], unchecked (ours aborts past 16).
extern "C" void __cdecl CommuTail_GameDispatch(void) {
    Jump(::bof3::Img(CommuTail_GameStates_addr), 16, AreaW0c_TailSub, "CommuTail_GameDispatch");
}

// original 0x457350: CommuTail23_States[4], [13], [20]: once File_LoadDone
// answers, Field_ScriptFlags2 &= 0xFFF8, the tail disarmed and a tail jump to
// CommuTail_RestoreFacing.
extern "C" void __cdecl CommuTail_EndAfterLoad(void) {
    if (SH_CALL(File_LoadDone)() == 0) return;
    Field_ScriptFlags2 = static_cast<std::uint32_t>(Field_ScriptFlags2) & 0xFFF8;
    SH_CALL(Area131_DisarmTail)();
    SH_CALL(CommuTail_RestoreFacing)();
}

// original 0x457370: Field_ModeTailKinds[24]: jmp [CommuTail24_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 12).
extern "C" void __cdecl CommuTail24_Dispatch(void) {
    Jump(::bof3::Img(CommuTail24_States_addr), 12, AreaW0c_TailState, "CommuTail24_Dispatch");
}

// original 0x457380: CommuTail23_States[9]: Field_ScriptFlags2 |= 7,
// LoadDatFile(0x12E), the next state.
extern "C" void __cdecl CommuTail24_LoadDat(void) {
    B(::bof3::Img(at::kScriptFlags2.v)) |= 7;
    SH_CALL(LoadDatFile)(0x12E);
    StateBy(1);
}

// original 0x4573B0: Field_ModeTailKinds[25]: jmp [CommuTail25_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 7).
extern "C" void __cdecl CommuTail25_Dispatch(void) {
    Jump(::bof3::Img(CommuTail25_States_addr), 7, AreaW0c_TailState, "CommuTail25_Dispatch");
}

// original 0x4573C0: CommuTail23_States[14]: the record 0x9039F5's +3 (high
// nibble n, low nibble the category) and +2 (the item); the 6-byte row n of
// 0x652AC4 {message, message, count} (read in place for any nibble). A count
// of 0: n 0 - message (row's first), state 2; else message (row's first), the
// next state. Otherwise the name into Text_Records and Inventory_Add(category,
// item, count): added, the record's +2 and +3 cleared and the row's first
// message, else its second; the next state. Field_Request 2.
extern "C" void __cdecl CommuTail_NibbleGift(void) {
    const unsigned arg = AreaW0c_TailSub;
    const unsigned char bits = B(::bof3::Img(at::kMembers.v) + 3 + 8 * arg);
    const unsigned n = bits >> 4;
    const U row = ::bof3::Img(at::kGiftLines.v) + 6 * n;
    unsigned message;
    if (W(row + 4) == 0) {
        message = W(row);
        if (n == 0) {
            AreaW0c_TailState = 2;
            SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(message));
            Field_Request = 2;
            return;
        }
    } else {
        const unsigned char* const name = SH_CALL(Item_NamePtr)(bits & 0xFu, B(::bof3::Img(at::kMembers.v) + 2 + 8 * arg));
        CopyName(name);
        const unsigned a = AreaW0c_TailSub;
        const unsigned char item = B(::bof3::Img(at::kMembers.v) + 2 + 8 * a);
        const unsigned char category = static_cast<unsigned char>(B(::bof3::Img(at::kMembers.v) + 3 + 8 * a) & 0xF);
        const unsigned char added = SH_CALL(Inventory_Add)(category, item, B(row + 4));
        if (added != 0) {
            message = W(row);
            B(Member(AreaW0c_TailSub, "CommuTail_NibbleGift") + 2) = 0;
            B(Member(AreaW0c_TailSub, "CommuTail_NibbleGift") + 3) = 0;
        } else {
            message = W(row + 2);
        }
    }
    StateBy(1);
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(message));
    Field_Request = 2;
}

// original 0x4574E0: CommuTail23_States[16]: while a message is open nothing;
// else Field_ScriptFlags2 |= 7, LoadDatFile(0x12F), the next state.
extern "C" void __cdecl CommuTail25_LoadDat(void) {
    if (Field_Request == 2) return;
    B(::bof3::Img(at::kScriptFlags2.v)) |= 7;
    SH_CALL(LoadDatFile)(0x12F);
    StateBy(1);
}

// original 0x457510: Field_ModeTailKinds[26]: jmp [CommuTail26_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 5).
extern "C" void __cdecl CommuTail26_Dispatch(void) {
    Jump(::bof3::Img(CommuTail26_States_addr), 5, AreaW0c_TailState, "CommuTail26_Dispatch");
}

// original 0x457520: CommuTail26_States[0]: with Field_ScriptFlags, 0x904A90
// and 0x937F80 all 0, the tail kind becomes 6 and nothing else; otherwise
// Field_ScriptFlags2 |= 7, LoadDatFile(0x130), the next state.
extern "C" void __cdecl CommuTail26_LoadDat(void) {
    if (Rest4a_RemovedCount == 0 && Rest4b_BusyA == 0 && Rest4b_BusyB == 0) {
        AreaW0a_TailKind = 6;
        return;
    }
    B(::bof3::Img(at::kScriptFlags2.v)) |= 7;
    SH_CALL(LoadDatFile)(0x130);
    StateBy(1);
}

// original 0x457570: CommuTail26_States[4]: once File_LoadDone answers,
// Field_ScriptFlags2 &= 0xFFF8 and a tail jump to Area131_DisarmTail.
extern "C" void __cdecl CommuTail26_End(void) {
    if (SH_CALL(File_LoadDone)() == 0) return;
    Field_ScriptFlags2 = static_cast<std::uint32_t>(Field_ScriptFlags2) & 0xFFF8;
    SH_CALL(Area131_DisarmTail)();
}

// original 0x457590: Field_ModeTailKinds[60]: a switch on the s8 0x9039F4. 0:
// the music track kept in 0x939A3D, Music_FadeOutStop(10), the stream 8 (0x939A3C
// 0) or 0xA loaded, the next state. 1: once Sound_StreamDone answers, message
// 0x2A1, the next state, Field_Request 2. 2: once no message is open (any
// Field_Request but 2), Music_Play(the kept track, 8), Field_ScriptFlags2 &=
// 0xFFF8, the tail disarmed, a tail jump to CommuTail_RestoreFacing. Any other
// state: nothing.
extern "C" void __cdecl CommuTail60_Stream(void) {
    switch (AreaW0c_TailState) {
    case 0: {
        Rest4d_Again = Music_Track;
        SH_CALL(Music_FadeOutStop)(10);
        SH_CALL(Sound_LoadStream)(Rest4b_BoardFlag == 0 ? 8u : 0xAu);
        StateBy(1);
        return;
    }
    case 1:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Msg_OpenScript)(0x2A1);
        StateBy(1);
        Field_Request = 2;
        return;
    case 2:
        if (Field_Request == 2) return;
        SH_CALL(Music_Play)(Rest4d_Again, 8);
        Field_ScriptFlags2 = static_cast<std::uint32_t>(Field_ScriptFlags2) & 0xFFF8;
        SH_CALL(Area131_DisarmTail)();
        SH_CALL(CommuTail_RestoreFacing)();
        return;
    default:
        return;
    }
}

#pragma clang attribute pop

void CommuTail_Inject() {
    BOF3_INJECT(CommuTail14_Dispatch);
    BOF3_INJECT(CommuTail14_OpenF8);
    BOF3_INJECT(CommuTail14_LoadDat);
    BOF3_INJECT(CommuTail_WaitLoad);
    BOF3_INJECT(CommuTail_LoadSoundBank);
    BOF3_INJECT(CommuTail14_End);
    BOF3_INJECT(CommuTail_RestoreFacing);
    BOF3_INJECT(CommuTail21_Dispatch);
    BOF3_INJECT(CommuTail_TimedGift);
    BOF3_INJECT(CommuTail_EndAfterMessage);
    BOF3_INJECT(CommuTail_Open97);
    BOF3_INJECT(CommuTail22_Dispatch);
    BOF3_INJECT(CommuTail_RandomGift);
    BOF3_INJECT(CommuTail23_Dispatch);
    BOF3_INJECT(CommuTail23_LoadDat);
    BOF3_INJECT(CommuTail_GameDispatch);
    BOF3_INJECT(CommuTail_EndAfterLoad);
    BOF3_INJECT(CommuTail24_Dispatch);
    BOF3_INJECT(CommuTail24_LoadDat);
    BOF3_INJECT(CommuTail25_Dispatch);
    BOF3_INJECT(CommuTail_NibbleGift);
    BOF3_INJECT(CommuTail25_LoadDat);
    BOF3_INJECT(CommuTail26_Dispatch);
    BOF3_INJECT(CommuTail26_LoadDat);
    BOF3_INJECT(CommuTail26_End);
    BOF3_INJECT(CommuTail60_Stream);
}
