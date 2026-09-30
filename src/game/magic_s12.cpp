// Two spell overlays compiled into the exe, round nine group S12
// (docs/magic_s12.md): the PSX's MAGIC060 and MAGIC062.EMI, Magic_Rows rows
// 39 and 95. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Identify and Celerity; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC060 0x4B0D50..0x4B1CAA: a roll by the actor's and the target's
//     bytes, remembered as a bit per enemy kind (0x9040A8); three children
//     (kind 1, 0x12) - the screen dimmed, a grey disc grown on screen, the
//     target's tint pulsed - and a panel of text drawn over them: the
//     target's name, two numbers, six element glyphs and two item names for
//     an enemy it identified, question marks otherwise, first for 16 frames,
//     then until a key is pressed. The children count the parent's +0xB down
//     to 0x80 as they end.
//   - MAGIC062 0x4B1CB0..0x4B2F34: MAGIC082's buff (group S18's Buff_*) with
//     its own colours: a ring child and four sparks (kind 1, 0x4A), the
//     target's tint faded, four stats moved by the ability's step
//     (Celerity_ApplyStat) with a popup child each (kind 1, 0x48).
//
// Every call goes through the harness (MH_CALL / MH_AT, the .data tables read
// in place), so the start-up fuzz can stand recorders in for ours as for the
// originals' copies. No divergence: each is a faithful replacement, except
// that an index past a dispatch table aborts where the original would call
// through the bytes after it (docs/magic_fx_reached.md section 3, the
// precedent), and CeleritySpark_Draw aborts where the original would store a
// depth outside its four-entry stack array (group S18's BuffSpike_Draw, the
// same code).
#include "game/magic_s12.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The target's sprite (the harness's kSource): what the tint and the
// position come from.
constexpr std::uint32_t kTargetSprite = 0x904B4C;
// A bit per enemy kind (the enemy record's byte +0x8C): set once identified.
constexpr std::uint32_t kSeenBits = 0x9040A8;
// The panel's texts: two in the engine's .data the originals centre, a label
// beside the second number, and MAGIC060's own: a label beside the first
// number, one mark, a row of marks, the number format.
constexpr std::uint32_t kText1 = 0x66A3E0, kText2 = 0x66A3E8, kLabel2 = 0x66A31C;
constexpr std::uint32_t kLabel1 = 0x65AAB0, kMark = 0x65AAC4, kMarks = 0x65AAC8, kNumberFormat = 0x65AB08;
// Six pointers to the elements' two-byte glyph codes.
constexpr std::uint32_t kElementGlyphs = 0x66A398;
// Celerity_End's byte, and Celerity_ApplyStat's cells: the result record
// pointer, the ability word and the step byte of its NameTable_Abilities
// record (+3, records of 0x18).
constexpr std::uint32_t kEndByte = 0x90465C;
constexpr std::uint32_t kResultRecord = 0x904B60, kAbility = 0x904B80, kAbilityStep = 0x65C4DB;
// Capcom's, unnamed, in no group: a stat change for the target
// (MagicFx_ApplyBuff's 0x44F650 calls it the same way).
constexpr std::uint32_t kStatChanged = bof3::addr::Battle_RecalcStats;   // rebound 2026-09-29 (round twelve, BE6): the value is unchanged, the fuzz keys on it
// Celerity_Start's CLUT strip: 16 words at +0 and 16 at +0x20, copied over.
constexpr std::uint32_t kClutFrom = 0x80E980, kClutTo = 0x812980;

