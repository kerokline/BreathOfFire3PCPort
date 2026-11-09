// Moved here by tools/move_funcs.py from magic_s12.cpp (docs/source-layout-round.md).
#include "game/magic/identify.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/magic_harness.h"
#include "game/magic_s12.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

// The target's sprite (the harness's kSource): what the tint and the
// position come from.
// kTargetSprite: now BattleOdds_ActorObject (symbols.toml).
// A bit per enemy kind (the enemy record's byte +0x8C): set once identified.
constexpr ::bof3::image::Va kSeenBits{0x9040A8};

// The panel's texts: two in the engine's .data the originals centre, a label
// beside the second number, and MAGIC060's own: a label beside the first
// number, one mark, a row of marks, the number format.
constexpr ::bof3::image::Va kText1{0x66A3E0};

constexpr ::bof3::image::Va kText2{0x66A3E8};

constexpr ::bof3::image::Va kLabel2{0x66A31C};

constexpr ::bof3::image::Va kLabel1{0x65AAB0};

constexpr ::bof3::image::Va kMark{0x65AAC4};

constexpr ::bof3::image::Va kMarks{0x65AAC8};

constexpr ::bof3::image::Va kNumberFormat{0x65AB08};

// Six pointers to the elements' two-byte glyph codes.
constexpr ::bof3::image::Va kElementGlyphs{0x66A398};

unsigned char* SC() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char* Slot(unsigned k) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (k & 0xFFu) * at::kTaskStride); }

std::int32_t Ptr(const void* p) { return static_cast<std::int32_t>(Low32(p)); }

std::uint32_t U32(const void* p) { return Low32(p); }

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// The records as the originals index them, unchecked: a member by its index
// (stride 0x14C), an enemy by its index (stride 0x128) - the battle index
// less 3, which a party index sends below the enemy records.
unsigned char* PartyRec(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRec(int i) { return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(i) * at::kEnemyStride); }

// The scratch words SharedScratch + k and the vertex scratch
// Prim_VertexScratch + k (SVECTORs at +0, +8, +0x10, +0x18).
constexpr ::bof3::image::Va kDs{SharedScratch_addr};

std::int32_t Dd(unsigned k) { return Long(Mem(::bof3::Img(kDs.v) + k)); }

void SetDd(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kDs.v) + k), static_cast<std::int32_t>(v)); }

unsigned char DsByte(unsigned k) { return Mem(::bof3::Img(kDs.v) + k)[0]; }

