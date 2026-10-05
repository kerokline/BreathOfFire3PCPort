// Group R4C of round fourteen (wave four): 60 functions at 0x459EE0..0x45C3F3 -
// the cut's 60 rows for R4C (analysis/round14_cut.tsv), none added, none
// dropped, each read to its last instruction with capstone (2026-10-05) and
// fuzzed through the scenario harness's field mode (rest_4c_fuzz.cpp).
// docs/rest_4c.md has them one row each.
//
// The field hook's state 2 (R4B's table 0x652A70 by 0x9039F4) jumps through
// R4B's 0x652A84 by the game byte 0x9039F5; its entries 0 and 1 are this
// group's CommuHiLo_Run and CommuHitBlow_Run, each a dispatcher on the phase
// byte 0x939A3E through its own Phases table, whose entries dispatch on the
// state byte 0x939A40 and (two levels down) the step byte 0x939A3F. The PSX
// twins lie in the COMMU overlays (analysis/pairs_propagated.json); none has
// a name in the sibling.
//
// Every one is a faithful replacement. Where the original indexes a .data
// table, the stake's digits, the cards or the guess records by a byte it never
// bounds, ours aborts with a message before the access (round9 doc section 6;
// docs/rest_4c.md section 7 has the one policy). Every call goes through the
// harness (SH_CALL / SH_AT), so the start-up fuzz can stand recorders in for
// the callees; every cell is re-read after a call where the original re-reads
// it, and a callee's answer is taken into a local before the next read.
#include "game/rest_4c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_4c_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4c::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();

unsigned char& B(U address) { return At(address)[0]; }
signed char S8(U address) { return static_cast<signed char>(At(address)[0]); }
U L(U address) { return static_cast<U>(Long(At(address))); }
void SetL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void PutFloat(unsigned char* p, unsigned offset, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p + offset, &f, sizeof f);
}
void PutWord(unsigned char* p, unsigned offset, unsigned v) { SetWord(p + offset, v); }
// A word argument read as movsx word.
int S16(U v) { return static_cast<std::int16_t>(v & 0xFFFF); }

// jmp [table + 4 * byte]: the table's `entries` handlers, read in place (the
// fuzz swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after.
void Dispatch(const char* who, const unsigned long* table, unsigned entries, U byte_address) {
    const unsigned index = B(byte_address);
    if (index >= entries)
        bof3::Fatal("%s: the byte 0x%X is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4c.md section 7)",
                    who, (unsigned)byte_address, index, entries, (unsigned)Key(table));
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
    SH_AT(void (__cdecl*)(int, int, unsigned), at::kGuessPanel)(x, y, flag);
}
void GuessRow(int x, int y, unsigned row, unsigned flag) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kGuessRow)(x, y, row, flag);
}
void Secret(int x, int y, unsigned show) { SH_AT(void (__cdecl*)(int, int, unsigned), at::kSecret)(x, y, show); }
unsigned Rnd() { return static_cast<unsigned>(SH_CALL(Rand)()); }
int Request() { return Field_Request; }
const unsigned char* Text(U address) { return At(address); }
char* Buffer() { return reinterpret_cast<char*>(At(bof3::addr::Text_Records - 0x140)); }   // 0x904BA0

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
    B(at::kMusic) = Music_Track;
    SH_CALL(Music_FadeOutStop)(10);
    B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
}

// The two games' close phase (0x45AF00, 0x45C3A0): state 0 fades the music
// once no message is up; then, once the load is done, the kept track plays (8
// frames) and the field hook's tail state moves on.
void ClosePhase() {
    if (B(at::kState) == 0) {
        if (Request() == 2) return;
        SH_CALL(Music_FadeOutStop)(10);
        B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
        return;
    }
    if (SH_CALL(File_LoadDone)() == 0) return;
    SH_CALL(Music_Play)(L(at::kMusic) & 0xFF, 8);
    B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + 1);
}

}  // namespace

// ==== the shared helpers ================================================================