unsigned char* SC() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned char* Slot(unsigned k) { return Mem(at::kTasks + (k & 0xFFu) * at::kTaskStride); }
std::int32_t Ptr(const void* p) { return static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t U32(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::int32_t AddL(std::int32_t a, std::uint32_t b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + b); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

// The records as the originals index them, unchecked: a member by its index
// (stride 0x14C), an enemy by its index (stride 0x128) - the battle index
// less 3, which a party index sends below the enemy records.
unsigned char* PartyRec(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRec(int i) { return Mem(at::kEnemies + static_cast<std::uint32_t>(i) * at::kEnemyStride); }

// The scratch words DamageScratch + k and the vertex scratch
// Prim_VertexScratch + k (SVECTORs at +0, +8, +0x10, +0x18).
constexpr std::uint32_t kDs = bof3::addr::DamageScratch;
constexpr std::uint32_t kVx = 0x9037A0;
short Ds(unsigned k) { return static_cast<short>(Word(Mem(kDs + k))); }
void SetDs(unsigned k, unsigned v) { SetWord(Mem(kDs + k), v); }
std::int32_t Dd(unsigned k) { return Long(Mem(kDs + k)); }
void SetDd(unsigned k, std::uint32_t v) { SetLong(Mem(kDs + k), static_cast<std::int32_t>(v)); }
unsigned char DsByte(unsigned k) { return Mem(kDs + k)[0]; }
short Vx(unsigned k) { return static_cast<short>(Word(Mem(kVx + k))); }
void SetVx(unsigned k, unsigned v) { SetWord(Mem(kVx + k), v); }
const short* VxP(unsigned k) { return reinterpret_cast<const short*>(Mem(kVx + k)); }

// `imul` (a 32-bit product that wraps) then `sar n`; `shl l` then `sar r`.
int MulSar(int a, int b, int n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}
int ShlSar(int a, int l, int r) { return static_cast<int>(static_cast<std::uint32_t>(a) << l) >> r; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
float* Fl(unsigned char* p) { return reinterpret_cast<float*>(p); }

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
using FnU = void (__cdecl*)(unsigned);
using FnXY = void (__cdecl*)(int, int);
using BoolFn = unsigned char (__cdecl*)();
using ItemFn = const unsigned char* (__cdecl*)(unsigned, unsigned, int);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// The GTE calls as the originals push them: one pointer more than
// symbols.toml's prototypes carry (a flag word the callee may write).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                               long*, long*);
using Avg3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);
template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

long Rtp3(unsigned char* p) {
    long depth = 0, flag = 0;
    return MH_CALL(As<Rtp3Fn>(&::Gte_RotTransPers3))(VxP(0), VxP(8), VxP(0x10), Fl(p + 8), Fl(p + 0x18), Fl(p + 0x28), &depth,
                                                     &flag);
}
long Rtp4(unsigned char* p) {
    long depth = 0, flag = 0;
    return MH_CALL(As<Rtp4Fn>(&::Gte_RotTransPers4))(VxP(0), VxP(8), VxP(0x10), VxP(0x18), Fl(p + 8), Fl(p + 0x18),
                                                     Fl(p + 0x28), Fl(p + 0x38), &depth, &flag);
}
long Avg3(unsigned char* p) {
    long depth = 0, flag = 0;
    return MH_CALL(As<Avg3Fn>(&::Gte_RotAverage3))(VxP(0), VxP(8), VxP(0x10), Fl(p + 8), Fl(p + 0x18), Fl(p + 0x28), &depth,
                                                   &flag);
}
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// A draw-mode packet (tpage `tpage`, dithered) committed to `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}
void LinkAt(unsigned dy, unsigned size) {
    const unsigned char* const sc = SC();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(sc + 0x34)), static_cast<unsigned long>(Long(sc + 0x38)),
                                static_cast<int>(dy), size);
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
void DrawCentredCounted(const unsigned char* text, int y) {
    const unsigned n = CharCount(text) & 0xFFu;
    DrawAt(0xA0 - 6 * static_cast<int>(n), y, static_cast<int>(n), text);
}

}  // namespace

#define MS12_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC060 (row 39): the task, the roll, the panel

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
        SetLong(t + 0x80, Ptr(SC()));
        t[1] = 0;
    }
    {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x12));
        SetLong(t + 0x80, Ptr(SC()));
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
    unsigned char* const sc = SC();
    SetLong(t + 0x80, Ptr(sc));
    t[1] = 2;
    sc[9] = 0x10;
    Bump(SC()[1]);
}

namespace {
// The panel: a member's for a target below 3, else an enemy's.
void DrawPanel() {
    if (TargetByte() < 3) {
        Call0(bof3::addr::Identify_DrawMember);
    } else {
        Call0(bof3::addr::Identify_DrawEnemy);
    }
}
}  // namespace

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
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B0EF0: the roll. The chance by the acting member's byte +0x8A
// less the target enemy's word +0x98: 16 at 15 or more, 14 / 12 / 10 / 8 /
// 6 / 4 / 2 below 15 / 10 / 5 / 0 / -5 / -10 / -15; a hit when Rand & 0xF
// is below it. A kind seen before shows (+4 1); else a hit on an enemy whose
// +0x8F is not 0 (the target read again) marks the kind and shows; else +4 0.
MS12_EXPORT void __cdecl Identify_Roll(void) {
    const int enemy = static_cast<int>(Word(EnemyRec(static_cast<int>(TargetByte()) - 3) + 0x98));
    const int actor = PartyRec(Mem(at::kActor)[0])[0x8A];
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
    unsigned char* const word = Mem(kSeenBits + 4 * (kind >> 5));
    SetLong(word, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(word)) | (1u << (kind & 31))));
}