// `imul` (a 32-bit product that wraps) then `sar n`; `shl l` then `sar r`.
int MulSar(int a, int b, int n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// A .data dispatch table read in place: the cell called, the index checked
// (the originals' is not).
void CallCell(const unsigned long* table, unsigned entries, unsigned index, const char* who) {
    if (index >= entries) bof3::Fatal("%s: index %u, past the %u-entry table", who, index, entries);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

// A stack table's handler by the phase +1.
void CallPhase(const std::uint32_t* phases, unsigned entries, const char* who) {
    const unsigned phase = SC()[1];
    if (phase >= entries) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
    magic_harness::Phase(phases[phase])();
}

// This group's functions called directly, by their addresses as the originals
// call them: in the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using FnXY = void (__cdecl*)(int, int);

using SmallFn = const unsigned char* (__cdecl*)(int, int, unsigned, unsigned, const unsigned char*);   // Text_DrawSmall

using BoolFn = unsigned char (__cdecl*)();

using ItemFn = const unsigned char* (__cdecl*)(unsigned, unsigned, int);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// A draw-mode packet (tpage `tpage`, dithered) committed to `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

// The text calls. Text_DrawAt stores x and y as words, so the originals' x -
// 0xA0 less six times Text_CharCount's al, with its answer's upper half above
// it - is the same draw from the byte alone.
const unsigned char* Text(std::uint32_t address) { return Mem(address); }

const unsigned char* DrawAt(int x, int y, int count, const unsigned char* text) {
    return MH_CALL(Text_DrawAt)(x, y, 0, count, text);
}

unsigned CharCount(const unsigned char* text) { return MH_CALL(Text_CharCount)(text); }

// Centred on 0xA0 at y, `count` characters (the originals pass the count of
// the text itself, or 5 for a name).
void DrawCentred(const unsigned char* text, int y, int count) { DrawAt(0xA0 - 6 * static_cast<int>(CharCount(text)), y, count, text); }

// DIV-0064 group 15 (amended, DIVERGENCE DIV-0117): what the PlayStation
// draws in the small font where the port draws heading `i` (0 the weakness
// row's, 1 the items'), at the discs' own places - every disc's MAGIC060
// routine (US 0x801EF1E0..0x801EF2A0, its small draw 0x801EF76C):
// WEAK at (0x5A, 0x44) and POINT at (0x80, 0x44), the item word at
// (0x5A, 0x8A) - each the overlay's string, else the PC's own copy of the
// Japanese disc's word beside EXP (Labels_Slot). Before the amendment the
// item word was drawn at (0x59, y + 4), placed by eye.
void DrawSmallWords(unsigned i, int y) {
    const auto small = MH_AT(SmallFn, bof3::addr::Text_DrawSmall);
    if (i == 0) {
        small(0x5A, y, 0, 0x10, Labels_Slot(15, 2));
        small(0x80, y, 0, 0x10, Labels_Slot(15, 3));
    } else {
        small(0x5A, y, 0, 0x10, Labels_Slot(15, 4));
    }
}

// DIV-0064 group 15: heading `i` of the Identify panel (0 the weakness
// word, 1 the items word) - once the group is written, the discs' small
// words (above), then the overlay's string from Labels_Slot (a space, which
// draws nothing) centred on 0xA0 by the pen width it covers and drawn whole;
// else the shipped word at `address` centred by its character count, the
// count the draw's limit, as the originals have it. Answers what
// Text_DrawAt answers (Identify_DrawEnemy passes the upper half on).
const unsigned char* DrawHeading(unsigned i, std::uint32_t address, int y) {
    if (Labels_Written(15)) {
        DrawSmallWords(i, y);
        const unsigned char* const text = Labels_Slot(15, i);
        return DrawAt(0xA0 - static_cast<int>(TextAdvance_Width(text)) / 2, y, 0xFF, text);
    }
    const unsigned n = CharCount(Text(address)) & 0xFFu;
    return DrawAt(0xA0 - 6 * static_cast<int>(n), y, static_cast<int>(n), Text(address));
}

// The same with the character count already taken (Identify_DrawEnemy counts
// its first heading before reading the record).
// The names' draw count: the originals' 5 - five Chinese glyphs - or the
// eight bytes a name field holds once the overlay has written the panel's
// words (its names are one byte a letter: `Fly Man` was `Fly M`).
int NameCount() { return Labels_Written(15) ? 8 : 5; }

const unsigned char* DrawHeadingCounted(unsigned i, std::uint32_t address, int y, unsigned n) {
    if (Labels_Written(15)) {
        DrawSmallWords(i, y);
        const unsigned char* const text = Labels_Slot(15, i);
        return DrawAt(0xA0 - static_cast<int>(TextAdvance_Width(text)) / 2, y, 0xFF, text);
    }
    return DrawAt(0xA0 - 6 * static_cast<int>(n), y, static_cast<int>(n), Text(address));
}

// The EXP label at (0xB0, 0x64): the shipped 0x65AAB0 with the count 3, or
// once group 15 is written the overlay's word drawn whole (the German disc's
// EP; the others' EXP, the shipped word, when the overlay left it empty).
void DrawExpLabel() {
    if (Labels_Written(15)) {
        DrawAt(0xB0, 0x64, 0xFF, Labels_Slot(15, 5));
        return;
    }
    DrawAt(0xB0, 0x64, 3, Text(::bof3::Img(kLabel1.v)));
}

}  // namespace

#define MS12_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The panel: a member's for a target below 3, else an enemy's.
void DrawPanel() {
    if (TargetByte() < 3) {
        Call0(bof3::addr::Identify_DrawMember);
    } else {
        Call0(bof3::addr::Identify_DrawEnemy);
    }
}

// DIV-0128: the element row as the game's own 8 x 8 element icons (VRAM page
// 0xF, row 248, cell (icon + 7) * 8 - Menu_DrawIcon8's sheet; icons 0..5 are
// fire, ice, thunder, wind, earth and holy, the glyphs' order) under every
// language overlay but the Chinese original's, once the self-tests are over
// (so the fuzz still compares Capcom's glyph quads).
bool ElementIcons() { return Widescreen_FillsArmed() && (Lang_Latin() || Lang_Japanese()); }

// A POLY_FT4 of icon `i` stretched over the glyph's 12 x 12 cell, (x, y) to
// (right, y + 12), shade `shade` in all three channels (0x80 a weakness, 0x30
// the rest, as the glyphs); CLUT 0x780E for icons 1 and 3, 0x780C for the rest
// (Menu_DrawIcon8's choice); page 0xF; committed to slot 2 as the glyph is.
void DrawElementIcon(int x, int y, int right, unsigned i, unsigned char shade) {
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    p[4] = p[5] = p[6] = shade;
    const auto f = [](int v) { return static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(static_cast<float>(v))); };
    SetLong(p + 0x08, f(x));
    SetLong(p + 0x0C, f(y));
    SetLong(p + 0x18, f(right));
    SetLong(p + 0x1C, f(y));
    SetLong(p + 0x28, f(x));
    SetLong(p + 0x2C, f(y + 0xC));
    SetLong(p + 0x38, f(right));
    SetLong(p + 0x3C, f(y + 0xC));
    // The corners name the first and last texels: the renderer's coordinates
    // are texel centres (D3d_TexCoords, (i + 0.512) / 256), so u0 + 8 would
    // reach into the next icon and v 256 does not fit the byte.
    const auto u0 = static_cast<unsigned char>((i + 7) * 8), u1 = static_cast<unsigned char>(u0 + 7);
    constexpr unsigned char v0 = 0xF8, v1 = 0xFF;
    p[0x14] = u0; p[0x15] = v0;
    p[0x24] = u1; p[0x25] = v0;
    p[0x34] = u0; p[0x35] = v1;
    p[0x44] = u1; p[0x45] = v1;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, (i == 1 || i == 3) ? 0x780Eu : 0x780Cu);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, 0xF);
    MH_CALL(Gfx_CommitPrim)(2, 0x48);
}

}  // namespace

