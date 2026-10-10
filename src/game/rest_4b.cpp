// The community band's tail kinds and its board - round fourteen, wave four,
// group R4B: the 60 functions 0x456D50..0x459EDA of the cut
// (analysis/round14_cut.tsv), each read with capstone to its last instruction
// (docs/rest_4b.md section 1). Field_ModeTailKinds 14, 21..26 and 60 and the
// states their tables hold; the board those states run (CommuBoard_States, by
// 0x939A3E): a grid cursor over eight slots and three more, the 60 community
// records placed in them, the lists a slot's kind is picked from, and the
// panel, cards, bars and sprites it draws.
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a table's handler is called by the address the table holds. No divergence:
// each is a faithful replacement. Where the original jumps through a table by
// an index past the table's own count, writes a record or a slot past its
// table, or reads a stack table past its end, ours aborts with a message
// (docs/rest_4b.md section 5, one policy for the group); reads of the .data
// tables by an index the code computes read the image in place, as the
// original does.
#include "game/rest_4b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/rest_4b_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4b::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return *At(address); }
signed char SB(U address) { return static_cast<signed char>(*At(address)); }
unsigned W(U address) { return Word(At(address)); }
U L(U address) { return static_cast<U>(Long(At(address))); }
void PutW(U address, unsigned v) { SetWord(At(address), v); }
void PutL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }
short S16(U address) { return static_cast<short>(Word(At(address))); }
void PutF(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }
void PutFBits(unsigned char* p, U bits) { std::memcpy(p, &bits, sizeof bits); }
void PutW(unsigned char* p, unsigned v) { SetWord(p, v); }
unsigned char* Packet() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(L(at::kPacketNext))); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

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
                    static_cast<unsigned>(table), static_cast<unsigned>(table + 4 * static_cast<U>(index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * static_cast<U>(index))))();
}

// The community record k (0x9046D0 + 8k): ours writes only the 60; past them
// the original writes on into the save block (section 5).
U Member(unsigned k, const char* who) {
    if (k >= 60)
        bof3::Fatal("%s: community record %u is past the 60 at 0x9046D0; the original writes at 0x%X", who, k,
                    static_cast<unsigned>(at::kMembers + 8 * k));
    return at::kMembers + 8 * k;
}

// A slot's bytes (0x9048A8 + 8 * slot, the slots 1..8 at 0x9048B0): read in
// place for any slot byte, as the original reads them.
U Slot(unsigned slot) { return at::kSlots + 8 * slot; }
// ... and written only for the eight (section 5).
U SlotToWrite(unsigned slot, const char* who) {
    if (slot - 1 >= 8)
        bof3::Fatal("%s: slot %u is outside the eight at 0x9048B0; the original writes at 0x%X", who, slot,
                    static_cast<unsigned>(at::kSlots + 8 * slot));
    return at::kSlots + 8 * slot;
}

// The field tail's state byte, moved by one as the original's inc / dec do.
void StateBy(int d) { B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + d); }

// The flashing intensity three draws share: from Frame_Counter's bits 1..2,
// counted up while bit 3 is set and down while it is clear, +0x3F.
unsigned char Flash() {
    unsigned char f = B(at::kFrame);
    if (!(f & 8)) f = static_cast<unsigned char>(~f);
    return static_cast<unsigned char>(((f & 6) << 5) + 0x3F);
}

// A slot's column offset: (slot - 1) % 4 as the original's signed remainder
// (slot 0 gives -1), times 52.
int Column(unsigned char slot) { return ((static_cast<int>(slot) - 1) % 4) * 52; }

// The 16 bytes of an item's record copied into Text_Records' first four dwords.
void CopyName(const unsigned char* name) {
    PutL(at::kTextRecords, static_cast<U>(Long(name)));
    PutL(at::kTextRecords + 4, static_cast<U>(Long(name + 4)));
    PutL(at::kTextRecords + 8, static_cast<U>(Long(name + 8)));
    PutL(at::kTextRecords + 0xC, static_cast<U>(Long(name + 0xC)));
}

void R4EHand(int x, int y, unsigned flash) {
    SH_AT(void (__cdecl*)(int, int, unsigned), at::kR4EHand)(x, y, flash);
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ===========================================================================
// The tail kinds (Field_ModeTailKinds 14, 21..26, 60) and their states
// ===========================================================================

// original 0x456D50: Field_ModeTailKinds[14]: jmp [CommuTail14_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 11).
extern "C" void __cdecl CommuTail14_Dispatch(void) {
    Jump(at::kTail14States, 11, SB(at::kTailState), "CommuTail14_Dispatch");
}

// original 0x456D60: CommuTail14_States[0]: while a message is open
// (Field_Request 2) nothing; once the message box shows message 0xF9 the next
// state; else message 0xF8 opened and Field_Request 2.
extern "C" void __cdecl CommuTail14_OpenF8(void) {
    if (B(at::kRequest) == 2) return;
    if (W(at::kMessageIndex) == 0xF9) {
        StateBy(1);
        return;
    }
    SH_CALL(Msg_OpenScript)(0xF8);
    B(at::kRequest) = 2;
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
    B(at::kClutDirty) = 1;
    B(at::kBoardTrack + 1) = 0;   // 0x939A3E, CommuBoard_States' index
    B(at::kBoardTrack + 3) = 0;   // 0x939A40
    B(at::kBoardTrack + 2) = 0;   // 0x939A3F
    B(at::kBoardTrack + 4) = 0;   // 0x939A41
    B(at::kBoardFlag) = 0;        // 0x939A3C
    B(at::kBoardTrack) = 0;       // 0x939A3D
    StateBy(1);
}

// original 0x456DF0: CommuTail14_States[4] (and CommuTail23_States[3], [12],
// [19], CommuTail26_States[3]): bit 7 of 0x90412C set, the sound bank its low
// seven bits name + 0x2C2 loaded (Snd_LoadBankFile), the next state.
extern "C" void __cdecl CommuTail_LoadSoundBank(void) {
    const unsigned char bits = static_cast<unsigned char>(B(at::kSoundBankBits) | 0x80);
    B(at::kSoundBankBits) = bits;
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
    SH_AT(void (__cdecl*)(), at::kR4ACountSlots)();
    SH_AT(void (__cdecl*)(), at::kR4ASettle)();
}

// original 0x456E40: Sprite_Current = the object 0x939A38 holds; its +8 (the
// facing) = its +0x85, and Sprite_FaceDirection(+8 read back).
extern "C" void __cdecl CommuTail_RestoreFacing(void) {
    const U object = L(at::kTailObject);
    PutL(at::kSpriteCurrent, object);
    unsigned char* const o = At(object);
    o[8] = o[0x85];
    const unsigned char facing = At(L(at::kSpriteCurrent))[8];
    SH_CALL(Sprite_FaceDirection)(facing);
}

// original 0x456E70: Field_ModeTailKinds[21]: jmp [CommuTail21_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 5).
extern "C" void __cdecl CommuTail21_Dispatch(void) {
    Jump(at::kTail21States, 5, SB(at::kTailState), "CommuTail21_Dispatch");
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
    const unsigned arg = B(at::kTailArg);
    const U elapsed = L(at::kClock) - L(at::kMembers + 4 + 8 * arg);
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
            B(at::kRequest) = 2;
            return;
        }
        const U clock = L(at::kClock);
        PutL(Member(B(at::kTailArg), "CommuTail_TimedGift") + 4, clock);
    }
    StateBy(1);
    B(at::kRequest) = 2;
}