// original 0x459EE0 (0x3D bytes): called by R4B's 0x457DD0, 0x457FE0 and
// 0x458240. 0x675F81 = 0x939A40, the word 0x675F78 = the word 0x675F7A (both
// read first); Sound_PlayEffect(0x103); the word 0x675F7A = 0x3D, 0x675F7C =
// 0, 0x939A40 = 5.
extern "C" void __cdecl Commu_PushSubscreen(void) {
    const unsigned char state = B(at::kState);
    const U sub = Word(At(at::kSub));
    B(at::kSavedState) = state;
    SetWord(At(at::kSavedSub), sub);
    Sound(0x103);
    SetWord(At(at::kSub), 0x3D);
    B(at::kSub7C) = 0;
    B(at::kState) = 5;
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
    PutFloat(p, 8, S16(static_cast<U>(x)));
    PutFloat(p, 0xC, S16(static_cast<U>(y)));
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

// original 0x45B3A0 (0x57 bytes): a LINE_F2 (Gpu_SetLineF2) at the packet
// cursor from (s16 x, s16 y) to (x + 0xC, y), shade 0x80; Gfx_CommitPrim(1,
// 0x20) - the stake digit's underline.
extern "C" void __cdecl Commu_DrawUnderline(int x, int y) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    const int x0 = S16(static_cast<U>(x)), y0 = S16(static_cast<U>(y));
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
    const unsigned char* const e = At(at::kPieces + 4 * i);
    PutFloat(p, 8, S16(static_cast<U>(x)));
    PutFloat(p, 0xC, S16(static_cast<U>(y)));
    PutWord(p, 0x18, e[2]);
    PutWord(p, 0x1A, e[3]);
    p[4] = p[5] = p[6] = 0x80;
    p[0x14] = e[0];
    p[0x15] = e[1];
    PutWord(p, 0x16, 0x7887);
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
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

// original 0x45B490 (0x89 bytes): the money box at (x, y): Menu_DrawBox(x + 1,
// y + 1, 0x6A, 0x28, 0x80, the style byte 0x903A5A read first), its frame,
// the title (Text_DrawAt(x + 0x24, y + 7, 0, 3, 0x669F60)), Party_Zenny (read
// after the title) through sprintf(0x904BA0, 0x64ADDC) in the 12-dot font at
// (x + 6, y + 0x18), the unit (Text_DrawAt(x + 0x5A, y + 0x18, 0, 1,
// 0x66A31C)).
extern "C" void __cdecl Commu_DrawZennyBox(int x, int y) {
    const unsigned style = B(at::kStyle);
    SH_CALL(Menu_DrawBox)(x + 1, y + 1, 0x6A, 0x28, 0x80, static_cast<int>(style));
    SH_CALL(Commu_DrawFrame)(x, y);
    SH_CALL(Text_DrawAt)(x + 0x24, y + 7, 0, 3, Text(at::kZennyTitle));
    SH_CALL(Crt_sprintf)(Buffer(), reinterpret_cast<const char*>(At(at::kFmtWide)), L(bof3::addr::Party_Zenny));
    SH_CALL(Text_DrawFont12)(x + 6, y + 0x18, 0, reinterpret_cast<const unsigned char*>(Buffer()));
    SH_CALL(Text_DrawAt)(x + 0x5A, y + 0x18, 0, 1, Text(at::kZennyUnit));
}

// original 0x45B5F0 (0x177 bytes): the stake box at (x, y): as the money box
// with the title 0x669F68 and the amount 0x675F88 (read after the title);
// digits' low byte 2, 3, 4 with the amount below 10, 100, 1000 (signed) -
// sprintf by the narrow formats 0x65306C, 0x653074, 0x65307C at x + 0x42,
// + 0x36, + 0x2A; otherwise the wide format 0x64ADDC at x + 6. The unit as the
// money box's.
extern "C" void __cdecl Commu_DrawStakeBox(int x, int y, unsigned digits) {
    const unsigned style = B(at::kStyle);
    SH_CALL(Menu_DrawBox)(x + 1, y + 1, 0x6A, 0x28, 0x80, static_cast<int>(style));
    SH_CALL(Commu_DrawFrame)(x, y);
    SH_CALL(Text_DrawAt)(x + 0x24, y + 7, 0, 3, Text(at::kBetTitle));
    const auto amount = static_cast<std::int32_t>(L(at::kBet));
    U format = at::kFmtWide;
    int left = x + 6;
    switch (digits & 0xFF) {
    case 2:
        if (amount < 10) format = at::kFmt1, left = x + 0x42;
        break;
    case 3:
        if (amount < 100) format = at::kFmt2, left = x + 0x36;
        break;
    case 4:
        if (amount < 1000) format = at::kFmt3, left = x + 0x2A;
        break;
    default:
        break;
    }
    SH_CALL(Crt_sprintf)(Buffer(), reinterpret_cast<const char*>(At(format)), amount);
    SH_CALL(Text_DrawFont12)(left, y + 0x18, 0, reinterpret_cast<const unsigned char*>(Buffer()));
    SH_CALL(Text_DrawAt)(x + 0x5A, y + 0x18, 0, 1, Text(at::kZennyUnit));
}

// ==== the first game: CommuHiLo ===========================================================

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
    SH_AT(void (__cdecl*)(unsigned char*), at::kSetPolyF3)(p);
    const int x0 = S16(static_cast<U>(x)), y0 = S16(static_cast<U>(y));
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
            y = B(at::kPicks + i) != 0 ? 0x74 : 0x64;
        }
        if (i < m) {
            Room("CommuHiLo_DrawRow (0x45AFF0)", "the value", i, 0, 8);
            Card(0x10 + 32 * i, y, B(at::kCards + i));
        } else {
            Card(0x10 + 32 * i, y, 0xFF);
        }
    }
}

// original 0x45B080 (0x44 bytes): the nine values 0x675F98.. a shuffle of
// 1..9 - each Commu_RandDigit(9) (its al) drawn again while an earlier value
// has it.
extern "C" void __cdecl CommuHiLo_Shuffle(void) {
    B(at::kCards) = static_cast<unsigned char>(SH_CALL(Commu_RandDigit)(9));
    for (int i = 1; i < 9; ++i) {
        unsigned char v;
        int j;
        do {
            v = static_cast<unsigned char>(SH_CALL(Commu_RandDigit)(9));
            for (j = 0; j < i && B(at::kCards + j) != v; ++j) {}
        } while (j != i);
        B(at::kCards + i) = v;
    }
}