// original 0x4B0D50: the kind-2 task. Its phase +1 through a five-entry stack
// table; the original's is unchecked, ours aborts past it.
MS12_EXPORT void __cdecl Identify_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Identify_Start, bof3::addr::Identify_WaitOpen,
                                                 bof3::addr::Identify_ShowTimed, bof3::addr::Identify_ShowUntilInput,
                                                 bof3::addr::Identify_End};
    CallPhase(kPhases, 5, "Identify_Task");
}

// original 0x4B0D90: two children (kind 1, 0x12): the dim (+1 0) and the disc
// (+1 1), each owned by Sprite_Current read after its create; the roll; +0xB
// 0 and the phase on.
MS12_EXPORT void __cdecl Identify_Start(void) {
    {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x12));
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        tt->owner = Ptr(SC());
        t[1] = 0;
    }
    {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x12));
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        tt->owner = Ptr(SC());
        t[1] = 1;
    }
    Call0(bof3::addr::Identify_Roll);
    SC()[0xB] = 0;
    Bump(SC()[1]);
}

// original 0x4B0E00: once the disc has grown (+0xB 2), the third child (+1 2,
// the target's tint); the task's +9 0x10 and the phase on.
MS12_EXPORT void __cdecl Identify_WaitOpen(void) {
    if (SC()[0xB] != 2) return;
    unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x12));
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    unsigned char* const sc = SC();
    tt->owner = Ptr(sc);
    t[1] = 2;
    sc[9] = 0x10;
    Bump(SC()[1]);
}

// original 0x4B0E50: the panel; +9 down, at 0 the phase on.
MS12_EXPORT void __cdecl Identify_ShowTimed(void) {
    DrawPanel();
    Drop(SC()[9]);
    unsigned char* const sc = SC();
    if (sc[9] == 0) Bump(sc[1]);
}