// original 0x4B1040: that bit, 1 when set.
MS12_EXPORT unsigned char __cdecl Identify_WasSeen(void) {
    const unsigned kind = EnemyRec(static_cast<int>(TargetByte()) - 3)[0x8C];
    return (static_cast<std::uint32_t>(Long(Mem(kSeenBits + 4 * (kind >> 5)))) & (1u << (kind & 31))) != 0 ? 1 : 0;
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
        DrawCentred(rec + 0x80, 0x30, 5);
        PartyRec(TargetByte())[0x89] = kept;
    }
    DrawCentredCounted(Text(kText1), 0x44);
    DrawAt(0x70, 0x50, 8, Text(kMarks));
    DrawAt(0x9C, 0x64, 1, Text(kMark));
    DrawAt(0xB0, 0x64, 3, Text(kLabel1));
    DrawAt(0x9C, 0x78, 1, Text(kMark));
    DrawAt(0xB0, 0x78, 1, Text(kLabel2));
    DrawCentredCounted(Text(kText2), 0x8A);
    DrawAt(0x70, 0x98, 8, Text(kMarks));
    DrawAt(0x70, 0xAC, 8, Text(kMarks));
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
    const unsigned text1 = CharCount(Text(kText1)) & 0xFFu;
    unsigned char* const rec = EnemyRec(index);
    if (rec[0x8F] != 0) {
        DrawCentred(rec + 0x80, 0x30, 5);
        DrawAt(0xA0 - 6 * static_cast<int>(text1), 0x44, static_cast<int>(text1), Text(kText1));
        MH_AT(FnXY, bof3::addr::Identify_DrawElements)(0x66, 0x54);
        char number[12];
        MH_CALL(Crt_sprintf)(number, reinterpret_cast<const char*>(Text(kNumberFormat)), static_cast<unsigned>(Word(rec + 0x96)));
        DrawAt(0x70, 0x64, 5, reinterpret_cast<const unsigned char*>(number));
        DrawAt(0xB0, 0x64, 3, Text(kLabel1));
        MH_CALL(Crt_sprintf)(number, reinterpret_cast<const char*>(Text(kNumberFormat)), static_cast<unsigned>(Word(rec + 0x94)));
        DrawAt(0x70, 0x78, 5, reinterpret_cast<const unsigned char*>(number));
        DrawAt(0xB0, 0x78, 1, Text(kLabel2));
    } else {
        DrawCentred(rec + 0x80, 0x30, 5);
        DrawAt(0xA0 - 6 * static_cast<int>(text1), 0x44, static_cast<int>(text1), Text(kText1));
        DrawAt(0x70, 0x50, 8, Text(kMarks));
        DrawAt(0x9C, 0x64, 1, Text(kMark));
        DrawAt(0xB0, 0x64, 3, Text(kLabel1));
        DrawAt(0x9C, 0x78, 1, Text(kMark));
        DrawAt(0xB0, 0x78, 1, Text(kLabel2));
    }
    const unsigned text2 = CharCount(Text(kText2)) & 0xFFu;
    const unsigned char* last = DrawAt(0xA0 - 6 * static_cast<int>(text2), 0x8A, static_cast<int>(text2), Text(kText2));
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
    if (SC()[4] == 0) return DrawAt(0x70, y, 8, Text(kMarks));
    const unsigned i = item & 0xFFu;
    const unsigned char* name;
    switch (category & 0xFFu) {
    case 1: name = Mem(bof3::addr::NameTable_Weapons + 28 * i); break;
    case 2: name = Mem(bof3::addr::NameTable_Armour + 26 * i); break;
    case 3: name = Mem(bof3::addr::NameTable_Accessories + 24 * i); break;
    default: name = Mem(bof3::addr::NameTable_Consumables + 22 * i); break;
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
        SetWord(p + 0xA, static_cast<unsigned>(y));
        SetWord(p + 0x12, static_cast<unsigned>(y));
        SetWord(p + 0x1A, static_cast<unsigned>(y + 0xC));
        SetWord(p + 0x22, static_cast<unsigned>(y + 0xC));
        SetWord(p + 8, static_cast<unsigned>(x));
        SetWord(p + 0x10, static_cast<unsigned>(right));
        SetWord(p + 0x18, static_cast<unsigned>(x));
        SetWord(p + 0x20, static_cast<unsigned>(right));
        unsigned shade = 0x30;
        SetDd(4, 0x30);
        if (EnemyRec(index)[kResist[i]] <= (i == 5 ? 3 : 1)) {
            shade = 0x80;
            SetDd(4, 0x80);
        }
        p[4] = static_cast<unsigned char>(shade);
        p[5] = DsByte(4);
        p[6] = DsByte(4);
        const unsigned char* const glyph = Pointer(kElementGlyphs + 4 * i);
        SetWord(p + 0x16, (static_cast<unsigned>(glyph[0] & 0x7F) << 8) + glyph[1]);
        SetWord(p + 0xE, MH_CALL(Gpu_GetClut)(0, 0x1E0));
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

// ===========================================================================
// MAGIC060's children (kind 1, parameter 0x12)

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
    MH_CALL(Sprite_ReleaseTint)(Pointer(kTargetSprite));
    const unsigned char record = MH_CALL(Sprite_SetTint)(Pointer(kTargetSprite), 0, 0, 0, 1);
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
    MH_CALL(Sprite_ReleaseTint)(Pointer(kTargetSprite));
    MH_CALL(BattleActor_Flash)(TargetByte());
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// ===========================================================================
// MAGIC062 (row 95): the task, the ring and the four sparks

// original 0x4B1CB0: the kind-2 task. Its phase +1 through a six-entry stack
// table: Celerity_Start, BattleFx_TintActor, BattleFx_Brighten (round eight's),
// Buff_WaitChildren (MAGIC082's), Celerity_Apply, Celerity_End. Unchecked in
// the original; ours aborts.
MS12_EXPORT void __cdecl Celerity_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Celerity_Start,    bof3::addr::BattleFx_TintActor,
                                                 bof3::addr::BattleFx_Brighten, bof3::addr::Buff_WaitChildren,
                                                 bof3::addr::Celerity_Apply,    bof3::addr::Celerity_End};
    CallPhase(kPhases, 6, "Celerity_Task");
}