// original 0x45B1E0 (0xD1 bytes): one choice at (x, y): Menu_DrawBox(x, y,
// 0x2A, 0x14, 0, the style byte read first); the draw mode
// (Gpu_SetDrawMode(cursor, 0, 0, 0x1D, 0), Gfx_CommitPrim(1, 0xC)); by mode's
// low byte: 0 piece 0xA and the label in colour 0, 1 piece 9 and colour 0, 2
// piece 0xA and colour 7 - the label Text_DrawAt(x + 0xB, y + 3, colour, 2,
// 0x669F54[label's low byte]); any other mode nothing more. The label table
// has 3 entries; ours aborts past them.
extern "C" void __cdecl CommuHiLo_DrawChoice(int x, int y, unsigned label, unsigned mode) {
    const unsigned style = B(at::kStyle);
    SH_CALL(Menu_DrawBox)(x, y, 0x2A, 0x14, 0, static_cast<int>(style));
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x1D, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned m = mode & 0xFF;
    if (m > 2) return;
    Piece(x, y, m == 1 ? 9 : 0xA);
    const unsigned i = label & 0xFF;
    Room("CommuHiLo_DrawChoice (0x45B1E0)", "the label", static_cast<int>(i), 0, at::kChoiceLabelsCount - 1);
    const auto* const text = reinterpret_cast<const unsigned char*>(
        static_cast<std::uintptr_t>(L(at::kChoiceLabels + 4 * i)));
    SH_CALL(Text_DrawAt)(x + 0xB, y + 3, m == 2 ? 7 : 0, 2, text);
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

// original 0x459F20 (0xE bytes): CommuGame[0] (R4B's 0x652A84) - jmp
// [CommuHiLo_Phases + 0x939A3E * 4], unchecked (ours aborts past its 3).
extern "C" void __cdecl CommuHiLo_Run(void) {
    Dispatch("CommuHiLo_Run (0x459F20)", CommuHiLo_Phases, CommuHiLo_Phases_count, at::kPhase);
}

// original 0x459F30 (0xE bytes): CommuHiLo_Phases[0] - jmp
// [CommuHiLo_OpenStates + 0x939A40 * 4] (2).
extern "C" void __cdecl CommuHiLo_OpenDispatch(void) {
    Dispatch("CommuHiLo_OpenDispatch (0x459F30)", CommuHiLo_OpenStates, CommuHiLo_OpenStates_count, at::kState);
}

// original 0x459F40 (0x36 bytes): CommuHiLo_OpenStates[0] - the dwords
// 0x675F98, 0x675F9C = -1 and the word 0x675FA0 = 0xFFFF (the values and
// pick 0 to 0xFF), Music_Track kept in 0x675F90; Music_FadeOutStop(10);
// 0x939A40 (read after) + 1.
extern "C" void __cdecl CommuHiLo_OpenFade(void) {
    const unsigned char track = Music_Track;
    SetL(at::kCards, 0xFFFFFFFFu);
    SetL(at::kCards + 4, 0xFFFFFFFFu);
    B(at::kMusic) = track;
    SetWord(At(at::kCards + 8), 0xFFFF);
    SH_CALL(Music_FadeOutStop)(10);
    B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
}

// original 0x459F80 (0x2C bytes): CommuHiLo_OpenStates[1] - once
// File_LoadDone: Music_Play(0x94, 8), 0x939A40 = 0, 0x939A3E (read after) + 1.
extern "C" void __cdecl CommuHiLo_OpenMusic(void) {
    if (SH_CALL(File_LoadDone)() == 0) return;
    SH_CALL(Music_Play)(0x94, 8);
    const auto phase = static_cast<unsigned char>(B(at::kPhase) + 1);
    B(at::kState) = 0;
    B(at::kPhase) = phase;
}

// original 0x459FB0 (0xE bytes): CommuHiLo_Phases[1] - jmp
// [CommuHiLo_PlayStates + 0x939A40 * 4] (13).
extern "C" void __cdecl CommuHiLo_PlayDispatch(void) {
    Dispatch("CommuHiLo_PlayDispatch (0x459FB0)", CommuHiLo_PlayStates, CommuHiLo_PlayStates_count, at::kState);
}

// original 0x459FC0 (0xB0 bytes): CommuHiLo_PlayStates[0] - step 0: 0x675F95
// = 4, Sound_PlayEffect(0x102), 0x939A3F + 1. Then 0x675F95 down one a frame,
// the money box at (0x6E - 0x28 n, 0x48) and the stake box at (0x6E - 0x28 n,
// 0x74, 1) for n the counter (re-read); at 0 the message 0x71, Field_Request
// = 2, the stake 0, 0x675F8C 0, 0x939A40 + 1, 0x939A3F 0.
extern "C" void __cdecl CommuHiLo_Intro(void) {
    if (B(at::kStep) == 0) {
        B(at::kSlide) = 4;
        Sound(0x102);
        B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
        return;
    }
    const auto n = static_cast<unsigned char>(B(at::kSlide) - 1);
    B(at::kSlide) = n;
    ZennyBox(0x6E - 0x28 * n, 0x48);
    StakeBox(0x6E - 0x28 * B(at::kSlide), 0x74, 1);
    if (B(at::kSlide) != 0) return;
    Message(0x71);
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    Field_Request = 2;
    SetL(at::kBet, 0);
    B(at::kCount) = 0;
    B(at::kState) = state;
    B(at::kStep) = 0;
}

// original 0x45A070 (0x27 bytes): CommuHiLo_PlayStates[1] - the money box at
// (0x6E, 0x48), the stake box at (0x6E, 0x74, 1); 0x939A40 + 1 once
// Field_Request is not 2.
extern "C" void __cdecl CommuHiLo_WaitIntro(void) {
    ZennyBox(0x6E, 0x48);
    StakeBox(0x6E, 0x74, 1);
    if (Request() != 2) B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
}

// original 0x45A0A0 (0xE bytes): CommuHiLo_PlayStates[2] - jmp
// [CommuHiLo_BetSteps + 0x939A3F * 4] (3).
extern "C" void __cdecl CommuHiLo_BetDispatch(void) {
    Dispatch("CommuHiLo_BetDispatch (0x45A0A0)", CommuHiLo_BetSteps, CommuHiLo_BetSteps_count, at::kStep);
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
    const int low = static_cast<signed char>(B(at::kBet));
    B(at::kDigits + 2) = static_cast<unsigned char>(low / 100);
    const int rest = static_cast<signed char>(static_cast<unsigned char>(low % 100));
    B(at::kDigits + 1) = static_cast<unsigned char>(rest / 10);
    B(at::kDigits) = static_cast<unsigned char>(rest % 10);
    const unsigned pressed = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000u);
    if ((pressed & 0xFFFF) != 0) {
        Sound(0x100);
        if (pressed & 0x8000) {
            const auto c = static_cast<unsigned char>(B(at::kCount) + 1);
            B(at::kCount) = c;
            if (static_cast<signed char>(c) >= 3) B(at::kCount) = 0;
        } else if (pressed & 0x2000) {
            const auto c = static_cast<unsigned char>(B(at::kCount) - 1);
            B(at::kCount) = c;
            if (static_cast<signed char>(c) < 0) B(at::kCount) = 2;
        } else if (pressed & 0x4000) {
            const int i = S8(at::kCount);
            Room("CommuHiLo_BetInput (0x45A0B0)", "the digit cursor 0x675F8C", i, 0, 2);
            const auto d = static_cast<unsigned char>(B(at::kDigits + i) - 1);
            B(at::kDigits + i) = d;
            if (static_cast<signed char>(d) < 0) B(at::kDigits + i) = 9;
        } else if (pressed & 0x1000) {
            const int i = S8(at::kCount);
            Room("CommuHiLo_BetInput (0x45A0B0)", "the digit cursor 0x675F8C", i, 0, 2);
            const auto d = static_cast<unsigned char>(B(at::kDigits + i) + 1);
            B(at::kDigits + i) = d;
            if (static_cast<signed char>(d) > 9) B(at::kDigits + i) = 0;
        }
        const int stake = S8(at::kDigits + 2) * 100 + S8(at::kDigits + 1) * 10 + S8(at::kDigits);
        SetL(at::kBet, static_cast<U>(stake));
        if (stake < 0) {
            SetL(at::kBet, 0);
        } else {
            int v = stake;
            if (v > 100) {
                v = 100;
                SetL(at::kBet, 100);
            }
            const U zenny = L(bof3::addr::Party_Zenny);
            if (static_cast<U>(v) > zenny) SetL(at::kBet, static_cast<U>(static_cast<int>(static_cast<std::int16_t>(zenny))));
        }
    } else {
        const unsigned buttons = Input_Pressed;
        bool back = false;
        if (Field_ConfirmButtons & buttons) {
            Sound(0x102);
            if (L(at::kBet) == 0) {
                back = true;
            } else {
                B(at::kSlide) = 4;
                Sound(0x104);
                const U stake = L(at::kBet);
                const U zenny = L(bof3::addr::Party_Zenny);
                const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
                SetL(bof3::addr::Party_Zenny, zenny - stake);
                B(at::kStep) = step;
            }
        } else if (Field_CancelButtons & buttons) {
            Sound(0x102);
            back = true;
        }
        if (back) {
            const auto step = static_cast<unsigned char>(B(at::kStep) + 2);
            B(at::kSlide) = 0;
            B(at::kStep) = step;
        }
    }
    if (B(at::kPhase) != 1 || B(at::kState) != 2) return;
    ZennyBox(0x6E, 0x48);
    StakeBox(0x6E, 0x74, static_cast<unsigned char>(B(at::kCount) + 1));
    SH_CALL(Commu_DrawUnderline)(0xBA - 12 * S8(at::kCount), 0x99);
}