// original 0x4570C0: CommuTail14_States[7], [10], CommuTail23_States[15]: once
// no message is open (Field_Request 0), the tail disarmed and a tail jump to
// CommuTail_RestoreFacing.
extern "C" void __cdecl CommuTail_EndAfterMessage(void) {
    if (B(at::kRequest) != 0) return;
    SH_CALL(Area131_DisarmTail)();
    SH_CALL(CommuTail_RestoreFacing)();
}

// original 0x4570E0: CommuTail14_States[8]: once no message is open, message
// 0x97, the state before, Field_Request 2.
extern "C" void __cdecl CommuTail_Open97(void) {
    if (B(at::kRequest) != 0) return;
    SH_CALL(Msg_OpenScript)(0x97);
    StateBy(-1);
    B(at::kRequest) = 2;
}

// original 0x457110: Field_ModeTailKinds[22]: jmp [CommuTail22_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 2).
extern "C" void __cdecl CommuTail22_Dispatch(void) {
    Jump(at::kTail22States, 2, SB(at::kTailState), "CommuTail22_Dispatch");
}

// original 0x457120: CommuTail14_States[9], CommuTail22_States[0]: by the
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
    const unsigned arg = B(at::kTailArg);
    const unsigned char mark = B(at::kMembers + 2 + 8 * arg);
    if (mark == 3) {
        SH_CALL(Msg_OpenScript)(0x52);
        const unsigned a = B(at::kTailArg);
        PutL(Member(a, "CommuTail_RandomGift") + 4, L(at::kClock));
        B(Member(B(at::kTailArg), "CommuTail_RandomGift") + 2) = 0;
        PutW(L(at::kTailObject) + 0x88, 0x51);
    } else if (mark == 2) {
        unsigned char r = static_cast<unsigned char>(SH_CALL(Rand)() & 0x7F);
        if (r > 0x64) r = 0x64;
        const unsigned char slot = B(at::kMembers + 1 + 8 * B(at::kTailArg));
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
        const U pair = at::kGiftItems + 2 * (pick + static_cast<U>(tier - 1) * 16);
        const unsigned char item = B(pair), category = B(pair + 1);
        const unsigned char added = SH_CALL(Inventory_Add)(category, item, 1);
        if (added == 0) {
            SH_CALL(Msg_OpenScript)(0x50);
        } else {
            const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
            CopyName(name);
            SH_CALL(Msg_OpenScript)(0x4F);
            const U clock = L(at::kClock);
            PutL(Member(B(at::kTailArg), "CommuTail_RandomGift") + 4, clock);
            B(Member(B(at::kTailArg), "CommuTail_RandomGift") + 2) = 0;
            PutW(L(at::kTailObject) + 0x88, 0x51);
        }
    }
    StateBy(1);
    B(at::kRequest) = 2;
}

// original 0x4572F0: Field_ModeTailKinds[23]: jmp [CommuTail23_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 21).
extern "C" void __cdecl CommuTail23_Dispatch(void) {
    Jump(at::kTail23States, 21, SB(at::kTailState), "CommuTail23_Dispatch");
}

// original 0x457300: CommuTail23_States[0]: Field_ScriptFlags2's low byte |= 7,
// LoadDatFile(0x12D when 0x9039F5 is 3, else 0x12C), the next state.
extern "C" void __cdecl CommuTail23_LoadDat(void) {
    const unsigned char arg = B(at::kTailArg);
    B(at::kScriptFlags2) |= 7;
    SH_CALL(LoadDatFile)(arg == 3 ? 0x12D : 0x12C);
    StateBy(1);
}

// original 0x457340: CommuTail23_States[2]: jmp [CommuTail_GameStates + 4 * the
// u8 0x9039F5], unchecked (ours aborts past 16).
extern "C" void __cdecl CommuTail_GameDispatch(void) {
    Jump(at::kTailGames, 16, B(at::kTailArg), "CommuTail_GameDispatch");
}

// original 0x457350: CommuTail23_States[4], [13], [20]: once File_LoadDone
// answers, Field_ScriptFlags2 &= 0xFFF8, the tail disarmed and a tail jump to
// CommuTail_RestoreFacing.
extern "C" void __cdecl CommuTail_EndAfterLoad(void) {
    if (SH_CALL(File_LoadDone)() == 0) return;
    PutW(at::kScriptFlags2, W(at::kScriptFlags2) & 0xFFF8);
    SH_CALL(Area131_DisarmTail)();
    SH_CALL(CommuTail_RestoreFacing)();
}

// original 0x457370: Field_ModeTailKinds[24]: jmp [CommuTail24_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 12).
extern "C" void __cdecl CommuTail24_Dispatch(void) {
    Jump(at::kTail24States, 12, SB(at::kTailState), "CommuTail24_Dispatch");
}

// original 0x457380: CommuTail23_States[9]: Field_ScriptFlags2 |= 7,
// LoadDatFile(0x12E), the next state.
extern "C" void __cdecl CommuTail24_LoadDat(void) {
    B(at::kScriptFlags2) |= 7;
    SH_CALL(LoadDatFile)(0x12E);
    StateBy(1);
}

// original 0x4573B0: Field_ModeTailKinds[25]: jmp [CommuTail25_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 7).
extern "C" void __cdecl CommuTail25_Dispatch(void) {
    Jump(at::kTail25States, 7, SB(at::kTailState), "CommuTail25_Dispatch");
}

// original 0x4573C0: CommuTail23_States[14]: the record 0x9039F5's +3 (high
// nibble n, low nibble the category) and +2 (the item); the 6-byte row n of
// 0x652AC4 {message, message, count} (read in place for any nibble). A count
// of 0: n 0 - message (row's first), state 2; else message (row's first), the
// next state. Otherwise the name into Text_Records and Inventory_Add(category,
// item, count): added, the record's +2 and +3 cleared and the row's first
// message, else its second; the next state. Field_Request 2.
extern "C" void __cdecl CommuTail_NibbleGift(void) {
    const unsigned arg = B(at::kTailArg);
    const unsigned char bits = B(at::kMembers + 3 + 8 * arg);
    const unsigned n = bits >> 4;
    const U row = at::kGiftLines + 6 * n;
    unsigned message;
    if (W(row + 4) == 0) {
        message = W(row);
        if (n == 0) {
            B(at::kTailState) = 2;
            SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(message));
            B(at::kRequest) = 2;
            return;
        }
    } else {
        const unsigned char* const name = SH_CALL(Item_NamePtr)(bits & 0xFu, B(at::kMembers + 2 + 8 * arg));
        CopyName(name);
        const unsigned a = B(at::kTailArg);
        const unsigned char item = B(at::kMembers + 2 + 8 * a);
        const unsigned char category = static_cast<unsigned char>(B(at::kMembers + 3 + 8 * a) & 0xF);
        const unsigned char added = SH_CALL(Inventory_Add)(category, item, B(row + 4));
        if (added != 0) {
            message = W(row);
            B(Member(B(at::kTailArg), "CommuTail_NibbleGift") + 2) = 0;
            B(Member(B(at::kTailArg), "CommuTail_NibbleGift") + 3) = 0;
        } else {
            message = W(row + 2);
        }
    }
    StateBy(1);
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(message));
    B(at::kRequest) = 2;
}