// original 0x4B0E90: the panel; once Input_Pressed is not 0, +0xB 0x83 (the
// children close) and the phase on.
MS12_EXPORT void __cdecl Identify_ShowUntilInput(void) {
    DrawPanel();
    if (Input_Pressed == 0) return;
    SC()[0xB] = 0x83;
    Bump(SC()[1]);
}

// original 0x4B0ED0: once the three children have ended (+0xB 0x80), the
// effect-done bit and the task freed.
MS12_EXPORT void __cdecl Identify_End(void) {
    if (SC()[0xB] != 0x80) return;
    Mem(::bof3::Img(magic_harness::at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B0EF0: the roll. The chance by the acting member's byte +0x8A
// less the target enemy's word +0x98: 16 at 15 or more, 14 / 12 / 10 / 8 /
// 6 / 4 / 2 below 15 / 10 / 5 / 0 / -5 / -10 / -15; a hit when Rand & 0xF
// is below it. A kind seen before shows (+4 1); else a hit on an enemy whose
// +0x8F is not 0 (the target read again) marks the kind and shows; else +4 0.
MS12_EXPORT void __cdecl Identify_Roll(void) {
    const int enemy = static_cast<int>(rv::As<records::BattleEnemy>(EnemyRec(static_cast<int>(TargetByte()) - 3))->level);
    const int actor = PartyRec(Mem(::bof3::Img(at::kActor))[0])[0x8A];
    const int d = actor - enemy;
    int chance = 0x10;
    if (d < 15) chance = 0xE;
    if (d < 10) chance = 0xC;
    if (d < 5) chance = 0xA;
    if (d < 0) chance = 8;
    if (d < -5) chance = 6;
    if (d < -10) chance = 4;
    if (d < -15) chance = 2;
    const bool hit = (MH_CALL(Rand)() & 0xF) < chance;
    if (MH_AT(BoolFn, bof3::addr::Identify_WasSeen)() != 0) {
        SC()[4] = 1;
        return;
    }
    if (hit && EnemyRec(static_cast<int>(TargetByte()) - 3)[0x8F] != 0) {
        Call0(bof3::addr::Identify_MarkSeen);
        SC()[4] = 1;
        return;
    }
    SC()[4] = 0;
}

// original 0x4B0FF0: the target enemy's kind (+0x8C) as a bit of 0x9040A8.
MS12_EXPORT void __cdecl Identify_MarkSeen(void) {
    const unsigned kind = EnemyRec(static_cast<int>(TargetByte()) - 3)[0x8C];
    unsigned char* const word = Mem(::bof3::Img(kSeenBits.v) + 4 * (kind >> 5));
    SetLong(word, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(word)) | (1u << (kind & 31))));
}

// original 0x4B1040: that bit, 1 when set.
MS12_EXPORT unsigned char __cdecl Identify_WasSeen(void) {
    const unsigned kind = EnemyRec(static_cast<int>(TargetByte()) - 3)[0x8C];
    return (static_cast<std::uint32_t>(Long(Mem(::bof3::Img(kSeenBits.v) + 4 * (kind >> 5)))) & (1u << (kind & 31))) != 0 ? 1 : 0;
}

// original 0x4B1090: the panel for a party target. The member's name with
// its byte +0x89 zeroed for the draw (put back through the target byte read
// again), the text 0x66A3E0; question marks and the labels for every value;
// the text 0x66A3E8 and two rows of question marks for the items.
MS12_EXPORT void __cdecl Identify_DrawMember(void) {
    {
        unsigned char* const rec = PartyRec(TargetByte());
        const unsigned char kept = rec[0x89];
        rec[0x89] = 0;
        DrawCentred(rec + 0x80, 0x30, NameCount());
        PartyRec(TargetByte())[0x89] = kept;
    }
    DrawHeading(0, ::bof3::Img(kText1.v), 0x44);
    DrawAt(0x70, 0x50, 8, Text(::bof3::Img(kMarks.v)));
    DrawAt(0x9C, 0x64, 1, Text(::bof3::Img(kMark.v)));
    DrawExpLabel();
    DrawAt(0x9C, 0x78, 1, Text(::bof3::Img(kMark.v)));
    DrawAt(0xB0, 0x78, 1, Text(::bof3::Img(kLabel2.v)));
    DrawHeading(1, ::bof3::Img(kText2.v), 0x8A);
    DrawAt(0x70, 0x98, 8, Text(::bof3::Img(kMarks.v)));
    DrawAt(0x70, 0xAC, 8, Text(::bof3::Img(kMarks.v)));
}

// original 0x4B11F0: the panel for an enemy target (its record by the
// target byte - 3 as a byte, once). Its name and the text 0x66A3E0; for an
// enemy whose +0x8F is not 0 the six element glyphs, the words +0x96 and
// +0x94 printed with their labels, else question marks; the text 0x66A3E8;
// the two items +0xA8 and +0xAC, each word passed with the previous callee's
// upper half above it (the original loads it into ax) and its high byte as
// the category.
MS12_EXPORT void __cdecl Identify_DrawEnemy(void) {
    const unsigned char index = static_cast<unsigned char>(TargetByte() - 3);
    const unsigned text1 = CharCount(Text(::bof3::Img(kText1.v))) & 0xFFu;   // counted here, before the record is read, as the original
    unsigned char* const rec = EnemyRec(index);
    records::BattleEnemy* const trec = rv::As<records::BattleEnemy>(rec);
    if (rec[0x8F] != 0) {
        DrawCentred(rec + 0x80, 0x30, NameCount());
        DrawHeadingCounted(0, ::bof3::Img(kText1.v), 0x44, text1);
        MH_AT(FnXY, bof3::addr::Identify_DrawElements)(0x66, 0x54);
        char number[12];
        MH_CALL(Crt_sprintf)(number, reinterpret_cast<const char*>(Text(::bof3::Img(kNumberFormat.v))), static_cast<unsigned>(trec->exp));
        DrawAt(0x70, 0x64, 5, reinterpret_cast<const unsigned char*>(number));
        DrawExpLabel();
        MH_CALL(Crt_sprintf)(number, reinterpret_cast<const char*>(Text(::bof3::Img(kNumberFormat.v))), static_cast<unsigned>(trec->zenny));
        DrawAt(0x70, 0x78, 5, reinterpret_cast<const unsigned char*>(number));
        DrawAt(0xB0, 0x78, 1, Text(::bof3::Img(kLabel2.v)));
    } else {
        DrawCentred(rec + 0x80, 0x30, NameCount());
        DrawHeadingCounted(0, ::bof3::Img(kText1.v), 0x44, text1);
        DrawAt(0x70, 0x50, 8, Text(::bof3::Img(kMarks.v)));
        DrawAt(0x9C, 0x64, 1, Text(::bof3::Img(kMark.v)));
        DrawExpLabel();
        DrawAt(0x9C, 0x78, 1, Text(::bof3::Img(kMark.v)));
        DrawAt(0xB0, 0x78, 1, Text(::bof3::Img(kLabel2.v)));
    }
    const unsigned char* last = DrawHeading(1, ::bof3::Img(kText2.v), 0x8A);
    unsigned item = (U32(last) & 0xFFFF0000u) | Word(rec + 0xA8);
    last = MH_AT(ItemFn, bof3::addr::Identify_DrawItem)(item, (item >> 8) & 0xFFu, 0x98);
    item = (U32(last) & 0xFFFF0000u) | Word(rec + 0xAC);
    MH_AT(ItemFn, bof3::addr::Identify_DrawItem)(item, (item >> 8) & 0xFFu, 0xAC);
}

// original 0x4B1420: an item's name at y, centred: by the category's low
// byte from NameTable_Weapons (1), _Armour (2), _Accessories (3) or
// _Consumables (any other), by the item's low byte; its count the item
// argument with the character count over its low byte (the original stores
// al over the argument's slot and passes the dword). Question marks when +4
// (the roll) is 0. Answers what Text_DrawAt answers.
MS12_EXPORT const unsigned char* __cdecl Identify_DrawItem(unsigned item, unsigned category, int y) {
    if (SC()[4] == 0) return DrawAt(0x70, y, 8, Text(::bof3::Img(kMarks.v)));
    const unsigned i = item & 0xFFu;
    const unsigned char* name;
    switch (category & 0xFFu) {
    case 1: name = Mem(::bof3::Img(bof3::addr::NameTable_Weapons) + 28 * i); break;
    case 2: name = Mem(::bof3::Img(bof3::addr::NameTable_Armour) + 26 * i); break;
    case 3: name = Mem(::bof3::Img(bof3::addr::NameTable_Accessories) + 24 * i); break;
    default: name = Mem(::bof3::Img(bof3::addr::NameTable_Consumables) + 22 * i); break;
    }
    const unsigned n = CharCount(name) & 0xFFu;
    return DrawAt(0xA0 - 6 * static_cast<int>(n), y, static_cast<int>((item & 0xFFFFFF00u) | n), name);
}

// original 0x4B1520: six glyph quads 0x14 apart from (x, y), each 12 x 12:
// the glyph the pointer 0x66A398[i] holds, clut Gpu_GetClut(0, 0x1E0), shade
// 0x80 when the target enemy's (target - 3, once) resistance byte is 1 or
// less (the sixth, +0xC4: 3 or less), else 0x30 (the dword 0x903854 too);
// Gfx_CommitPrim(2, 0x28).
MS12_EXPORT void __cdecl Identify_DrawElements(int x, int y) {
    static constexpr unsigned kResist[6] = {0xBF, 0xC0, 0xC1, 0xC3, 0xC2, 0xC4};
    const int index = static_cast<int>(TargetByte()) - 3;
    int right = x + 0xC;
    for (unsigned i = 0; i < 6; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetCode6C)(p);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y0, static_cast<unsigned>(y));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y1, static_cast<unsigned>(y));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y2, static_cast<unsigned>(y + 0xC));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->y3, static_cast<unsigned>(y + 0xC));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x0, static_cast<unsigned>(x));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x1, static_cast<unsigned>(right));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x2, static_cast<unsigned>(x));
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->x3, static_cast<unsigned>(right));
        unsigned shade = 0x30;
        SetDd(4, 0x30);
        if (EnemyRec(index)[kResist[i]] <= (i == 5 ? 3 : 1)) {
            shade = 0x80;
            SetDd(4, 0x80);
        }
        if (ElementIcons()) {
            DrawElementIcon(x, y, right, i, static_cast<unsigned char>(shade));
            x += 0x14;
            right += 0x14;
            continue;
        }
        p[4] = static_cast<unsigned char>(shade);
        p[5] = DsByte(4);
        p[6] = DsByte(4);
        const unsigned char* const glyph = Pointer(::bof3::Img(kElementGlyphs.v) + 4 * i);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->glyph, (static_cast<unsigned>(glyph[0] & 0x7F) << 8) + glyph[1]);
        rv::Store<unsigned>(rv::As<records::GpuGlyph>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1E0));
        p[0xC] = 0;
        p[0xD] = 0;
        p[0x14] = 0xC;
        p[0x15] = 0;
        p[0x1C] = 0;
        p[0x1D] = 0xC;
        p[0x24] = 0xC;
        p[0x25] = 0xC;
        MH_CALL(Gfx_CommitPrim)(2, 0x28);
        x += 0x14;
        right += 0x14;
    }
}