// original 0x45A300 (0x75 bytes): CommuHiLo_BetSteps[1] - 0x675F95 down one;
// the money box at (0x12 n + 0x26, 0xE n + 0x10) and the stake box at (0xE
// (0xC - n), 0x19 n + 0x10, 0), n the counter (re-read); at 0 0x939A3F = 0,
// 0x939A40 + 1.
extern "C" void __cdecl CommuHiLo_BetSlide(void) {
    const auto n = static_cast<unsigned char>(B(at::kSlide) - 1);
    B(at::kSlide) = n;
    ZennyBox(0x12 * n + 0x26, 0xE * n + 0x10);
    const int m = B(at::kSlide);
    StakeBox(0xE * (0xC - m), 0x19 * m + 0x10, 0);
    if (B(at::kSlide) != 0) return;
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(at::kStep) = 0;
    B(at::kState) = state;
}

// original 0x45B370 (0x2F bytes): the first game's quit - Sound_PlayEffect(0x106),
// the message 0x72, Field_Request = 2, 0x939A3E (read after) + 1, 0x939A40 = 0.
// Reached by CommuHiLo_BetBack's tail jmp (hidden in Commu_DrawCard's extent).
extern "C" void __cdecl CommuHiLo_Quit(void) {
    Sound(0x106);
    Message(0x72);
    const auto phase = static_cast<unsigned char>(B(at::kPhase) + 1);
    Field_Request = 2;
    B(at::kPhase) = phase;
    B(at::kState) = 0;
}

// original 0x45A380 (0x56 bytes): CommuHiLo_BetSteps[2] - 0x675F95 up one; the
// boxes at (0x6E - 0x28 n, 0x48) and (0x6E - 0x28 n, 0x74, 1); at 4 a tail jmp
// to CommuHiLo_Quit.
extern "C" void __cdecl CommuHiLo_BetBack(void) {
    const auto n = static_cast<unsigned char>(B(at::kSlide) + 1);
    B(at::kSlide) = n;
    ZennyBox(0x6E - 0x28 * n, 0x48);
    StakeBox(0x6E - 0x28 * B(at::kSlide), 0x74, 1);
    if (B(at::kSlide) == 4) SH_CALL(CommuHiLo_Quit)();
}

// original 0x45A3E0 (0xE bytes): CommuHiLo_PlayStates[3] - jmp
// [CommuHiLo_DealSteps + 0x939A3F * 4] (5).
extern "C" void __cdecl CommuHiLo_DealDispatch(void) {
    Dispatch("CommuHiLo_DealDispatch (0x45A3E0)", CommuHiLo_DealSteps, CommuHiLo_DealSteps_count, at::kStep);
}

// original 0x45A3F0 (0x62 bytes): CommuHiLo_DealSteps[0] - the two boxes; once
// Field_Request is not 2: the picks' bytes 0x675FA2..0x675FAB 0, the shuffle,
// Sound_PlayEffect(0x102), 0x675F95 = 3, 0x675F8C = 0, 0x939A3F + 1.
extern "C" void __cdecl CommuHiLo_DealStart(void) {
    HiLoBoxes();
    if (Request() == 2) return;
    SetL(at::kPicks + 1, 0);
    SetL(at::kPicks + 5, 0);
    SetWord(At(at::kPicks + 9), 0);
    SH_CALL(CommuHiLo_Shuffle)();
    Sound(0x102);
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    B(at::kSlide) = 3;
    B(at::kCount) = 0;
    B(at::kStep) = step;
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
    const auto n = static_cast<unsigned char>(B(at::kSlide) - 1);
    unsigned char count = B(at::kCount);
    B(at::kSlide) = n;
    if (static_cast<signed char>(count) > 0) {
        int i = 0;
        do {
            Card(32 * i + 0x10, 0x6C, 0xFF);
            count = B(at::kCount);
            ++i;
        } while (i < static_cast<signed char>(count));
    }
    const int c = static_cast<signed char>(count);
    Card((2 * c - B(at::kSlide) + 1) * 16, 0x6C, 0xFF);
    if (B(at::kSlide) != 0) return;
    const auto dealt = static_cast<unsigned char>(B(at::kCount) + 1);
    B(at::kCount) = dealt;
    if (dealt != 9) {
        Sound(0x102);
        B(at::kSlide) = 2;
        return;
    }
    B(at::kCursor) = 0;
    if ((Rnd() & 3) == 0) {
        const unsigned r = Rnd() & 0xF;
        unsigned char left = r < 8 ? 1 : (r < 0xE ? 2 : 3);
        do {
            const unsigned bit = Rnd() & 7;
            if ((static_cast<int>(S8(at::kCursor)) & (1 << bit)) == 0) {
                B(at::kCursor) = static_cast<unsigned char>(B(at::kCursor) | (1u << bit));
                --left;
            }
        } while (left != 0);
    }
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// The shown values (0x675F8D's bits 0..7, re-read each time) at (0x30 + 32 i,
// 0x6C): value i + 1.
static void ShownValues() {
    for (int i = 0; i < 8; ++i)
        if (static_cast<int>(S8(at::kCursor)) & (1 << i)) Card(32 * i + 0x30, 0x6C, B(at::kCards + 1 + i));
}

// original 0x45A590 (0x7B bytes): CommuHiLo_DealSteps[2] - the row (9, 1, 0),
// the shown values, the two boxes; the message 0x73, Field_Request = 2,
// 0x939A3F (read after) + 1.
extern "C" void __cdecl CommuHiLo_DealHints(void) {
    Row(9, 1, 0);
    ShownValues();
    HiLoBoxes();
    Message(0x73);
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    Field_Request = 2;
    B(at::kStep) = step;
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
    const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
    B(at::kSlide) = 4;
    B(at::kStep) = step;
}

// original 0x45A6A0 (0xCA bytes): CommuHiLo_DealSteps[4] - the row (9, 1, 0);
// 0x675F95 down one; the three choices sliding in at (0x58, 0x88, 0xB8; 0x44
// - 0x1E n, mode 0) with n re-read for each; the two boxes; at 0 0x675F8C =
// 0x675F8D = 0, 0x939A40 + 1, 0x939A3F = 0.
extern "C" void __cdecl CommuHiLo_DealChoices(void) {
    Row(9, 1, 0);
    const auto n = static_cast<unsigned char>(B(at::kSlide) - 1);
    B(at::kSlide) = n;
    Choice(0x58, 0x44 - 0x1E * n, 0, 0);
    Choice(0x88, 0x44 - 0x1E * B(at::kSlide), 1, 0);
    Choice(0xB8, 0x44 - 0x1E * B(at::kSlide), 2, 0);
    HiLoBoxes();
    if (B(at::kSlide) != 0) return;
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(at::kCount) = 0;
    B(at::kCursor) = 0;
    B(at::kState) = state;
    B(at::kStep) = 0;
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
        if (S8(at::kCount) < 8) {
            Sound(0x100);
            if (B(at::kCount) == 0) {
                B(at::kCursor) = static_cast<unsigned char>(B(at::kCursor) ^ 1);
                how = kSelect, selected = 2;
            } else if (pressed & 0x8000) {
                const auto c = static_cast<unsigned char>(B(at::kCursor) - 1);
                B(at::kCursor) = c;
                if (static_cast<signed char>(c) < 0) B(at::kCursor) = 2;
            } else if (pressed & 0x2000) {
                const auto c = static_cast<unsigned char>(B(at::kCursor) + 1);
                B(at::kCursor) = c;
                if (static_cast<signed char>(c) > 2) B(at::kCursor) = 0;
            }
        }
    } else {
        const unsigned buttons = Input_Pressed;
        if (Field_ConfirmButtons & buttons) {
            const unsigned char cursor = B(at::kCursor);
            if (cursor == 2) {
                Sound(0x104);
                Sound(0x102);
                const auto state = static_cast<unsigned char>(B(at::kState) + 1);
                B(at::kSlide) = 0;
                B(at::kState) = state;
            } else {
                const unsigned char n = B(at::kCount);
                if (static_cast<signed char>(n) < 8) {
                    const int i = static_cast<signed char>(n);
                    Room("CommuHiLo_Pick (0x45A770)", "the pick count 0x675F8C", i, 0, 7);
                    B(at::kPicks + 1 + i) = cursor;
                    B(at::kCount) = static_cast<unsigned char>(n + 1);
                    Sound(0x103);
                    if (B(at::kCount) == 8) {
                        B(at::kCursor) = 2;
                        how = kSelect, selected = 3;
                    }
                }
            }
        } else if (Field_CancelButtons & buttons) {
            if (B(at::kCount) != 0) {
                Sound(0x106);
                const auto n = static_cast<unsigned char>(B(at::kCount) - 1);
                B(at::kCount) = n;
                if (n == 0) {
                    if (B(at::kCursor) == 2) B(at::kCursor) = 0;
                    how = kSelect, selected = 2;
                }
            }
        }
    }
    if (how == kByCount) {
        const signed char n = S8(at::kCount);
        selected = n < 1 ? 2 : (n >= 8 ? 3 : 0xFF);
    }
    Choices(0x58, 0x44, selected);
    HiLoBoxes();
    Row(9, 1, B(at::kCount));
    const signed char n = S8(at::kCount);
    if (n < 8) SH_CALL(CommuHiLo_DrawMarker)((n + 2) * 32, 0x6E);
    SH_CALL(Menu_DrawHand)(S8(at::kCursor) * 48 + 0x5E, 0x4A, 0);
}