// original 0x4574E0: CommuTail23_States[16]: while a message is open nothing;
// else Field_ScriptFlags2 |= 7, LoadDatFile(0x12F), the next state.
extern "C" void __cdecl CommuTail25_LoadDat(void) {
    if (B(at::kRequest) == 2) return;
    B(at::kScriptFlags2) |= 7;
    SH_CALL(LoadDatFile)(0x12F);
    StateBy(1);
}

// original 0x457510: Field_ModeTailKinds[26]: jmp [CommuTail26_States + 4 *
// the s8 0x9039F4], unchecked (ours aborts outside 5).
extern "C" void __cdecl CommuTail26_Dispatch(void) {
    Jump(at::kTail26States, 5, SB(at::kTailState), "CommuTail26_Dispatch");
}

// original 0x457520: CommuTail26_States[0]: with Field_ScriptFlags, 0x904A90
// and 0x937F80 all 0, the tail kind becomes 6 and nothing else; otherwise
// Field_ScriptFlags2 |= 7, LoadDatFile(0x130), the next state.
extern "C" void __cdecl CommuTail26_LoadDat(void) {
    if (B(at::kScriptFlags) == 0 && B(at::kBusyA) == 0 && B(at::kBusyB) == 0) {
        B(at::kTailKind) = 6;
        return;
    }
    B(at::kScriptFlags2) |= 7;
    SH_CALL(LoadDatFile)(0x130);
    StateBy(1);
}

// original 0x457570: CommuTail26_States[4]: once File_LoadDone answers,
// Field_ScriptFlags2 &= 0xFFF8 and a tail jump to Area131_DisarmTail.
extern "C" void __cdecl CommuTail26_End(void) {
    if (SH_CALL(File_LoadDone)() == 0) return;
    PutW(at::kScriptFlags2, W(at::kScriptFlags2) & 0xFFF8);
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
    switch (SB(at::kTailState)) {
    case 0: {
        B(at::kBoardTrack) = B(at::kMusicTrack);
        SH_CALL(Music_FadeOutStop)(10);
        SH_CALL(Sound_LoadStream)(B(at::kBoardFlag) == 0 ? 8u : 0xAu);
        StateBy(1);
        return;
    }
    case 1:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Msg_OpenScript)(0x2A1);
        StateBy(1);
        B(at::kRequest) = 2;
        return;
    case 2:
        if (B(at::kRequest) == 2) return;
        SH_CALL(Music_Play)(B(at::kBoardTrack), 8);
        PutW(at::kScriptFlags2, W(at::kScriptFlags2) & 0xFFF8);
        SH_CALL(Area131_DisarmTail)();
        SH_CALL(CommuTail_RestoreFacing)();
        return;
    default:
        return;
    }
}

// ===========================================================================
// The board (CommuBoard_States, by 0x939A3E)
// ===========================================================================

// original 0x457640: CommuTail14_States[3]: jmp [CommuBoard_States + 4 * the u8
// 0x939A3E], unchecked (ours aborts past 14).
extern "C" void __cdecl CommuBoard_Dispatch(void) {
    Jump(at::kBoardStates, 14, B(at::kBoardState), "CommuBoard_Dispatch");
}

// original 0x457650: CommuBoard_States[0]: the grid cursor at mode 0 with its
// bytes 0, 1; the help line 0xB5; the next board state.
extern "C" void __cdecl CommuBoard_Init(void) {
    B(at::kCursorB) = 1;
    B(at::kCursorA) = 0;
    B(at::kCursorMode) = 0;
    PutW(at::kHelp, 0xB5);
    B(at::kBoardState) = static_cast<unsigned char>(B(at::kBoardState) + 1);
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
    const unsigned pressed = W(at::kPressed);
    if (W(at::kCancel) & pressed) {
        PutW(at::kHelp, 0xFFFF);
        StateBy(1);
    } else if (W(at::kConfirm) & pressed) {
        B(at::kSavedB) = B(at::kCursorB);
        const unsigned char a = B(at::kCursorA);
        const unsigned char mode = B(at::kCursorMode);
        B(at::kSavedMode) = mode;
        B(at::kSavedA) = a;
        B(at::kBoardStep) = 0;
        B(at::kListCursor) = 0;
        int allowed = -1;
        if (mode == 0 || mode == 2) {
            const unsigned char count = SH_CALL(Commu_CountInSlot)(B(at::kTailArg));
            allowed = count != 0;
        } else if (mode == 1) {
            const unsigned char arg = B(at::kTailArg);
            allowed = arg <= B(at::kAreaLimits + W(at::kArea));
        }
        if (allowed == 1) {
            SH_CALL(Sound_PlayEffect)(0x104);
            SH_CALL(CommuBoard_SlotHelp)();
            B(at::kBoardState) = static_cast<unsigned char>(B(at::kBoardState) + 1);
        } else {
            SH_CALL(Sound_PlayEffect)(0x107);
        }
    } else {
        SH_CALL(CommuBoard_MoveGridCursor)(At(at::kTailArg));
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kTailArg), 0);
}

// original 0x457780: CommuBoard_States[2]: jmp [CommuBoard_ModeStates + 4 * the
// u8 0x675F86] (the kept cursor mode), unchecked (ours aborts past 11).
extern "C" void __cdecl CommuBoard_ModeDispatch(void) {
    Jump(at::kBoardModes, 11, B(at::kSavedMode), "CommuBoard_ModeDispatch");
}

// original 0x457790: CommuBoard_States[3], [5] (ModeStates[0], [2]): jmp
// [CommuBoard_Steps + 4 * the u8 0x939A40], unchecked (ours aborts past 8).
extern "C" void __cdecl CommuBoard_StepDispatch(void) {
    Jump(at::kBoardSteps, 8, B(at::kBoardStep), "CommuBoard_StepDispatch");
}

// original 0x457AD0: CommuBoard_States[4] (ModeStates[1]): jmp
// [CommuBoard_StepsB + 4 * the u8 0x939A40], unchecked (ours aborts past 6).
extern "C" void __cdecl CommuBoard_StepDispatchB(void) {
    Jump(at::kBoardStepsB, 6, B(at::kBoardStep), "CommuBoard_StepDispatchB");
}