// original 0x4B1D00: the target's sprite's +8 and position to the task; +0xB
// 0, +9 0x10, the phase on. Five children BattleTask_Create(1, 0x4A), this
// task's +0xB counting them: the ring (+1 0, +9 0, the task's position) and
// four sparks (+1 1, +4 0..3, +0xB 0 / 8 / 0x10 / 0x18, +9 that + 1, +0xA the
// task's +9). The CLUT strip 0x80E980 (16 + 16 words) to 0x812980,
// Gfx_ClutStripDirty 1, Sound_PlayById(0x100).
MS12_EXPORT void __cdecl Celerity_Start(void) {
    const unsigned char* const src = Pointer(kTargetSprite);
    SC()[8] = src[8];
    SetLong(SC() + 0x34, Long(src + 0x34));
    SetLong(SC() + 0x38, Long(src + 0x38));
    SetLong(SC() + 0x3C, Long(src + 0x3C));
    SC()[0xB] = 0;
    SC()[9] = 0x10;
    Bump(SC()[1]);
    {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x4A));
        unsigned char* const sc = SC();
        SetLong(t + 0x80, Ptr(sc));
        t[1] = 0;
        t[9] = 0;
        SetLong(t + 0x34, Long(sc + 0x34));
        SetLong(t + 0x38, Long(sc + 0x38));
        SetLong(t + 0x3C, Long(sc + 0x3C));
        Bump(sc[0xB]);
    }
    unsigned char spark = 0;
    for (unsigned b = 0; b < 0x20; b += 8) {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x4A));
        unsigned char* const sc = SC();
        SetLong(t + 0x80, Ptr(sc));
        t[1] = 1;
        t[4] = spark;
        t[0xB] = static_cast<unsigned char>(b);
        t[9] = static_cast<unsigned char>(b + 1);
        t[0xA] = sc[9];
        Bump(sc[0xB]);
        ++spark;
    }
    for (unsigned i = 0; i < 0x20; i += 2) {
        SetWord(Mem(kClutTo + i), Word(Mem(kClutFrom + i)));
        SetWord(Mem(kClutTo + 0x20 + i), Word(Mem(kClutFrom + 0x20 + i)));
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4B1F40: the tint record MoveScript_TintRecords[+0xA]'s three
// colour bytes (+2, +3, +4) down one; +9 down. At 0: the tint released, the
// target flashed; for each of the four stats MagicFx_BuffStats[i] the stat
// applied (Celerity_ApplyStat) and a popup child BattleTask_Create(1, 0x48)
// (+4 i, +9 5i + 1, +0xA 12i + 1), +0xB up; the phase on.
MS12_EXPORT void __cdecl Celerity_Apply(void) {
    unsigned char* const sc = SC();
    unsigned char* const tints = MoveScript_TintRecords;
    Drop(tints[sc[0xA] * 12u + 2]);
    Drop(tints[sc[0xA] * 12u + 3]);
    Drop(tints[sc[0xA] * 12u + 4]);
    Drop(sc[9]);
    if (SC()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(kTargetSprite));
    MH_CALL(BattleActor_Flash)(TargetByte());
    for (unsigned i = 0; i < 4; ++i) {
        MH_AT(FnU, bof3::addr::Celerity_ApplyStat)(MagicFx_BuffStats[i]);
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x48));
        unsigned char* const owner = SC();
        SetLong(t + 0x80, Ptr(owner));
        t[4] = static_cast<unsigned char>(i);
        t[9] = static_cast<unsigned char>(i * 5 + 1);
        t[0xA] = static_cast<unsigned char>(i * 12 + 1);
        Bump(owner[0xB]);
    }
    Bump(SC()[1]);
}