// original 0x45A980 (0xBE bytes): CommuHiLo_PlayStates[5] - the two boxes, the
// row (count + 1, 1, count); 0x675F95 up one; the backs past the count at (0x10
// + 32 i, 0x28 n + 0x6C) for i from count + 1 below 9, n re-read; the choices
// at (0x58, 0x44 - 0x28 n, 2); at 4 0x675F95 = 0x1E, the turn 0x675F8E = 1,
// 0x939A40 + 1.
extern "C" void __cdecl CommuHiLo_PickClose(void) {
    HiLoBoxes();
    const unsigned char count = B(at::kCount);
    Row(static_cast<unsigned char>(count + 1), 1, count);
    int i = S8(at::kCount);
    B(at::kSlide) = static_cast<unsigned char>(B(at::kSlide) + 1);
    for (++i; i < 9; ++i) Card(32 * i + 0x10, 0x28 * B(at::kSlide) + 0x6C, 0xFF);
    Choices(0x58, 0x44 - 0x28 * B(at::kSlide), 2);
    if (B(at::kSlide) != 4) return;
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(at::kSlide) = 0x1E;
    B(at::kTurn) = 1;
    B(at::kState) = state;
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
    const auto n = static_cast<unsigned char>(B(at::kSlide) - 1);
    B(at::kSlide) = n;
    if (n == 0) {
        const int t = S8(at::kTurn);
        Room("CommuHiLo_Reveal (0x45AA40)", "the turn 0x675F8E", t, 1, 8);
        const unsigned char higher = B(at::kCards + t - 1) >= B(at::kCards + t) ? 1 : 0;
        if (B(at::kPicks + t) == higher) {
            Sound(0x104);
            const auto turn = static_cast<unsigned char>(B(at::kTurn) + 1);
            const unsigned char count = B(at::kCount);
            B(at::kTurn) = turn;
            if (static_cast<signed char>(turn) == static_cast<signed char>(count) + 1) {
                const auto back = static_cast<unsigned char>(B(at::kTurn) - 1);
                const auto state = static_cast<unsigned char>(B(at::kState) + 1);
                B(at::kTurn) = back;
                B(at::kLost) = 0;
                B(at::kSlide) = 0x3C;
                B(at::kState) = state;
            } else {
                B(at::kSlide) = 0x1E;
            }
        } else {
            Sound(0x107);
            const auto state = static_cast<unsigned char>(B(at::kState) + 1);
            B(at::kLost) = 1;
            B(at::kSlide) = 0x3C;
            B(at::kState) = state;
        }
    }
    const unsigned char count = B(at::kCount);
    Row(static_cast<unsigned char>(count + 1), B(at::kTurn), count);
}

// original 0x45AB30 (0x54 bytes): CommuHiLo_PlayStates[7] - the two boxes, the
// row (count + 1, count + 1, count); 0x675F95 down one, at 0 Sound 0x102 and
// 0x939A40 + 1.
extern "C" void __cdecl CommuHiLo_RevealPause(void) {
    HiLoBoxes();
    const unsigned char count = B(at::kCount);
    const auto all = static_cast<unsigned char>(count + 1);
    Row(all, all, count);
    const auto n = static_cast<unsigned char>(B(at::kSlide) - 1);
    B(at::kSlide) = n;
    if (n != 0) return;
    Sound(0x102);
    B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
}