namespace {

// The hand and the card at the list cursor of the slot 0x9039F5 (slot 0: a row
// at the panel's top; else a 5-wide grid), as the Steps states draw them: R4E's
// 0x45ECC0 and the card of the cursor's record - beside the hand when steady
// (flash 0), at (0xA0, 0x3E) when flashing.
void CursorAndCard(unsigned char slot, unsigned flash) {
    const int c = SB(at::kListCursor);
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
    const unsigned char cursor = B(at::kListCursor);
    const unsigned char arg = B(at::kTailArg);
    const unsigned idx = SH_CALL(Commu_NthInSlot)(arg, cursor);
    if (flash == 0)
        SH_CALL(CommuBoard_DrawRecordCard)(cx, cy, idx, 0);
    else
        SH_CALL(CommuBoard_DrawRecordCard)(0xA0, 0x3E, idx, 1);
}

}  // namespace

// original 0x4577A0: CommuBoard_Steps[0]: a record of the slot picked. Cancel:
// sound 0x106, the board's state back, help 0xB5. Confirm: sound 0x104, the
// picked slot 0x675F7F = 0x9039F5, the next step, help 0x44. Neither: the list
// cursor stepped over the slot's records by the auto-repeated keys (0x2000 up,
// back to 0 at the count; 0x8000 down, to count - 1 below 0; sound 0x100 when
// it moved). Then the panel, the slot's lines (steady), the hand and the card.
extern "C" void __cdecl CommuBoard_PickRecord(void) {
    const unsigned pressed = W(at::kPressed);
    if (W(at::kCancel) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kBoardState) = static_cast<unsigned char>(B(at::kBoardState) - 1);
        PutW(at::kHelp, 0xB5);
    } else if (W(at::kConfirm) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x104);
        B(at::kPick) = B(at::kTailArg);
        B(at::kBoardStep) = static_cast<unsigned char>(B(at::kBoardStep) + 1);
        PutW(at::kHelp, 0x44);
    } else {
        const unsigned char count = SH_CALL(Commu_CountInSlot)(B(at::kTailArg));
        const unsigned keys = SH_CALL(Input_AutoRepeat)(W(at::kPressed) & 0xA000);
        const unsigned char old = B(at::kListCursor);
        unsigned char c = old;
        if (keys & 0x2000) {
            ++c;
            B(at::kListCursor) = c;
            if (static_cast<signed char>(c) == static_cast<int>(count)) {
                c = 0;
                B(at::kListCursor) = c;
            }
        } else if (keys & 0x8000) {
            --c;
            B(at::kListCursor) = c;
            if (static_cast<signed char>(c) < 0) {
                c = static_cast<unsigned char>(count - 1);
                B(at::kListCursor) = c;
            }
        }
        if (static_cast<signed char>(c) != static_cast<int>(old)) SH_CALL(Sound_PlayEffect)(0x100);
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kTailArg), 1);
    CursorAndCard(B(at::kTailArg), 0);
}

// original 0x457990: CommuBoard_Steps[1]: the slot the record moves to. Cancel:
// sound 0x106, the slot's help, the step back (CommuBoard_CancelStep). Confirm:
// CommuBoard_PlaceRecord(0); placed, help 0xB5. Neither: the grid cursor moved
// on 0x675F7F and its help line. Then the panel, both slots' lines, and unless
// placed the hand (flashing) and the card.
extern "C" void __cdecl CommuBoard_MoveRecord(void) {
    const unsigned pressed = W(at::kPressed);
    unsigned char placed = 0;
    if (W(at::kCancel) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(CommuBoard_SlotHelp)();
        SH_CALL(CommuBoard_CancelStep)();
    } else if (W(at::kConfirm) & pressed) {
        placed = SH_CALL(CommuBoard_PlaceRecord)(0);
        if (placed) PutW(at::kHelp, 0xB5);
    } else {
        SH_CALL(CommuBoard_MoveGridCursor)(At(at::kPick));
        SH_CALL(CommuBoard_PickHelp)(At(at::kPick));
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kTailArg), 1);
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kPick), 0);
    if (placed) return;
    CursorAndCard(B(at::kTailArg), 1);
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
    const unsigned pressed = W(at::kPressed);
    if (W(at::kCancel) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kBoardState) = static_cast<unsigned char>(B(at::kBoardState) - 1);
        PutW(at::kHelp, 0xB5);
    } else if (W(at::kConfirm) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x103);
        if (B(at::kListCursor) != 0) {
            PutW(at::kHelp, 0x44);
            B(at::kPick) = B(at::kTailArg);
            B(at::kBoardStep) = static_cast<unsigned char>(B(at::kBoardStep) + 1);
        } else {
            const unsigned char kind = B(Slot(B(at::kTailArg)));
            B(at::kPick) = kind == 0 ? 0 : static_cast<unsigned char>(kind - 4);
            B(at::kPickB) = 0;
            B(at::kPickC) = 0;
            B(at::kPickD) = 0xFF;
            B(at::kPickE) = 0xFF;
            B(at::kBoardStep) = 2;
        }
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
        const unsigned char old = B(at::kListCursor);
        const unsigned char count = SH_CALL(Commu_CountInSlot)(B(at::kTailArg));
        if (keys & 0x8000) {
            const unsigned char c = static_cast<unsigned char>(B(at::kListCursor) - 1);
            B(at::kListCursor) = c;
            if (static_cast<signed char>(c) < 0) B(at::kListCursor) = count;
        } else if (keys & 0x2000) {
            const unsigned char c = static_cast<unsigned char>(B(at::kListCursor) + 1);
            B(at::kListCursor) = c;
            if (static_cast<signed char>(c) > static_cast<int>(count)) B(at::kListCursor) = 0;
        }
        if (SB(at::kListCursor) != static_cast<int>(old)) SH_CALL(Sound_PlayEffect)(0x100);
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kTailArg), 1);
    short x = 0, y = 0;
    {
        const unsigned char cursor = B(at::kListCursor);
        const unsigned char arg = B(at::kTailArg);
        SH_CALL(CommuBoard_SlotRecordXY)(&x, &y, arg, cursor);
    }
    if (B(at::kListCursor) == 0) {
        R4EHand(x, y, 0);
        return;
    }
    R4EHand(x + 4, y, 0);
    const unsigned char before = static_cast<unsigned char>(B(at::kListCursor) - 1);
    const unsigned char arg = B(at::kTailArg);
    const unsigned idx = SH_CALL(Commu_NthInSlot)(arg, before);
    SH_CALL(CommuBoard_DrawRecordCard)(x + 0x10, y - 8, idx, 0);
}