// original 0x4B2040: once the children have ended (+0xB 0): the effect-done
// bit, the byte 0x90465C 5, the task freed.
MS12_EXPORT void __cdecl Celerity_End(void) {
    if (SC()[0xB] != 0) return;
    Mem(at::kFlags)[0] |= 4;
    Mem(kEndByte)[0] = 5;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B2060: one stat of the target moved by the ability's step.
// 0x904B60 = the target's result record (a member's record + 0x124 below 3,
// else the enemy's + 0x104); its words +4 and +6 and byte +8 cleared; the
// byte +0x14 + stat plus the signed step (the byte at 0x65C4DB + 0x18 * the
// ability word 0x904B80): above 100 made 100, below -100 made -100, else
// stored and the engine's 0x453300(target) told.
MS12_EXPORT void __cdecl Celerity_ApplyStat(unsigned stat) {
    const unsigned char target = TargetByte();
    unsigned char* const rec = target < 3 ? PartyRec(target) + 0x124 : EnemyRec(static_cast<int>(target) - 3) + 0x104;
    SetLong(Mem(kResultRecord), Ptr(rec));
    SetWord(Pointer(kResultRecord) + 4, 0);
    SetWord(Pointer(kResultRecord) + 6, 0);
    Pointer(kResultRecord)[8] = 0;
    const signed char step = static_cast<signed char>(Mem(kAbilityStep + 0x18u * Word(Mem(kAbility)))[0]);
    unsigned char* const cell = Pointer(kResultRecord) + 0x14 + (stat & 0xFFu);
    const int value = static_cast<signed char>(cell[0]) + step;
    if (value > 100) {
        cell[0] = 100;
        return;
    }
    if (value < -100) {
        cell[0] = 0x9C;
        return;
    }
    cell[0] = static_cast<unsigned char>(cell[0] + static_cast<unsigned char>(step));
    MH_AT(FnU, kStatChanged)(TargetByte());
}

// original 0x4B2120: the children (kind 1, parameter 0x4A): jmp
// [CelerityChild_Kinds + 4 * +1] - the ring, the sparks.
MS12_EXPORT void __cdecl CelerityChild_Task(void) { CallCell(CelerityChild_Kinds, 2, SC()[1], "CelerityChild_Task"); }

// original 0x4B2140: the ring - its three steps (BarrierRing_Grow,
// BuffRing_Wait, MagicFx_CountDownRelease); while live (+0), the disc and the
// band under the actor matrix.
MS12_EXPORT void __cdecl CelerityRing_Run(void) {
    CallCell(CelerityRing_Steps, 3, SC()[2], "CelerityRing_Run");
    if (SC()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::CelerityRing_DrawDisc);
    Call0(bof3::addr::CelerityRing_DrawBand);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4B2180: a flat disc of sixteen semi-transparent Gouraud
// triangles (tpage 0x35), radius 0x80 (sin / cos << 7 >> 12); the centre
// +9 * 5 grey, the rim (Rand & 3 + 3) * +9 per channel (one Rand each, the
// words 0x903858 / 0x90385A / 0x90385C, which the band reads).
MS12_EXPORT void __cdecl CelerityRing_DrawDisc(void) {
    DrawModeCommit(0x35, 5);
    SetDs(6, SC()[9] * 5u);
    for (unsigned k : {8u, 0xAu, 0xCu}) {
        const int r = MH_CALL(Rand)();
        SetDs(k, static_cast<unsigned>((r & 3) + 3) * SC()[9]);
    }
    int r = Sin(0);
    SetVx(0x10, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Cos(0);
    SetVx(0x12, static_cast<unsigned>(ShlSar(r, 7, 12)));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        const unsigned x = static_cast<std::uint16_t>(Vx(0x10)), y = static_cast<std::uint16_t>(Vx(0x12));
        SetVx(0, 0);
        SetVx(2, 0);
        SetVx(8, x);
        SetVx(0xA, y);
        r = Sin(a);
        SetVx(0x10, static_cast<unsigned>(ShlSar(r, 7, 12)));
        r = Cos(a);
        unsigned char* const p = Gfx_PacketNext;
        SetVx(0x14, 0);
        SetVx(0x12, static_cast<unsigned>(ShlSar(r, 7, 12)));
        SetVx(0xC, 0);
        SetVx(4, 0);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = DsByte(6);
        p[5] = DsByte(6);
        p[6] = DsByte(6);
        for (unsigned k : {0x14u, 0x24u}) {
            p[k] = DsByte(8);
            p[k + 1] = DsByte(0xA);
            p[k + 2] = DsByte(0xC);
        }
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawModeCommit(0x15, 5);
}

// original 0x4B2380: a flat band of sixteen semi-transparent Gouraud quads
// (tpage 0x35) from radius 0x80 to 0x100; the inner edge the scratch words
// the disc left, the outer 1 1 1.
MS12_EXPORT void __cdecl CelerityRing_DrawBand(void) {
    DrawModeCommit(0x35, 5);
    int r = Sin(0);
    SetVx(8, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Cos(0);
    SetVx(0xA, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Sin(0);
    SetVx(0x18, static_cast<unsigned>(ShlSar(r, 8, 12)));
    r = Cos(0);
    SetVx(0x1A, static_cast<unsigned>(ShlSar(r, 8, 12)));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        SetVx(0, static_cast<std::uint16_t>(Vx(8)));
        SetVx(2, static_cast<std::uint16_t>(Vx(0xA)));
        SetVx(0x10, static_cast<std::uint16_t>(Vx(0x18)));
        SetVx(0x12, static_cast<std::uint16_t>(Vx(0x1A)));
        r = Sin(a);
        SetVx(8, static_cast<unsigned>(ShlSar(r, 7, 12)));
        r = Cos(a);
        SetVx(0xA, static_cast<unsigned>(ShlSar(r, 7, 12)));
        r = Sin(a);
        SetVx(0x18, static_cast<unsigned>(ShlSar(r, 8, 12)));
        r = Cos(a);
        unsigned char* const p = Gfx_PacketNext;
        SetVx(0x1C, 0);
        SetVx(0x1A, static_cast<unsigned>(ShlSar(r, 8, 12)));
        SetVx(0x14, 0);
        SetVx(0xC, 0);
        SetVx(4, 0);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 0x14u}) {
            p[k] = DsByte(8);
            p[k + 1] = DsByte(0xA);
            p[k + 2] = DsByte(0xC);
        }
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    DrawModeCommit(0x15, 5);
}

// original 0x4B2570: a spark - jmp [CeleritySpark_Steps + 4 * +2] (six:
// MagicFx_WaitA, ShieldSpark_Place, then the four below).
MS12_EXPORT void __cdecl CeleritySpark_Run(void) { CallCell(CeleritySpark_Steps, 6, SC()[2], "CeleritySpark_Run"); }

namespace {

void DrawDisc(unsigned radius) { MH_AT(FnU, bof3::addr::CeleritySpark_DrawDisc)(radius); }

// The sparks' shared frame: the actor's screen point, a disc of radius
// `base` + Rand() & 3, and the spark itself under the actor matrix.
void SparkFrame(unsigned base) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    const int r = MH_CALL(Rand)();
    DrawDisc(static_cast<unsigned>((r & 3) + static_cast<int>(base)));
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::CeleritySpark_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// +9 up on odd frames; the speed +0x14 plus the acceleration +0x20, and the
// height word +0x3E plus the speed's low word; +0xA down, answering whether
// it reached 0.
bool SparkFly() {
    if (Frame_Counter & 1) Bump(SC()[9]);
    unsigned char* sc = SC();
    SetLong(sc + 0x14, AddL(Long(sc + 0x14), static_cast<std::uint32_t>(Long(sc + 0x20))));
    sc = SC();
    SetWord(sc + 0x3E, Word(sc + 0x3E) + Word(sc + 0x14));
    Drop(SC()[0xA]);
    return SC()[0xA] == 0;
}

}  // namespace

// original 0x4B2590: the spark's frame (disc 0x18); flies; at +0xA 0, the
// speed -16, the acceleration 4, +0xA 8, on.
MS12_EXPORT void __cdecl CeleritySpark_Rise(void) {
    SparkFrame(0x18);
    if (!SparkFly()) return;
    SetLong(SC() + 0x14, -16);
    SetLong(SC() + 0x20, 4);
    SC()[0xA] = 8;
    Bump(SC()[2]);
}

// original 0x4B2630: the same frame and flight; at +0xA 0, +0xA 0x1C - +0xB
// and on.
MS12_EXPORT void __cdecl CeleritySpark_Fall(void) {
    SparkFrame(0x18);
    if (!SparkFly()) return;
    unsigned char* const sc = SC();
    sc[0xA] = static_cast<unsigned char>(0x1C - sc[0xB]);
    Bump(SC()[2]);
}

// original 0x4B26C0: the frame (disc 0x40 once the spin +0xC is 4 and +0xA
// below 5, else 0x18); the spin up one every eighth frame of +0x10 until 4;
// +9 (the angle) plus the spin. At spin 4, +0xA down; at 0,
// Sound_PlayById(0x101) for the first spark (+0xB 0), +0x10 +0x14 +0x20 8,
// on.
MS12_EXPORT void __cdecl CeleritySpark_Spin(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* sc = SC();
    const bool wide = Long(sc + 0xC) == 4 && sc[0xA] < 5;
    const int r = MH_CALL(Rand)();
    DrawDisc(static_cast<unsigned>((r & 3) + (wide ? 0x40 : 0x18)));
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::CeleritySpark_Draw);
    MH_CALL(Gte_PopMatrix)();
    sc = SC();
    if (Long(sc + 0xC) < 4) {
        SetLong(sc + 0x10, AddL(Long(sc + 0x10), 1));
        sc = SC();
        if ((sc[0x10] & 7) == 0) {
            SetLong(sc + 0xC, AddL(Long(sc + 0xC), 1));
            sc = SC();
        }
    }
    sc[9] = static_cast<unsigned char>(sc[9] + sc[0xC]);
    sc = SC();
    if (Long(sc + 0xC) != 4) return;
    Drop(sc[0xA]);
    sc = SC();
    if (sc[0xA] != 0) return;
    if (sc[0xB] == 0) {
        MH_CALL(Sound_PlayById)(0x101);
        sc = SC();
    }
    SetLong(sc + 0x10, 8);
    SetLong(SC() + 0x14, 8);
    SetLong(SC() + 0x20, 8);
    Bump(SC()[2]);
}

// original 0x4B27A0: the frame (disc 0x20); +0x10 up 4; +9 plus the spin; +0xB
// up 2; the spark at the owner's x / z plus sin / cos((+0xB & 31) << 7) times
// (+0xA * +0x10 + 0xB0) >> 3; +0xA up; at 0x10, the owner's +0xB down and
// freed.
MS12_EXPORT void __cdecl CeleritySpark_Orbit(void) {
    SparkFrame(0x20);
    unsigned char* sc = SC();
    SetLong(sc + 0x10, AddL(Long(sc + 0x10), 4));
    sc = SC();
    sc[9] = static_cast<unsigned char>(sc[9] + sc[0xC]);
    sc = SC();
    sc[0xB] = static_cast<unsigned char>(sc[0xB] + 2);
    sc = SC();
    SetDs(4, (sc[0xB] & 0x1Fu) << 7);
    SetDs(0, sc[0xA] * static_cast<unsigned>(Word(sc + 0x10)) + 0xB0u);
    int r = Sin(Ds(4));
    SetLong(SC() + 0x34, AddL(static_cast<std::int32_t>(MulSar(r, Ds(0), 3)), static_cast<std::uint32_t>(Long(Owner() + 0x34))));
    r = Cos(Ds(4));
    SetLong(SC() + 0x38, AddL(static_cast<std::int32_t>(MulSar(r, Ds(0), 3)), static_cast<std::uint32_t>(Long(Owner() + 0x38))));
    Bump(SC()[0xA]);
    if (SC()[0xA] != 0x10) return;
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// One of CeleritySpark_Draw's two rows: four flat triangles from +9 to +9 +
// 0x20 by 8, each from the centre to two points of radius 0x30 at angles
// (a & 31) << 7 and ((a + 8) & 31) << 7, the centre `height` above (the
// first row +0x50, the second the scratch word +2 negated), coloured by
// `colors`[3 * (i + 4 * +4)]; each triangle's depth into `depths` and the
// packet cursor moved past it (0x34).
void SparkRow(const unsigned char* colors, bool first, int (&depths)[4]) {
    unsigned char* sc = SC();
    int a = sc[9];
    if (a >= sc[9] + 0x20) return;
    do {
        if (first) {
            SetDs(2, 0x50);
            SetDs(0, 0x30);
        }
        const int i = (a - sc[9]) / 8;
        SetDs(0xE, static_cast<unsigned>(i));
        const unsigned char* const rgb = colors + 3 * (static_cast<short>(i) + 4 * sc[4]);
        SetDs(8, rgb[0]);
        SetDs(0xA, rgb[1]);
        const unsigned height = first ? 0x50u : static_cast<unsigned>(-static_cast<int>(Ds(2)));
        SetVx(0, 0);
        SetVx(2, 0);
        SetDs(0xC, rgb[2]);
        SetDs(4, (static_cast<unsigned>(a) & 0x1Fu) << 7);
        SetVx(4, height);
        int r = Sin(Ds(4));
        SetVx(8, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        r = Cos(Ds(4));
        SetVx(0xC, 0);
        SetVx(0xA, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        SetDs(4, ((static_cast<unsigned>(a) + 8) & 0x1Fu) << 7);
        r = Sin(Ds(4));
        SetVx(0x10, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        r = Cos(Ds(4));
        unsigned char* const p = Gfx_PacketNext;
        SetVx(0x14, 0);
        SetVx(0x12, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 0);
        const long depth = Avg3(p);
        const int k = (a - SC()[9]) / 8;
        a += 8;
        // The original stores into its four-entry stack array by this index,
        // unchecked; nothing between the reads moves +9 in the game.
        if (k < 0 || k > 3) bof3::Fatal("CeleritySpark_Draw: depth index %d, past the four-entry array", k);
        depths[k] = static_cast<int>(depth);
        for (unsigned q : {4u, 0x14u, 0x24u}) {
            p[q] = DsByte(8);
            p[q + 1] = DsByte(0xA);
            p[q + 2] = DsByte(0xC);
        }
        Gfx_PacketNext = Gfx_PacketNext + 0x34;
        sc = SC();
    } while (a < sc[9] + 0x20);
}

}  // namespace

// original 0x4B28B0: a spark (tpage 0x15, linked at the task's x / z, +2,
// 0xC), two rows of four triangles - up from CeleritySpark_ColorsUp, down
// from CeleritySpark_ColorsDown - each row's four depths and first packet to
// MagicFx_LinkByDepth(x, z, depths, packets, 4, 0x34, 2).
MS12_EXPORT void __cdecl CeleritySpark_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    LinkAt(2, 0xC);
    int depths[4] = {};
    unsigned char* const up = Gfx_PacketNext;
    SparkRow(CeleritySpark_ColorsUp, true, depths);
    {
        const unsigned char* const sc = SC();
        MH_CALL(MagicFx_LinkByDepth)(static_cast<unsigned>(Long(sc + 0x34)), static_cast<unsigned>(Long(sc + 0x38)), depths,
                                     U32(up), 4, 0x34, 2);
    }
    unsigned char* const down = Gfx_PacketNext;
    SparkRow(CeleritySpark_ColorsDown, false, depths);
    const unsigned char* const sc = SC();
    MH_CALL(MagicFx_LinkByDepth)(static_cast<unsigned>(Long(sc + 0x34)), static_cast<unsigned>(Long(sc + 0x38)), depths,
                                 U32(down), 4, 0x34, 2);
}

// original 0x4B2D70: a flat disc of eight semi-transparent Gouraud triangles
// on screen (tpage 0x35), at the task's screen point (+0x2E / +0x30), radius
// the argument's low word; the centre CeleritySpark_DiscColors[+4] (through
// the scratch words 0x903858 / 0x90385A / 0x90385C), the rim 1 1 1; each
// linked at the task's x / z, +2 (0xC for the mode, 0x34 for the triangle).
MS12_EXPORT void __cdecl CeleritySpark_DrawDisc(unsigned radius) {
    SetDs(0, radius & 0xFFFFu);
    {
        const unsigned char* const rgb = CeleritySpark_DiscColors + 3u * SC()[4];
        SetDs(8, rgb[0]);
        SetDs(0xA, rgb[1]);
        SetDs(0xC, rgb[2]);
    }
    int a = 0;
    do {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        LinkAt(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(SC() + 0x2E));
        PutFloat(p + 0xC, S16(SC() + 0x30));
        int r = Sin(a);
        PutFloat(p + 0x18, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(S16(SC() + 0x2E))));
        r = Cos(a);
        PutFloat(p + 0x1C, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(S16(SC() + 0x30))));
        a += 0x200;
        r = Sin(a);
        PutFloat(p + 0x28, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(S16(SC() + 0x2E))));
        r = Cos(a);
        PutFloat(p + 0x2C, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(S16(SC() + 0x30))));
        p[4] = DsByte(8);
        p[5] = DsByte(0xA);
        p[6] = DsByte(0xC);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        LinkAt(2, 0x34);
    } while (a < 0x1000);
}

void MagicS12_Inject() {
    if (bof3::WantsShadow("magic_s12")) magic_s12::SelfTest();
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
    BOF3_INJECT(Celerity_Task);
    BOF3_INJECT(Celerity_Start);
    BOF3_INJECT(Celerity_Apply);
    BOF3_INJECT(Celerity_End);
    BOF3_INJECT(Celerity_ApplyStat);
    BOF3_INJECT(CelerityChild_Task);
    BOF3_INJECT(CelerityRing_Run);
    BOF3_INJECT(CelerityRing_DrawDisc);
    BOF3_INJECT(CelerityRing_DrawBand);
    BOF3_INJECT(CeleritySpark_Run);
    BOF3_INJECT(CeleritySpark_Rise);
    BOF3_INJECT(CeleritySpark_Fall);
    BOF3_INJECT(CeleritySpark_Spin);
    BOF3_INJECT(CeleritySpark_Orbit);
    BOF3_INJECT(CeleritySpark_Draw);
    BOF3_INJECT(CeleritySpark_DrawDisc);
}