// original 0x45AB90 (0xDC bytes): CommuHiLo_PlayStates[8] - 0x675F95 up one;
// values 0..count (re-read, signed) at (0x10 + 32 i, 0x28 n + 0x6C); at 4: lost
// - the stake 0 and the message 0x74; else the stake = (factor * stake + 50) /
// 100 (32 bits, signed; the factor the u16 0x652CF2 + 2 * turn) and the message
// 0x76; Field_Request = 2, 0x939A40 + 1. Last the two boxes. Ours aborts on a
// value index past 8 or a turn outside 1..8.
extern "C" void __cdecl CommuHiLo_Payout(void) {
    const auto n = static_cast<unsigned char>(B(at::kSlide) + 1);
    const unsigned char count = B(at::kCount);
    B(at::kSlide) = n;
    if (static_cast<signed char>(count) >= 0) {
        int i = 0;
        do {
            Room("CommuHiLo_Payout (0x45AB90)", "the value", i, 0, 8);
            const unsigned char v = B(at::kCards + i);
            Card(32 * i + 0x10, 0x28 * B(at::kSlide) + 0x6C, v);
            ++i;
        } while (i <= S8(at::kCount));
    }
    if (B(at::kSlide) == 4) {
        unsigned message;
        if (B(at::kLost) != 0) {
            SetL(at::kBet, 0);
            message = 0x74;
        } else {
            const int t = S8(at::kTurn);
            Room("CommuHiLo_Payout (0x45AB90)", "the turn 0x675F8E", t, 1, 8);
            const U factor = Word(At(at::kPayouts + 2 * t));
            const auto product = static_cast<std::int32_t>(factor * L(at::kBet) + 0x32u);
            SetL(at::kBet, static_cast<U>(product / 100));
            message = 0x76;
        }
        Message(message);
        const auto state = static_cast<unsigned char>(B(at::kState) + 1);
        Field_Request = 2;
        B(at::kState) = state;
    }
    HiLoBoxes();
}