// original 0x457CE0: CommuBoard_StepsB[1]: as CommuBoard_MoveRecord, with
// CommuBoard_PlaceRecord(1), no sound of its own on cancel (CancelStep first,
// then the slot's help), and the hand beside the record position.
extern "C" void __cdecl CommuBoard_MoveRecordB(void) {
    const unsigned pressed = W(at::kPressed);
    unsigned char placed = 0;
    if (W(at::kCancel) & pressed) {
        SH_CALL(CommuBoard_CancelStep)();
        SH_CALL(CommuBoard_SlotHelp)();
    } else if (W(at::kConfirm) & pressed) {
        placed = SH_CALL(CommuBoard_PlaceRecord)(1);
        if (placed) PutW(at::kHelp, 0xB5);
    } else {
        SH_CALL(CommuBoard_MoveGridCursor)(At(at::kPick));
        SH_CALL(CommuBoard_PickHelp)(At(at::kPick));
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kTailArg), 1);
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kPick), 0);
    if (placed) return;
    short x = 0, y = 0;
    {
        const unsigned char cursor = B(at::kListCursor);
        const unsigned char arg = B(at::kTailArg);
        SH_CALL(CommuBoard_SlotRecordXY)(&x, &y, arg, cursor);
    }
    R4EHand(x + 4, y, 1);
    const unsigned char before = static_cast<unsigned char>(B(at::kListCursor) - 1);
    const unsigned char arg = B(at::kTailArg);
    const unsigned idx = SH_CALL(Commu_NthInSlot)(arg, before);
    SH_CALL(CommuBoard_DrawRecordCard)(0xA0, 0x3E, idx, 1);
}

namespace {

// The lists' hand: at the slot's first record position, flashing (the three
// list steps and the confirm step draw it first).
void HandAtSlot() {
    short x = 0, y = 0;
    const unsigned char arg = B(at::kTailArg);
    SH_CALL(CommuBoard_SlotRecordXY)(&x, &y, arg, 0);
    R4EHand(x, y, 1);
}

}  // namespace

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
    const unsigned pressed = W(at::kPressed);
    if (W(at::kCancel) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        SH_CALL(CommuBoard_SlotHelp)();
        B(at::kBoardStep) = 0;
    } else if (W(at::kConfirm) & pressed) {
        const int row = SB(at::kPick);
        const unsigned char next = B(at::kPickNext + static_cast<U>(row));
        if (next == 0xFF) {
            SH_AT(void (__cdecl*)(), at::kR4CAsk)();
        } else {
            const U slot = Slot(B(at::kTailArg));
            B(at::kPickB) = row == static_cast<int>(B(slot)) - 4 ? B(slot + 1) : 0;
            B(at::kPickD) = next;
            SH_CALL(Sound_PlayEffect)(0x103);
            B(at::kBoardStep) = static_cast<unsigned char>(B(at::kBoardStep) + 1);
            PutW(at::kHelp, W(at::kPickNextHelp + 2 * B(at::kPickD)));
        }
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0x5000);
        const unsigned char old = B(at::kPick);
        unsigned char c = old;
        if (keys & 0x1000) {
            --c;
            B(at::kPick) = c;
            if (static_cast<signed char>(c) < 0) {
                c = static_cast<unsigned char>(B(at::kListCount) - 1);
                B(at::kPick) = c;
            }
        } else if (keys & 0x4000) {
            ++c;
            B(at::kPick) = c;
            if (static_cast<signed char>(c) >= static_cast<int>(B(at::kListCount))) {
                c = 0;
                B(at::kPick) = c;
            }
        }
        if (static_cast<signed char>(c) != static_cast<int>(old)) {
            SH_CALL(Sound_PlayEffect)(0x100);
            c = B(at::kPick);
        }
        PutW(at::kHelp, W(at::kPickHelpB + 2 * static_cast<U>(static_cast<signed char>(c))));
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kTailArg), 1);
    HandAtSlot();
    const int y = static_cast<short>(SH_CALL(CommuBoard_ListY)());   // its callers read the low word only
    SH_CALL(CommuBoard_DrawListBox)(Column(B(at::kTailArg)) + 0x42, y, 0);
    const int hy = static_cast<short>(SB(at::kPick)) * 16 + y + 4;
    SH_CALL(Menu_DrawHand)(Column(B(at::kTailArg)) + 0x44, hy, 0);
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
    const unsigned pressed = W(at::kPressed);
    if (W(at::kCancel) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kBoardStep) = static_cast<unsigned char>(B(at::kBoardStep) - 1);
        B(at::kPickD) = 0xFF;
    } else if (W(at::kConfirm) & pressed) {
        const unsigned char next = B(at::kPickPairs + 1 + 2 * B(at::kPickD));
        if (next == 0xFF) {
            SH_AT(void (__cdecl*)(), at::kR4CAsk)();
        } else {
            const U slot = Slot(B(at::kTailArg));
            B(at::kPickC) = SB(at::kPick) == static_cast<int>(B(slot)) - 4 ? B(slot + 2) : 0;
            B(at::kPickE) = next;
            SH_CALL(Sound_PlayEffect)(0x103);
            B(at::kBoardStep) = static_cast<unsigned char>(B(at::kBoardStep) + 1);
            PutW(at::kHelp, 0x46);
        }
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0x5000);
        const unsigned char old = B(at::kPickB);
        unsigned char c = old;
        if (keys & 0x1000) {
            --c;
            B(at::kPickB) = c;
            if (static_cast<signed char>(c) < 0) {
                c = static_cast<unsigned char>(B(at::kPickPairs + 2 * B(at::kPickD)) - 1);
                B(at::kPickB) = c;
            }
        } else if (keys & 0x4000) {
            ++c;
            B(at::kPickB) = c;
            if (static_cast<signed char>(c) >= static_cast<int>(B(at::kPickPairs + 2 * B(at::kPickD)))) {
                c = 0;
                B(at::kPickB) = c;
            }
        }
        if (static_cast<signed char>(c) != static_cast<int>(old)) SH_CALL(Sound_PlayEffect)(0x100);
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kTailArg), 1);
    HandAtSlot();
    const int y = static_cast<short>(SH_CALL(CommuBoard_ListY)());   // its callers read the low word only
    SH_CALL(CommuBoard_DrawListBox)(Column(B(at::kTailArg)) + 0x42, y, 1);
    const unsigned char list = B(at::kPickD);
    if (list == 0xFF) return;
    const int y2 = y + static_cast<short>(SB(at::kPick)) * 16 + 0xC;
    const int yb = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(B(at::kTailArg)) + 0x4A, y2, list, 0));
    const int hy = static_cast<short>(SB(at::kPickB)) * 16 + yb + 4;
    SH_CALL(Menu_DrawHand)(Column(B(at::kTailArg)) + 0x4C, hy, 0);
}