// original 0x4B16C0: jmp [IdentifyChild_Kinds + 4 * +1].
MS12_EXPORT void __cdecl IdentifyChild_Task(void) { CallCell(IdentifyChild_Kinds, 3, SC()[1], "IdentifyChild_Task"); }

// original 0x4B16E0: the dim's step by +2; then while +0 and +2 are set, its
// draw.
MS12_EXPORT void __cdecl IdentifyDim_Run(void) {
    CallCell(IdentifyDim_Steps, 4, SC()[2], "IdentifyDim_Run");
    const unsigned char* const sc = SC();
    if (sc[0] == 0 || sc[2] == 0) return;
    Call0(bof3::addr::IdentifyDim_Draw);
}

// original 0x4B1710: +9 up; at 0x10 the owner's +0xB 1 and on.
MS12_EXPORT void __cdecl IdentifyDim_FadeIn(void) {
    Bump(SC()[9]);
    if (SC()[9] != 0x10) return;
    Owner()[0xB] = 1;
    Bump(SC()[2]);
}

// original 0x4B1740 (a child's last phase in sixteen files' tables): +9 down;
// at 0 the owner's +0xB down - one child fewer - and the task freed.
MS12_EXPORT void __cdecl MagicFx_CountDownRelease(void) {
    Drop(SC()[9]);
    if (SC()[9] != 0) return;
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B1770: the screen darkened: a semi-transparent tile over
// 320 x 240, grey +9, on layer 3 between two draw modes (tpage 0x55, then
// 0x15).
MS12_EXPORT void __cdecl IdentifyDim_Draw(void) {
    DrawModeCommit(0x55, 3);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetTile)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetLong(p + 0xC, 0);
    SetLong(p + 8, std::bit_cast<std::int32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    SetLong(p + 0x14, std::bit_cast<std::int32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetLong(p + 0x18, 0x43700000);   // 240.0f
    const unsigned grey = SC()[9];
    SetDd(4, grey);
    p[4] = static_cast<unsigned char>(grey);
    p[5] = DsByte(4);
    p[6] = DsByte(4);
    MH_CALL(Gfx_CommitPrim)(3, 0x1C);
    DrawModeCommit(0x15, 3);
}

// original 0x4B1810: the disc's step by +2; then while +0 and +2 are set,
// its draw.
MS12_EXPORT void __cdecl IdentifyDisc_Run(void) {
    CallCell(IdentifyDisc_Steps, 4, SC()[2], "IdentifyDisc_Run");
    const unsigned char* const sc = SC();
    if (sc[0] == 0 || sc[2] == 0) return;
    Call0(bof3::addr::IdentifyDisc_Draw);
}

// original 0x4B1840: once the dim is in (the owner's +0xB 1): +9 0 and on.
MS12_EXPORT void __cdecl IdentifyDisc_WaitDim(void) {
    if (Owner()[0xB] != 1) return;
    SC()[9] = 0;
    Bump(SC()[2]);
}

// original 0x4B1860: +9 up; at 0x1E the owner's +0xB 2 and on.
MS12_EXPORT void __cdecl IdentifyDisc_Grow(void) {
    Bump(SC()[9]);
    if (SC()[9] != 0x1E) return;
    Owner()[0xB] = 2;
    Bump(SC()[2]);
}

// original 0x4B1890: on once the owner's +0xB is 0x83 (a key was pressed).
MS12_EXPORT void __cdecl IdentifyFx_WaitClose(void) {
    if (Owner()[0xB] == 0x83) Bump(SC()[2]);
}

// original 0x4B18B0: +9 down by 2; at 0 the owner's +0xB down and the task
// freed (an odd +9 wraps through 0xFF).
MS12_EXPORT void __cdecl MagicFx_CountDown2Release(void) {
    SC()[9] = static_cast<unsigned char>(SC()[9] - 2);
    if (SC()[9] != 0) return;
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B18E0: a grey disc on screen: one semi-transparent POLY_G4
// round (0xA0, 0x78), radius 3 * (+9 + 6), its corners at the angles
// ((+9 + k) & 31) << 7 for k 6, 0xE, -2, -0xA (each through the dword
// 0x903858, read back for the cosine; the radius the dword 0x903850, read
// back at each use); grey (+9 / 3) * 6; on layer 3 between two draw modes.
MS12_EXPORT void __cdecl IdentifyDisc_Draw(void) {
    DrawModeCommit(0x55, 3);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    SetDd(0, 3u * (SC()[9] + 6u));
    static constexpr unsigned kCorner[4] = {6, 0xE, 0xFFFFFFFEu, 0xFFFFFFF6u};
    for (unsigned c = 0; c < 4; ++c) {
        const unsigned angle = ((SC()[9] + kCorner[c]) & 0x1F) << 7;
        SetDd(8, angle);
        int v = Sin(static_cast<int>(angle));
        PutFloat(p + 8 + 0x10 * c, MulSar(v, Dd(0), 12) + 0xA0);
        v = Cos(Dd(8));
        PutFloat(p + 0xC + 0x10 * c, MulSar(v, Dd(0), 12) + 0x78);
    }
    SetDd(4, (SC()[9] / 3u) * 6u);
    for (unsigned k : {4u, 0x14u, 0x24u, 0x34u}) {
        p[k] = DsByte(4);
        p[k + 1] = DsByte(4);
        p[k + 2] = DsByte(4);
    }
    MH_CALL(Gfx_CommitPrim)(3, 0x44);
    DrawModeCommit(0x15, 3);
}

// original 0x4B1B40: the tint's step: jmp [IdentifyTint_Steps + 4 * +2].
MS12_EXPORT void __cdecl IdentifyTint_Task(void) { CallCell(IdentifyTint_Steps, 3, SC()[2], "IdentifyTint_Task"); }

// original 0x4B1B60: the target's sprite's tint released and a black one
// taken (Sprite_SetTint(.., 0, 0, 0, 1), its record into +0xB); +9 0, on.
MS12_EXPORT void __cdecl IdentifyTint_Start(void) {
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    const unsigned char record = MH_CALL(Sprite_SetTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)), 0, 0, 0, 1);
    SC()[0xB] = record;
    SC()[9] = 0;
    Bump(SC()[2]);
}

// original 0x4B1BA0: on odd frames the tint record's colour bytes (+2..+4 of
// MoveScript_TintRecords[+0xB]) up one; at red 8, on.
MS12_EXPORT void __cdecl IdentifyTint_Brighten(void) {
    unsigned char* const sc = SC();
    unsigned char* const tints = MoveScript_TintRecords;
    if (Frame_Counter & 1) {
        Bump(tints[sc[0xB] * 12u + 2]);
        Bump(tints[sc[0xB] * 12u + 3]);
        Bump(tints[sc[0xB] * 12u + 4]);
    }
    if (tints[sc[0xB] * 12u + 2] == 8) Bump(sc[2]);
}

// original 0x4B1C10: on odd frames the colour bytes down one; at red 0 back to
// IdentifyTint_Brighten (+2 1). Once the owner's +0xB has bit 0x80 (closing):
// the tint released, the target flashed, the owner's +0xB down, freed.
MS12_EXPORT void __cdecl IdentifyTint_Dim(void) {
    unsigned char* const sc = SC();
    unsigned char* const tints = MoveScript_TintRecords;
    if (Frame_Counter & 1) {
        Drop(tints[sc[0xB] * 12u + 2]);
        Drop(tints[sc[0xB] * 12u + 3]);
        Drop(tints[sc[0xB] * 12u + 4]);
    }
    if (tints[sc[0xB] * 12u + 2] == 0) sc[2] = 1;
    if ((Owner()[0xB] & 0x80) == 0) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    MH_CALL(BattleActor_Flash)(TargetByte());
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

void Identify_Inject() {
    BOF3_INJECT(Identify_Task);
    BOF3_INJECT(Identify_Start);
    BOF3_INJECT(Identify_WaitOpen);
    BOF3_INJECT(Identify_ShowTimed);
    BOF3_INJECT(Identify_ShowUntilInput);
    BOF3_INJECT(Identify_End);
    BOF3_INJECT(Identify_Roll);
    BOF3_INJECT(Identify_MarkSeen);
    BOF3_INJECT(Identify_WasSeen);
    BOF3_INJECT(Identify_DrawMember);
    BOF3_INJECT(Identify_DrawEnemy);
    BOF3_INJECT(Identify_DrawItem);
    BOF3_INJECT(Identify_DrawElements);
    BOF3_INJECT(IdentifyChild_Task);
    BOF3_INJECT(IdentifyDim_Run);
    BOF3_INJECT(IdentifyDim_FadeIn);
    BOF3_INJECT(MagicFx_CountDownRelease);
    BOF3_INJECT(IdentifyDim_Draw);
    BOF3_INJECT(IdentifyDisc_Run);
    BOF3_INJECT(IdentifyDisc_WaitDim);
    BOF3_INJECT(IdentifyDisc_Grow);
    BOF3_INJECT(IdentifyFx_WaitClose);
    BOF3_INJECT(MagicFx_CountDown2Release);
    BOF3_INJECT(IdentifyDisc_Draw);
    BOF3_INJECT(IdentifyTint_Task);
    BOF3_INJECT(IdentifyTint_Start);
    BOF3_INJECT(IdentifyTint_Brighten);
    BOF3_INJECT(IdentifyTint_Dim);
}