// original 0x45AC70 (0x67 bytes): CommuHiLo_PlayStates[9] - once Field_Request
// is not 2: winnings of 10000 or more (signed) - the message 0x79, Field_Request
// = 2, 0x939A40 + 1; else the message 0x78, Field_Request = 2, 0x939A40 = 3.
// Then the two boxes.
extern "C" void __cdecl CommuHiLo_CheckWinnings(void) {
    if (Request() != 2) {
        if (static_cast<std::int32_t>(L(at::kBet)) >= 10000) {
            Message(0x79);
            const auto state = static_cast<unsigned char>(B(at::kState) + 1);
            Field_Request = 2;
            B(at::kState) = state;
        } else {
            Message(0x78);
            Field_Request = 2;
            B(at::kState) = 3;
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
        SH_CALL(Zenny_Add)(L(at::kBet), 0);
        Message(0x75);
        SetL(at::kBet, 0);
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
    if (B(at::kStep) == 0) {
        HiLoBoxes();
        Sound(0x102);
        const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
        B(at::kSlide) = 4;
        B(at::kStep) = step;
        return;
    }
    const auto n = static_cast<unsigned char>(B(at::kSlide) - 1);
    B(at::kSlide) = n;
    ZennyBox(0x6E - 0x12 * n, 0x48 - 0xE * n);
    const int m = B(at::kSlide);
    StakeBox(0xF * m + 0x6E, 0x74 - 0x19 * m, 0);
    if (B(at::kSlide) != 0) return;
    Message(0x71);
    Field_Request = 2;
    SetL(at::kBet, 0);
    B(at::kCount) = 0;
    B(at::kState) = 1;
    B(at::kStep) = 0;
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
    if (B(at::kStep) == 0) {
        HiLoBoxes();
        Sound(0x102);
        const auto step = static_cast<unsigned char>(B(at::kStep) + 1);
        B(at::kSlide) = 0;
        B(at::kStep) = step;
        return;
    }
    const auto n = static_cast<unsigned char>(B(at::kSlide) + 1);
    B(at::kSlide) = n;
    ZennyBox(0x26, 0x10 - 0x14 * n);
    StakeBox(0xA8, 0x10 - 0x14 * B(at::kSlide), 0);
    if (B(at::kSlide) != 4) return;
    B(at::kPhase) = 2;
    B(at::kState) = 0;
}

// original 0x45AF00 (0x54 bytes): CommuHiLo_Phases[2] - the close phase
// (music back, the field hook's tail state + 1).
extern "C" void __cdecl CommuHiLo_Close(void) { ClosePhase(); }

// ==== the second game: CommuHitBlow ========================================================

// The panels the second game draws under its boxes, in the original's orders.
namespace {
void HitBlowPanels(unsigned panel, unsigned show, unsigned row_flag) {
    GuessPanel(0x10, 0x18, panel);
    Secret(0x68, 0x40, show);
    GuessRow(0x68, 0x78, B(at::kCount), row_flag);
    HitBlowBoxes();
}
}  // namespace

// original 0x45B770 (0xE bytes): CommuGame[1] (R4B's 0x652A84) - jmp
// [CommuHitBlow_Phases + 0x939A3E * 4] (3).
extern "C" void __cdecl CommuHitBlow_Run(void) {
    Dispatch("CommuHitBlow_Run (0x45B770)", CommuHitBlow_Phases, CommuHitBlow_Phases_count, at::kPhase);
}

// original 0x45B780 (0xE bytes): CommuHitBlow_Phases[0] - jmp
// [CommuHitBlow_OpenStates + 0x939A40 * 4] (4).
extern "C" void __cdecl CommuHitBlow_OpenDispatch(void) {
    Dispatch("CommuHitBlow_OpenDispatch (0x45B780)", CommuHitBlow_OpenStates, CommuHitBlow_OpenStates_count,
             at::kState);
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
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    Field_Request = 2;
    B(at::kState) = state;
}

// original 0x45B800 (0x34 bytes): CommuHitBlow_OpenStates[2] - once
// Field_Request is not 2: Sound 0x102, 0x675F95 = 4, the stake 0, 0x939A40 + 1.
extern "C" void __cdecl CommuHitBlow_OpenWait(void) {
    if (Request() == 2) return;
    Sound(0x102);
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(at::kSlide) = 4;
    SetL(at::kBet, 0);
    B(at::kState) = state;
}

// original 0x45B840 (0xA9 bytes): CommuHitBlow_OpenStates[3] - 0x675F95 down
// one; the guess panel at (0x10 - 0x1E n, 0x18, 0), the money box at (0x1E n
// + 0xCC, 0x10), the stake box at (0x1E n + 0xCC, 0x3C, 0), a back at (0x68,
// 0x40 - 0x1E n), n re-read for each; at 0 0x939A40 = 0, 0x939A3E + 1.
extern "C" void __cdecl CommuHitBlow_OpenSlide(void) {
    const auto n = static_cast<unsigned char>(B(at::kSlide) - 1);
    B(at::kSlide) = n;
    GuessPanel(0x10 - 0x1E * n, 0x18, 0);
    ZennyBox(0x1E * B(at::kSlide) + 0xCC, 0x10);
    StakeBox(0x1E * B(at::kSlide) + 0xCC, 0x3C, 0);
    Card(0x68, 0x40 - 0x1E * B(at::kSlide), 0xFF);
    if (B(at::kSlide) != 0) return;
    const auto phase = static_cast<unsigned char>(B(at::kPhase) + 1);
    B(at::kState) = 0;
    B(at::kPhase) = phase;
}

// original 0x45B8F0 (0xE bytes): CommuHitBlow_Phases[1] - jmp
// [CommuHitBlow_PlayStates + 0x939A40 * 4] (9).
extern "C" void __cdecl CommuHitBlow_PlayDispatch(void) {
    Dispatch("CommuHitBlow_PlayDispatch (0x45B8F0)", CommuHitBlow_PlayStates, CommuHitBlow_PlayStates_count,
             at::kState);
}

// original 0x45B900 (0xE bytes): CommuHitBlow_PlayStates[0] - jmp
// [CommuHitBlow_StartSteps + 0x939A3F * 4] (2).
extern "C" void __cdecl CommuHitBlow_StartDispatch(void) {
    Dispatch("CommuHitBlow_StartDispatch (0x45B900)", CommuHitBlow_StartSteps, CommuHitBlow_StartSteps_count,
             at::kStep);
}

// original 0x45B910 (0xBB bytes): CommuHitBlow_StartSteps[0] - the stake 500 and
// Party_Zenny - 500 (unchecked); three distinct digits 0x675F98.. by
// Commu_RandDigit(9) (each stored, then drawn again while an earlier one has
// it); the eight guess records 0x675F9B.. (40 bytes) all 1; the row 0; the
// guess panel (0x10, 0x18, 0), a back at (0x68, 0x40), the money box (0xCC,
// 0x10), the stake box (0xCC, 0x3C, 0); 0x675F95 = 4, Sound 0x102, 0x939A3F
// (read after) + 1.
extern "C" void __cdecl CommuHitBlow_Start(void) {
    const U zenny = L(bof3::addr::Party_Zenny);
    SetL(at::kBet, 500);
    SetL(bof3::addr::Party_Zenny, zenny - 500u);
    for (int i = 0; i < 3;) {
        const auto v = static_cast<unsigned char>(SH_CALL(Commu_RandDigit)(9));
        B(at::kCards + i) = v;
        if (i != 0) {
            int j = 0;
            while (j < i && B(at::kCards + j) != v) ++j;
            if (j != i) continue;
        }
        ++i;
    }
    std::memset(At(at::kRecords), 1, 40);
    B(at::kCount) = 0;
    GuessPanel(0x10, 0x18, 0);
    Card(0x68, 0x40, 0xFF);
    ZennyBox(0xCC, 0x10);
    StakeBox(0xCC, 0x3C, 0);
    B(at::kSlide) = 4;
    Sound(0x102);
    B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
}

// original 0x45B9D0 (0x120 bytes): CommuHitBlow_StartSteps[1] - 0x675F95 down
// one; the guess panel (0x10, 0x18, 0), the money and stake boxes at 0xCC;
// three cards 1 at (8 (0x15 - n), 8 (0x11 - n), 0x68; 0x78 - 0xE n) and three
// backs at (8 (0x15 - n), 8 (0x11 - n), 0x68; 0x40), n re-read for each; at 0
// 0x939A3F = 0, 0x939A40 + 1.
extern "C" void __cdecl CommuHitBlow_StartSlide(void) {
    B(at::kSlide) = static_cast<unsigned char>(B(at::kSlide) - 1);
    GuessPanel(0x10, 0x18, 0);
    HitBlowBoxes();
    int n = B(at::kSlide);
    Card(8 * (0x15 - n), 0x78 - 0xE * n, 1);
    n = B(at::kSlide);
    Card(8 * (0x11 - n), 0x78 - 0xE * n, 1);
    n = B(at::kSlide);
    Card(0x68, 0x78 - 0xE * n, 1);
    Card(8 * (0x15 - B(at::kSlide)), 0x40, 0xFF);
    Card(8 * (0x11 - B(at::kSlide)), 0x40, 0xFF);
    Card(0x68, 0x40, 0xFF);
    if (B(at::kSlide) != 0) return;
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    B(at::kStep) = 0;
    B(at::kState) = state;
}

// original 0x45BAF0 (0x64 bytes): CommuHitBlow_PlayStates[1] - the message 0x80,
// Field_Request = 2, 0x939A40 (read after) + 1; the panels (0, 0, row 0).
extern "C" void __cdecl CommuHitBlow_Prompt(void) {
    Message(0x80);
    const auto state = static_cast<unsigned char>(B(at::kState) + 1);
    Field_Request = 2;
    B(at::kState) = state;
    HitBlowPanels(0, 0, 0);
}

// original 0x45BB60 (0x61 bytes): CommuHitBlow_PlayStates[2] - once
// Field_Request is not 2: the cursor 0x675F8D 0, 0x939A40 + 1; the panels (0,
// 0, row 0).
extern "C" void __cdecl CommuHitBlow_WaitPrompt(void) {
    if (Request() != 2) {
        const auto state = static_cast<unsigned char>(B(at::kState) + 1);
        B(at::kCursor) = 0;
        B(at::kState) = state;
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
        const unsigned char row = B(at::kCount);
        const unsigned char cursor = B(at::kCursor);
        const auto index = static_cast<unsigned char>(row * 5 + cursor + 3);
        if (pressed & (0x4000 | 0x1000)) {
            Room("CommuHitBlow_Input (0x45BBD0)", "the row 0x675F8C", static_cast<signed char>(row), 0, 7);
            Room("CommuHitBlow_Input (0x45BBD0)", "the cursor 0x675F8D", static_cast<signed char>(cursor), 0, 2);
        }
        if (pressed & 0x4000) {
            const auto d = static_cast<unsigned char>(B(at::kCards + index) - 1);
            B(at::kCards + index) = d;
            if (static_cast<signed char>(d) < 1) B(at::kCards + index) = 9;
        } else if (pressed & 0x1000) {
            const auto d = static_cast<unsigned char>(B(at::kCards + index) + 1);
            B(at::kCards + index) = d;
            if (d > 9) B(at::kCards + index) = 1;
        } else if (pressed & 0x2000) {
            const auto c = static_cast<unsigned char>(cursor + 1);
            B(at::kCursor) = c;
            if (static_cast<signed char>(c) > 2) B(at::kCursor) = 0;
        } else if (pressed & 0x8000) {
            const auto c = static_cast<unsigned char>(cursor - 1);
            B(at::kCursor) = c;
            if (static_cast<signed char>(c) < 0) B(at::kCursor) = 2;
        }
    } else if (Field_ConfirmButtons & Input_Pressed) {
        Sound(0x104);
        B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
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
    const int r = S8(at::kCount);
    Room("CommuHitBlow_Score (0x45BD20)", "the row 0x675F8C", r, 0, 7);
    unsigned char* const rec = At(at::kRecords + 5 * r);
    unsigned char hits = 0, blows = 0;
    for (int j = 0; j < 3; ++j)
        if (B(at::kCards + j) == rec[j]) ++hits;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (i != j && rec[i] == B(at::kCards + j)) ++blows;
    rec[3] = hits;
    rec[4] = blows;
    char* const text_a = reinterpret_cast<char*>(At(bof3::addr::Text_Records));
    const auto* const format = reinterpret_cast<const char*>(At(bof3::addr::Area08_MessageFormat));
    if (hits != 3) {
        SH_CALL(Crt_sprintf)(text_a, format, static_cast<unsigned>(hits));
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kTextB)), format, static_cast<unsigned>(blows));
        Message(0x81);
        B(at::kState) = static_cast<unsigned char>(B(at::kState) + 1);
    } else {
        SH_CALL(Crt_sprintf)(text_a, format, r + 1);
        unsigned char rank = static_cast<unsigned char>(Rnd() & 3);
        B(at::kCursor) = rank;
        if (static_cast<signed char>(rank) > 2) {
            const unsigned second = Rnd();
            auto v = static_cast<unsigned char>(B(at::kCursor) - (second & 3));
            if (static_cast<signed char>(v) < 0) v = 0;
            else if (static_cast<signed char>(v) > 2) v = 2;
            B(at::kCursor) = v;
            rank = v;
        }
        const int prize = S8(at::kCount) * 3 + static_cast<signed char>(rank);
        Room("CommuHitBlow_Score (0x45BD20)", "the prize", prize, 0, at::kPrizesCount - 1);
        const unsigned item = B(at::kPrizes + 2 * prize);
        const unsigned category = B(at::kPrizes + 2 * prize + 1);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
        std::memcpy(At(at::kTextB), name, 16);
        Message(0x82);
        B(at::kState) = 7;
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
        GuessRow(0x68, 0x78, B(at::kCount), 0);
    } else {
        const auto row = static_cast<unsigned char>(B(at::kCount) + 1);
        B(at::kCount) = row;
        if (static_cast<signed char>(row) < 8) {
            const int r = static_cast<signed char>(row);
            Room("CommuHitBlow_NextGuess (0x45BEE0)", "the new row 0x675F8C", r, 1, 7);
            unsigned char* const rec = At(at::kRecords + 5 * r);
            for (int j = 0; j < 3; ++j) rec[j] = rec[j - 5];
            B(at::kState) = 1;
        } else {
            Message(0x85);
            const auto state = static_cast<unsigned char>(B(at::kState) + 1);
            Field_Request = 2;
            B(at::kState) = state;
        }
        GuessPanel(0x10, 0x18, 0);
        GuessRow(0x68, 0x78, static_cast<unsigned char>(B(at::kCount) - 1), 0);
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
        B(at::kState) = 8;
    }
    GuessPanel(0x10, 0x18, 1);
    Secret(0x68, 0x40, 1);
    const unsigned char row = B(at::kCount);
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
        const int prize = S8(at::kCount) * 3 + S8(at::kCursor);
        Room("CommuHitBlow_Prize (0x45C020)", "the prize", prize, 0, at::kPrizesCount - 1);
        const unsigned item = B(at::kPrizes + 2 * prize);
        const unsigned category = B(at::kPrizes + 2 * prize + 1);
        const unsigned char added = SH_CALL(Inventory_Add)(category, item, 1);
        if (added == 0) {
            Message(0x83);
            Field_Request = 2;
        }
        B(at::kState) = 6;
    }
    GuessPanel(0x10, 0x18, 1);
    Secret(0x68, 0x40, 1);
    GuessRow(0x68, 0x78, B(at::kCount), 0);
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
    const unsigned char count = B(at::kCount);
    const unsigned char r = count == 8 ? 7 : count;
    if (Request() == 2) {
        Secret(0x68, 0x40, 1);
        GuessRow(0x68, 0x78, r, 0);
        GuessPanel(0x10, 0x18, 1);
        HitBlowBoxes();
        return;
    }
    const unsigned char step = B(at::kStep);
    if (step == 0) {
        B(at::kSlide) = 0;
        Secret(0x68, 0x40, 1);
        GuessRow(0x68, 0x78, r, 0);
        GuessPanel(0x10, 0x18, 1);
        HitBlowBoxes();
        B(at::kStep) = static_cast<unsigned char>(B(at::kStep) + 1);
        return;
    }
    if (step == 1) {
        const auto n = static_cast<unsigned char>(B(at::kSlide) + 1);
        B(at::kSlide) = n;
        Room("CommuHitBlow_End (0x45C0C0)", "the row", r, 0, 7);
        const U rec = at::kRecords + 5 * r;
        Card(0xA8 - 16 * n, 0x78 - 0xE * n, B(rec + 2));
        int m = B(at::kSlide);
        Card(8 * (0x11 - m), 0x78 - 0xE * m, B(rec + 1));
        const unsigned char first = B(rec);
        m = B(at::kSlide);
        Card(0x68, 0x78 - 0xE * m, first);
        Card(0xA8 - 16 * B(at::kSlide), 0x40, B(at::kCards + 2));
        Card(8 * (0x11 - B(at::kSlide)), 0x40, B(at::kCards + 1));
        Card(0x68, 0x40, B(at::kCards));
        GuessPanel(0x10, 0x18, 1);
        HitBlowBoxes();
        if (B(at::kSlide) != 4) return;
        if (B(at::kAgain) == 0) {
            B(at::kState) = 0;
            B(at::kStep) = 0;
        } else {
            Sound(0x102);
            B(at::kSlide) = 0;
            B(at::kStep) = 2;
        }
        return;
    }
    const auto n = static_cast<unsigned char>(B(at::kSlide) + 1);
    B(at::kSlide) = n;
    GuessPanel(0x10 - 0x1E * n, 0x18, 0);
    ZennyBox(0x1E * B(at::kSlide) + 0xCC, 0x10);
    StakeBox(0x1E * B(at::kSlide) + 0xCC, 0x3C, 0);
    Card(0x68, 0x40 - 0x1E * B(at::kSlide), 0xFF);
    if (B(at::kSlide) != 5) return;
    B(at::kState) = 0;
    B(at::kStep) = 0;
    B(at::kAgain) = 0;
    B(at::kPhase) = 2;
}

// original 0x45C3A0 (0x54 bytes): CommuHitBlow_Phases[2] - the close phase, as
// CommuHiLo_Close.
extern "C" void __cdecl CommuHitBlow_Close(void) { ClosePhase(); }

void Rest4C_Inject() {
    if (bof3::WantsShadow("rest_4c")) rest_4c::SelfTest();
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