// original 0x458240: CommuBoard_Steps[6], StepsB[4]: the toggle 0x675F7D.
// Cancel: sound 0x106, the step back, help 0x45, 0x675F82 0xFF. Confirm: R4C's
// 0x459EE0. Neither: 0x1000 or 0x4000 (auto-repeated) flips 0x675F7D with sound
// 0x100. Then the panel, the slot's lines, the hand, both lists (steady) and,
// unless 0x675F82 is 0xFF, the third list and the hand at the toggle.
extern "C" void __cdecl CommuBoard_PickListC(void) {
    const unsigned pressed = W(at::kPressed);
    if (W(at::kCancel) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kBoardStep) = static_cast<unsigned char>(B(at::kBoardStep) - 1);
        PutW(at::kHelp, 0x45);
        B(at::kPickE) = 0xFF;
    } else if (W(at::kConfirm) & pressed) {
        SH_AT(void (__cdecl*)(), at::kR4CAsk)();
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0x5000);
        if (keys & 0x5000) {
            SH_CALL(Sound_PlayEffect)(0x100);
            B(at::kPickC) ^= 1;
        }
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kTailArg), 1);
    HandAtSlot();
    const int y = static_cast<short>(SH_CALL(CommuBoard_ListY)());   // its callers read the low word only
    SH_CALL(CommuBoard_DrawListBox)(Column(B(at::kTailArg)) + 0x42, y, 1);
    const int y2 = y + static_cast<short>(SB(at::kPick)) * 16 + 0xC;
    const unsigned char list = B(at::kPickD);
    const int yb = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(B(at::kTailArg)) + 0x4A, y2, list, 1));
    const unsigned char list2 = B(at::kPickE);
    if (list2 == 0xFF) return;
    const int y3 = yb + static_cast<short>(SB(at::kPickB)) * 16 + 0xC;
    const int yc = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(B(at::kTailArg)) + 0x52, y3, list2, 0));
    const int hy = static_cast<short>(SB(at::kPickC)) * 16 + yc + 4;
    SH_CALL(Menu_DrawHand)(Column(B(at::kTailArg)) + 0x54, hy, 0);
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
    const unsigned pressed = W(at::kPressed);
    if (W(at::kConfirm) & pressed) {
        if (B(at::kToggle) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            PutW(at::kHelp, W(at::kKeptHelp));
            B(at::kBoardStep) = B(at::kKeptStep);
        } else {
            unsigned char arg = B(at::kTailArg);
            const unsigned char first = B(at::kPickB);
            const U slot = Slot(arg);
            const bool same = static_cast<int>(B(slot)) == SB(at::kPick) + 4 && static_cast<int>(B(slot + 1)) == SB(at::kPickB) &&
                              static_cast<int>(B(slot + 2)) == SB(at::kPickC);
            if (!same) {
                for (U r = at::kMembers; r < at::kMembersEnd; r += at::kMemberStride) {
                    if (B(r + 1) != arg) continue;
                    PutL(r + 4, L(at::kClock));
                    B(r + 2) = 0;
                    B(r + 3) = 0;
                    arg = B(at::kTailArg);
                }
                B(SlotToWrite(arg, "CommuBoard_Confirm") + 3) = 0;
                PutL(SlotToWrite(B(at::kTailArg), "CommuBoard_Confirm") + 4, L(at::kClock));
                arg = B(at::kTailArg);
            }
            B(SlotToWrite(arg, "CommuBoard_Confirm")) = static_cast<unsigned char>(B(at::kPick) + 4);
            B(SlotToWrite(B(at::kTailArg), "CommuBoard_Confirm") + 1) = first;
            const unsigned char toggle = B(at::kPickC);
            B(at::kCursorB) = B(at::kSavedB);
            B(SlotToWrite(B(at::kTailArg), "CommuBoard_Confirm") + 2) = toggle;
            const unsigned char a = B(at::kSavedA);
            const unsigned char mode = B(at::kSavedMode);
            B(at::kCursorA) = a;
            B(at::kCursorMode) = mode;
            SH_CALL(Sound_PlayEffect)(0x104);
            B(at::kBoardState) = static_cast<unsigned char>(B(at::kBoardState) - 1);
            PutW(at::kHelp, 0xB5);
        }
    } else if (W(at::kCancel) & pressed) {
        SH_CALL(Sound_PlayEffect)(0x106);
        PutW(at::kHelp, W(at::kKeptHelp));
        B(at::kBoardStep) = B(at::kKeptStep);
    } else {
        const unsigned keys = SH_CALL(Input_AutoRepeat)(pressed & 0xA000);
        if (keys & 0xA000) {
            SH_CALL(Sound_PlayEffect)(0x100);
            B(at::kToggle) ^= 1;
        }
    }
    SH_CALL(CommuBoard_DrawPanel)();
    SH_CALL(CommuBoard_DrawSlotLines)(B(at::kTailArg), 1);
    HandAtSlot();
    const int y = static_cast<short>(SH_CALL(CommuBoard_ListY)());   // its callers read the low word only
    if (B(at::kPickD) == 0xFF) {
        SH_CALL(CommuBoard_DrawListBox)(Column(B(at::kTailArg)) + 0x42, y, 0);
        const int hy = static_cast<short>(SB(at::kPick)) * 16 + y + 4;
        SH_CALL(Menu_DrawHand)(Column(B(at::kTailArg)) + 0x44, hy, 0);
        return;
    }
    SH_CALL(CommuBoard_DrawListBox)(Column(B(at::kTailArg)) + 0x42, y, 1);
    const int y2 = y + static_cast<short>(SB(at::kPick)) * 16 + 0xC;
    if (B(at::kPickE) == 0xFF) {
        const unsigned char list = B(at::kPickD);
        const int yb = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(B(at::kTailArg)) + 0x4A, y2, list, 0));
        const int hy = static_cast<short>(SB(at::kPickB)) * 16 + yb + 4;
        SH_CALL(Menu_DrawHand)(Column(B(at::kTailArg)) + 0x4C, hy, 0);
        return;
    }
    const unsigned char list = B(at::kPickD);
    const int yb = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(B(at::kTailArg)) + 0x4A, y2, list, 1));
    const int y3 = yb + static_cast<short>(SB(at::kPickB)) * 16 + 0xC;
    const unsigned char list2 = B(at::kPickE);
    const int yc = static_cast<short>(SH_CALL(CommuBoard_DrawListBoxB)(Column(B(at::kTailArg)) + 0x52, y3, list2, 0));
    const int hy = static_cast<short>(SB(at::kPickC)) * 16 + yc + 4;
    SH_CALL(Menu_DrawHand)(Column(B(at::kTailArg)) + 0x54, hy, 0);
}

// ===========================================================================
// The board's draws
// ===========================================================================

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
    SH_CALL(Menu_DrawBox)(x + 4, y + 4, 0x48, 0x28, 0x81, B(at::kColour));
    SH_CALL(Menu_DrawBox)(x, y, 0x48, 0x26, 0x80, B(at::kColour));
    SH_CALL(CommuBoard_DrawCardFrame)(x, y);
    const unsigned r = record & 0xFF;
    SH_CALL(Text_DrawAt)(x + 6, y + 4, 0, 5, At(at::kNames + 5 * r));
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
    const U bars = at::kBars + 20 * r;
    const int bx = x + 6;
    if ((highlight & 0xFF) == 0) {
        SH_CALL(CommuBoard_DrawBar)(bx, y + 0x12, B(bars) * 12, 0, 0);
        SH_CALL(CommuBoard_DrawBar)(bx, y + 0x16, B(bars + 1) * 12, 1, 0);
        SH_CALL(CommuBoard_DrawBar)(bx, y + 0x1A, B(bars + 2) * 12, 2, 0);
        SH_CALL(CommuBoard_DrawBar)(bx, y + 0x1E, B(bars + 3) * 12, 3, 0);
        return;
    }
    const signed char pick = SB(at::kPick);
    unsigned char k;
    if (pick == 0)
        k = 0;
    else if (pick >= 9)
        k = static_cast<unsigned char>(pick - 8);
    else
        k = B(at::kSlots + 8 * static_cast<U>(static_cast<int>(pick)));
    const U lit = at::kBarHighlight + k;
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
    SH_CALL(Menu_DrawBox)(0x14, 0x12, 0x118, 0x13, 0xF2, B(at::kColour));
    SH_CALL(Menu_DrawOutlineNotched)(0x16, 0x14, 0x113, 0xF, 0);
    const unsigned help = W(at::kHelp);
    if (help != 0xFFFF) {
        const unsigned char* const text = At(at::kMessagePools + W(at::kMessagePools + 2 * help));
        SH_CALL(Text_DrawAt)(0x1D, 0x15, 0, 0xFF, text);
        if (W(at::kHelp) == 0x3D) {
            SH_CALL(Gpu_SetDrawMode)(Packet(), 0, 0, 0, 0);
            SH_CALL(Gfx_CommitPrim)(1, 0xC);
            const unsigned char intensity = Flash();
            unsigned char* const q = Packet();
            SH_CALL(Gpu_SetTile)(q);
            SH_CALL(Gpu_SetSemiTrans)(q, 1);
            PutFBits(q + 0xC, 0x41B00000u);   // 22.0
            const unsigned t = B(at::kToggle);
            PutF(q + 8, static_cast<float>(static_cast<int>(t * 36 + 0xD0)));
            PutFBits(q + 0x18, 0x41400000u);   // 12.0
            q[4] = intensity;
            q[5] = intensity;
            q[6] = 0;
            const unsigned t2 = B(at::kToggle);
            PutF(q + 0x14, static_cast<float>(static_cast<int>(t2 * 12 + 0x19)));
            SH_CALL(Gfx_CommitPrim)(1, 0x1C);
        }
    }
    SH_CALL(Menu_DrawBox)(0x15, 0x2E, 0x114, 0xAA, 0x80, B(at::kColour));
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
    U slot = at::kSlots + 8;
    for (unsigned char i = 0; i < 8; ++i, slot += 8) {
        const int sy = (i >> 2) * 61;
        const int sx = static_cast<int>(i % 4) * 52;
        SH_CALL(Menu_DrawOutlineNotched)(sx + 0x20, sy + 0x6B, 0x28, 0x28, 1);
        if (i >= B(at::kAreaLimits + W(at::kArea))) {
            SH_CALL(Menu_DrawBox)(sx + 0x20, sy + 0x6B, 0x28, 0x28, 1, B(at::kColour));
            continue;
        }
        SH_CALL(CommuBoard_DrawSprite)(sx + 0x1A, sy + 0x5B, 0);
        const unsigned char kind = B(slot);
        if (kind != 0) {
            const unsigned char digit = kind == 5 ? static_cast<unsigned char>(B(at::kSlotIcons + 5) + B(slot + 1))
                                                  : B(at::kSlotIcons + kind);
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
    char* const out = reinterpret_cast<char*>(At(at::kTextBuffer));
    SH_CALL(Crt_sprintf)(out, reinterpret_cast<const char*>(At(at::kCountFormat)), static_cast<int>(SB(at::kCountA)));
    SH_CALL(Text_DrawFont12)(0x50, 0x4C, 0, At(at::kTextBuffer));
    // DIV-0064 group 14: the label from the overlay's buffer, its count 2 the
    // Chinese word's (the overlay's is longer); the number after it then
    // moves from 0x98 - under the seven letters of `Culture` - to 0xBE, where
    // the US screen has it (the owner's web reference, 2026-10-10).
    const bool words = Labels_Written(14);
    SH_CALL(Text_DrawAt)(0x78, 0x4C, 0, words ? 0xFF : 2, words ? Labels_Slot(14, 0) : At(at::kPanelLabel));
    SH_CALL(Crt_sprintf)(out, reinterpret_cast<const char*>(At(at::kNumberFormat)), static_cast<unsigned>(B(at::kCountC)));
    SH_CALL(Text_DrawFont12)(words ? 0xBE : 0x98, 0x4C, 0, At(at::kTextBuffer));
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
    const U row = at::kSlotLines + 12 * s;
    const auto locked = [s] { return s != 0 && s < 9 && s > B(at::kAreaLimits + W(at::kArea)); };
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
    for (U r = at::kMembers; r < at::kMembersEnd; r += at::kMemberStride)
        if (B(r) != 0 && B(r + 1) == s) ++n;
    return n;
}

// original 0x459460: the index of the `nth` (0-based) record in use in `slot`,
// or 0xFF when there are fewer.
extern "C" unsigned __cdecl Commu_NthInSlot(unsigned slot, unsigned nth) {
    const unsigned char s = static_cast<unsigned char>(slot), want = static_cast<unsigned char>(nth);
    unsigned char seen = 0;
    for (unsigned k = 0; k < 60; ++k) {
        const U r = at::kMembers + 8 * k;
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
    const unsigned keys = SH_CALL(Input_AutoRepeat)(W(at::kPressed) & 0xF000);
    unsigned char& mode = B(at::kCursorMode);
    unsigned char& row = B(at::kCursorA);
    unsigned char& col = B(at::kCursorB);
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
    PutW(q + 0x16, 0x7840u | (B(at::kDigitCells + n) >> 4));
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
    const U e = at::kSprites + 6 * (id & 0xFF);
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

// original 0x4598A0: the record list's box at (x, y), 0x9046CB rows of 16: its
// shadow (x + 4, y + 5) 0x81 unless `steady`, the box 0x80 (both in the window
// style), its frame (CommuBoard_DrawListFrame at y - 1) and the rows' lines
// (the pointers at 0x669E68) at (x + 6, y + 2 + 16 * i), colour 7 * steady.
extern "C" void __cdecl CommuBoard_DrawListBox(int x, int y, unsigned steady) {
    const unsigned char flag = static_cast<unsigned char>(steady);
    const int h = B(at::kListCount) * 16;
    if (flag == 0) SH_CALL(Menu_DrawBox)(x + 4, y + 5, ListBoxWidth(), h, 0x81, B(at::kColour));
    SH_CALL(Menu_DrawBox)(x, y, ListBoxWidth(), h + 1, 0x80, B(at::kColour));
    SH_CALL(CommuBoard_DrawListFrame)(x, y - 1, static_cast<unsigned>(h));
    if (B(at::kListCount) == 0) return;
    const unsigned char colour = static_cast<unsigned char>(flag * 7);
    for (unsigned i = 0; i < B(at::kListCount); ++i) {
        const unsigned char* const line = At(L(at::kListTextA + 4 * i));
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
    if (B(at::kCursorA) == 0) {
        const unsigned n = B(at::kListCount);
        return n > 4 ? 0x6A - static_cast<int>((n - 4) * 9) : 0x6A;
    }
    return 0xAC - static_cast<int>(B(at::kListCount)) * 12;
}

// original 0x459A80: list `list`'s box at (x, y): its 2-byte row 0x652C6C + 2 *
// list {first, count}, h = 16 * count; when y + h + 4 passes 0xD8 (s16 y) the
// box moves up to 0xD4 - h. Its shadow unless `steady`, the box, its frame,
// and its lines (the pointers 0x669EE0[first + i]) at (x + 6, y + 2 + 16 * i),
// colour 7 * steady. Answers the y it drew at (ax).
extern "C" unsigned short __cdecl CommuBoard_DrawListBoxB(int x, int y, unsigned list, unsigned steady) {
    const U row = at::kLists + 2 * (list & 0xFF);
    const int h = B(row + 1) * 16;
    if (h + static_cast<short>(y) + 4 > 0xD8) y = 0xD4 - h;
    const unsigned char flag = static_cast<unsigned char>(steady);
    if (flag == 0) SH_CALL(Menu_DrawBox)(x + 4, y + 6, ListBoxWidth(), h, 0x81, B(at::kColour));
    SH_CALL(Menu_DrawBox)(x, y, ListBoxWidth(), h + 1, 0x80, B(at::kColour));
    SH_CALL(CommuBoard_DrawListFrame)(x, y - 1, static_cast<unsigned>(h));
    if (B(row + 1) != 0) {
        const unsigned char colour = static_cast<unsigned char>(flag * 7);
        for (unsigned i = 0; i < B(row + 1); ++i) {
            const unsigned char* const line = At(L(at::kListTexts + 4 * (B(row) + i)));
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
    const U e = at::kSlotOffsets + 4 * (t + static_cast<unsigned char>(kk - 1));
    *x = static_cast<short>(W(e) + static_cast<unsigned>(cx));
    *y = static_cast<short>(W(e + 2) + static_cast<unsigned>(cy));
}

// original 0x459C40: the step back: the grid cursor put back from 0x675F84..86,
// sound 0x106, 0x939A40 - 1.
extern "C" void __cdecl CommuBoard_CancelStep(void) {
    const unsigned char b = B(at::kSavedB), a = B(at::kSavedA), mode = B(at::kSavedMode);
    B(at::kCursorB) = b;
    B(at::kCursorA) = a;
    B(at::kCursorMode) = mode;
    SH_CALL(Sound_PlayEffect)(0x106);
    B(at::kBoardStep) = static_cast<unsigned char>(B(at::kBoardStep) - 1);
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
    const unsigned char mode = B(at::kCursorMode);
    bool refused;
    if (mode == 0 || mode == 2) {
        refused = false;
    } else if (mode == 1) {
        const unsigned char n = SH_CALL(Commu_CountInSlot)(B(at::kPick));
        refused = n == 3 || static_cast<int>(SB(at::kPick)) > static_cast<int>(B(at::kAreaLimits + W(at::kArea)));
    } else {
        refused = true;
    }
    const unsigned char arg = B(at::kTailArg);
    if (!refused && static_cast<int>(SB(at::kPick)) == static_cast<int>(arg)) refused = true;
    if (refused) {
        SH_CALL(Sound_PlayEffect)(0x107);
        return 0;
    }
    const unsigned char b = B(at::kSavedB), a = B(at::kSavedA), m = B(at::kSavedMode);
    B(at::kCursorB) = b;
    B(at::kCursorA) = a;
    B(at::kCursorMode) = m;
    const unsigned char nth = static_cast<unsigned char>(B(at::kListCursor) - static_cast<unsigned char>(back));
    const unsigned idx = SH_CALL(Commu_NthInSlot)(arg, nth);
    const U r = Member(idx & 0xFF, "CommuBoard_PlaceRecord");
    B(r + 1) = B(at::kPick);
    PutL(r + 4, L(at::kClock));
    const unsigned char state = B(at::kBoardState);
    B(r + 2) = 0;
    B(r + 3) = 0;
    B(at::kBoardState) = static_cast<unsigned char>(state - 1);
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
    const unsigned char arg = B(at::kTailArg);
    if (arg == 0) {
        PutW(at::kHelp, 0x6A);
        return;
    }
    if (arg >= 9) {
        PutW(at::kHelp, (arg * 2u + 0x2C) & 0xFFFF);
        return;
    }
    const U slot = Slot(arg);
    const unsigned char kind = B(slot);
    if (kind == 0) {
        PutW(at::kHelp, 0x6A);
        return;
    }
    const unsigned char k = static_cast<unsigned char>(kind - 4);
    if (k == 0) {
        PutW(at::kHelp, B(slot + 1) + 0x4Au);
        return;
    }
    if (k > 9)
        bof3::Fatal("CommuBoard_SlotHelp: slot %u's kind %u is past the nine of its stack table; the original reads the "
                    "frame above it",
                    static_cast<unsigned>(arg), static_cast<unsigned>(kind));
    PutW(at::kHelp, kHelps[k - 1]);
}

// original 0x459E50: the help line for the slot at `cell`: 0x44 on 0x9039F5's
// own slot; 0x69 for slot 0; from 9 the u16 0x652C72[slot]; past the area's
// limit 0x44; for an empty slot (kind 0) 0x69; else the u16 0x652AF0[kind]
// (each read in place).
extern "C" void __cdecl CommuBoard_PickHelp(unsigned char* cell) {
    const unsigned char v = *cell;
    if (v == B(at::kTailArg)) {
        PutW(at::kHelp, 0x44);
        return;
    }
    if (v == 0) {
        PutW(at::kHelp, 0x69);
        return;
    }
    if (v >= 9) {
        PutW(at::kHelp, W(at::kSlotHelp + 2 * v));
        return;
    }
    if (v > B(at::kAreaLimits + W(at::kArea))) {
        PutW(at::kHelp, 0x44);
        return;
    }
    const unsigned char kind = B(Slot(v));
    PutW(at::kHelp, kind == 0 ? 0x69 : W(at::kPickHelp + 2 * kind));
}

#pragma clang attribute pop

void Rest4B_Inject() {
    if (bof3::WantsShadow("rest_4b")) rest_4b::SelfTest();
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
