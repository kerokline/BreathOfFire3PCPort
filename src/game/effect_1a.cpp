// Round thirteen group E1A: the band 0x462B00..0x4672F0 of the cut
// (analysis/round13_cut.tsv), taken with the scenario harness in effect mode
// (scenario_harness.h, docs/scenario_harness.md section 8). Every state
// handler runs with Sprite_Current an Effect_Objects record, as
// Effect_RunObjects calls it through Effect_KindHandlers[+5] and the kind's
// dispatcher through its state table by +1. What each kind is, as far as the
// code says, is in docs/effect_1a.md section 2:
//
//   kinds 0xE, 0x16  a world map's record handler (WorldMap_Records +4 / +8)
//   kind 0x5C        area 104's or area 121's own handler
//   kind 1           a textured quad under an extra sprite (+0x18), flickering
//   kind 7           a colour ramp (+0x5D..+0x5F up to 0x80) with EffectKind07_DrawSprite
//   kinds 2, 8, 9, 0xB  dispatchers only (their states are R4F's, rest_4f.cpp)
//   kind 0x10        a textured quad between two extra sprites (+0xC, +0x18)
//   kind 3           a fading bar panel and an accessory's name with a count
//   kind 5           a sprite that rises to a height scaled by a parameter,
//                    bounces, spawns kind 0xA and then follows an object
//   kind 0xA         a copy of record 0's sprite following it (a shadow)
//   kind 0xC         an aim bar chasing an object's +0xA
//   kind 0xD         a sprite sliding in and out across the screen
//   kind 0xF         a message window: opens, titles, types lines, closes
//   kind 0x1A        the kind-count panel: option boxes, member rows, item
//                    lists, a panel sliding by +9
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Each is a
// faithful replacement; the one divergence, DIV-0069 (kind 0xF's lines and kind
// 3's name under a Latin overlay, src/game/fishing_text.cpp), is armed only after
// every self-test, so the fuzz compares Capcom's. Where the original indexes a table past its
// end, an object list past its records or the effect pool past its twenty,
// ours aborts with a message (docs/effect_1a.md section 6).
#include "game/effect_1a.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_1a_callees.h"
#include "game/fishing_text.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1a::at;
using U = std::uint32_t;
using move_script::At;

unsigned char* S() { return Sprite_Current; }

// DIV-0144 (D279): 1 through PatchBytes("FishingEmptyLine") / ("FishingLowGlyph")
// in FishingEmpty_Arm, after every module's self-test.
std::uint8_t g_empty_line_none = 0;
std::uint8_t g_low_glyph_none = 0;
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U L(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U L(U a) { return L(At(a)); }
void SetL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
U W(const unsigned char* p) { return move_script::Word(p); }
U W(U a) { return W(At(a)); }
void SetW(unsigned char* p, U v) { move_script::SetWord(p, v); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(move_script::Word(p)); }
std::int32_t SL(const unsigned char* p) { return move_script::Long(p); }
unsigned char B(U a) { return At(a)[0]; }
unsigned char* PtrAt(U cell) { return At(L(cell)); }
// The high half of a register the original pushes whole after loading a byte
// or word into its low half (`mov ax, [..]` over a pointer it held).
U Hi(U v) { return v & 0xFFFF0000u; }

using Handler = void (__cdecl*)();

// jmp [table + byte * 4]: the table's `entries` handlers, read in place (the
// fuzz swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after - the next table's entry or data.
void Run(const char* who, U table, unsigned entries, unsigned state) {
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_1a.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * state)))();
}

// A world map record's handler at `slot`, by WorldMap_RecordIndex's low byte;
// index 11 ("none") reads the dwords after the eleventh record, which are
// kind 1's and kind 7's state tables - the original's read, kept.
void RecordHandler(const char* who, U slot) {
    const U index = SH_CALL(WorldMap_RecordIndex)() & 0xFF;
    if (index >= at::kRecordCount)
        bof3::Fatal("%s: WorldMap_RecordIndex answered %u, past the twelve rows (docs/effect_1a.md section 6)", who,
                    (unsigned)index);
    const U handler = L(slot + index * at::kRecordSize);
    if (handler == 0)
        bof3::Fatal("%s: world map record %u has no handler at +0x%X (area 104's record) - the original jumps to 0 "
                    "(docs/effect_1a.md section 6)",
                    who, (unsigned)index, (unsigned)(slot - 0x653910));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(handler))();
}

// Sprite_ObjectsExtra record `index` (a dword of the effect record): four of them.
unsigned char* Extra(const char* who, U index) {
    if (index >= at::kExtraCount)
        bof3::Fatal("%s: extra sprite index %u, past the four Sprite_ObjectsExtra records - the original reads what "
                    "follows (docs/effect_1a.md section 6)",
                    who, (unsigned)index);
    return At(Sprite_ObjectsExtra_addr + index * at::kObjectStride);
}
// Sprite_Objects record `index` (the byte 0x939A1C): thirty of them.
unsigned char* Object(const char* who, U index) {
    if (index >= at::kObjectCount)
        bof3::Fatal("%s: object index %u (0x939A1C), past the thirty Sprite_Objects records - the original reads what "
                    "follows (docs/effect_1a.md section 6)",
                    who, (unsigned)index);
    return At(Sprite_Objects_addr + index * at::kObjectStride);
}
// Effect_FindFree's record: a Fatal on an answer past the twenty (the callee
// answers 0xFF or 0..19; the original would write past the pool). `index` is
// the answer as the caller widened it (a movsx in kind 0xF's spawns).
unsigned char* Spawned(const char* who, std::int32_t index) {
    if (index < 0 || index >= static_cast<std::int32_t>(at::kEffectCount))
        bof3::Fatal("%s: Effect_FindFree answered %d, past the 20 records - the original writes past the pool "
                    "(docs/effect_1a.md section 6)",
                    who, (int)index);
    return At(Effect_Objects_addr + static_cast<U>(index) * at::kEffectStride);
}
// Kind 0xF's text record `index` (+0x4B): thirteen of 8 bytes.
U Text(const char* who, unsigned index) {
    if (index >= at::kKind0FTextCount)
        bof3::Fatal("%s: text index +0x4B is %u, past the thirteen records of 0x653B98 (docs/effect_1a.md section 6)",
                    who, index);
    return at::kKind0FTexts + index * 8u;
}
// A text's line byte (+4 of its record) as an index of the label tables
// 0x653C00 / 0x66A2FC: three lines (0xFF, none, is tested by some callers).
unsigned Line(const char* who, unsigned line) {
    if (line >= 3)
        bof3::Fatal("%s: a text's line byte is 0x%X, past the three labels of 0x653C00 / 0x66A2FC - the original reads "
                    "what follows (docs/effect_1a.md section 6)",
                    who, line);
    return line;
}
// A text's label as drawn: its pen and its one-character text. DIV-0069: under
// a Latin overlay DIV-0051's icon, else the port's 0x66A2FC string. Ledger
// DIV-0136: the entry and the pen through sigma (FishingText_LabelOf; with it
// off, entry = the line byte and the pen 0x653C00's, the original's reads).
struct LabelDraw {
    U pen;
    U text;
};
LabelDraw Label(const char* who, unsigned index) {
    const unsigned line = Line(who, B(Text(who, index) + 4));
    const FishingLabel l = FishingText_LabelOf(index, line);
    if (FishingText_On()) return {l.pen, Key(FishingText_Label(l.entry))};
    // DIV-0069 (D279): a Japanese layer's own label, the disc's module's, where
    // its tag 4 gave one; else 0x66A2FC's string, the original's read.
    if (const unsigned char* disc = FishingText_DiscLabel(l.entry)) return {l.pen, Key(disc)};
    return {l.pen, L(at::kLabelTexts + 4 * l.entry)};
}
// Kind 0xF's row table 0x653C04: nine rows of four bytes, by +6 and a column.
U RowByte(const char* who, unsigned row, unsigned column) {
    const U offset = row * 4u + column;
    if (offset >= at::kKind0FRowBytes)
        bof3::Fatal("%s: row %u column %u, past the table 0x653C04 (docs/effect_1a.md section 6)", who, row, column);
    return at::kKind0FRows + offset;
}

// The raw callees (effect_1a_callees.h): E1F's two draw helpers answer eax
// (0x52CFE0 the primitive); E1B's are typed to answer eax too, because the
// original loads a byte or a word into the low half of the register the
// answer left and pushes it whole (docs/effect_1a.md section 5).
U DrawMode(U id, U slot) { return SH_AT(U (__cdecl*)(U, U), at::kDrawModeRecord)(id, slot); }
U DrawSprite(U id, U slot, U x, U y) { return SH_AT(U (__cdecl*)(U, U, U, U), at::kDrawSpriteRecord)(id, slot, x, y); }
void DepthPair() { SH_AT(void (__cdecl*)(), at::kDepthPair)(); }
U WindowBox(U x, U y, U w, U h, U colour) {
    return SH_AT(U (__cdecl*)(U, U, U, U, U), at::kWindowBox)(x, y, w, h, colour);
}
U OptionBoxes(U x, U y, U bits) { return SH_AT(U (__cdecl*)(U, U, U), at::kOptionBoxes)(x, y, bits); }
U MessageLine(U pen, U text, U width, U x) {
    return SH_AT(U (__cdecl*)(U, U, U, U), at::kMessageLine)(pen, text, width, x);
}
U MemberRows(U x, U y) { return SH_AT(U (__cdecl*)(U, U), at::kMemberRows)(x, y); }
U ItemListA(U x, U y) { return SH_AT(U (__cdecl*)(U, U), at::kItemListA)(x, y); }
U ItemListB(U x, U y) { return SH_AT(U (__cdecl*)(U, U), at::kItemListB)(x, y); }
void PanelTitle() { SH_AT(void (__cdecl*)(), at::kPanelTitle)(); }
U StringCount(U text) { return SH_AT(U (__cdecl*)(U), at::kStringCount)(text); }
void PrimFromRect(unsigned char* prim, unsigned char* rect) {
    SH_AT(void (__cdecl*)(unsigned char*, unsigned char*), at::kPrimFromRect)(prim, rect);
}
const unsigned char* TextAt(U address) { return At(address); }
void DrawText(U x, U y, U colour, U count, U text) {
    SH_CALL(Text_DrawAt)(static_cast<int>(x), static_cast<int>(y), static_cast<int>(colour), static_cast<int>(count),
                         TextAt(text));
}

// --- x87 as the original has it (the game's control word 0x027F: round to
// nearest, 53 bits); every value goes through the FPU as Capcom's does.
// `fild dword [v]; fstp dword [o]`
void Fild(std::int32_t v, void* o) {
    __asm__ volatile("fildl %1\n\tfstps (%0)" : : "r"(o), "m"(v) : "st", "memory");
}
// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}
// `fld dword [a]; fadd dword [c]; fstp dword [o]`
void FldAdd(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%1)\n\tfadds (%2)\n\tfstps (%0)" : : "r"(o), "r"(a), "r"(c) : "st", "memory");
}
// `fld dword [a]; fsub dword [c]; fstp dword [o]`
void FldSub(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%1)\n\tfsubs (%2)\n\tfstps (%0)" : : "r"(o), "r"(a), "r"(c) : "st", "memory");
}
// `fld dword [a]; fstp dword [o]` (a quiet copy through the FPU)
void Fld(const void* a, void* o) {
    __asm__ volatile("flds (%1)\n\tfstps (%0)" : : "r"(o), "r"(a) : "st", "memory");
}
// The CRT's _ftol 0x5B9550 on st(0): truncation toward zero set in a copy of
// the control word for one fistp to 64 bits, the word put back; the low dword.
// `fild qword [lo, 0]; fadd dword [f]; call _ftol`
U FildAddFtol(U lo, const void* f) {
    const std::uint64_t v = lo;
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "fildll %[v]\n\t"
        "fadds (%[f])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [v] "m"(v), [f] "r"(f)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// `fld dword [a]; fsub dword [b]; call _ftol`
U FldSubFtol(const void* a, const void* b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fsubs (%[b])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a), [b] "r"(b)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
void CopyDword(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, 4); }

// The RECT the two quad kinds wrap their primitive in: 8 bytes taken off the
// packet, then the library's primitive built from it at the cursor (12 bytes)
// and linked at Sprite_Current's point (0xC).
void AreaRect(U y, U w, U h) {
    unsigned char* const rect = Gfx_PacketNext;
    Gfx_PacketNext = rect + 8;
    SetW(rect, 0);
    SetW(rect + 2, y);
    SetW(rect + 4, w);
    SetW(rect + 6, h);
    PrimFromRect(Gfx_PacketNext, rect);
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(L(s + 0x34), L(s + 0x38), 0x11, 0xC);
}

// A world point with an attachment's offset turned into the GTE's vector: x
// and z as (v >> 9) - 0x4000, the height -(s16 +0x3E + out[2]) / 2, each an
// s16. The fourth word is 0: the original never writes it (stale stack), so
// it cannot be reproduced, only replaced - DIV-0023's ruling, as field_e2's
// Mode11_ObjectDraw; the projection reads x, y and z.
void AttachVector(const unsigned char* o, const long* out, short* v) {
    v[0] = static_cast<short>(static_cast<U>((static_cast<std::int32_t>(L(o + 0x34) + static_cast<U>(out[0]))) >> 9) - 0x4000u);
    v[1] = static_cast<short>(static_cast<U>((static_cast<std::int32_t>(L(o + 0x38) + static_cast<U>(out[1]))) >> 9) - 0x4000u);
    const std::int32_t h = static_cast<std::int32_t>(static_cast<U>(SW(o + 0x3E)) + static_cast<U>(out[2]));
    v[2] = static_cast<short>(-(h / 2));
    v[3] = 0;
}

// kinds 0xF's windows: ObjTrio 0's +3 at 3 or more names the option bit.
U OptionBits() {
    if (ObjTrio[3] < 3) return 0;
    return 1u << (S()[6] & 31);
}

}  // namespace

// ===========================================================================
// The dispatchers: Effect_KindHandlers' entries, by +1 through a state table
// (none bounds its index)
// ===========================================================================

// original 0x462B00 (Effect_KindHandlers 0xE): the world map record's +4
// handler by WorldMap_RecordIndex's low byte (Area33_Record04Run on Yraall's).
extern "C" void __cdecl EffectKind0E_WorldMap(void) { RecordHandler("EffectKind0E_WorldMap", WorldMap_Records_addr + 0x4); }

// original 0x462B20 (Effect_KindHandlers 0x16): the record's +8 handler.
extern "C" void __cdecl EffectKind16_WorldMap(void) { RecordHandler("EffectKind16_WorldMap", WorldMap_Records_addr + 0x8); }

// original 0x462B60 (Effect_KindHandlers 0x5C): Area104_Kind5CRun in area
// 0x68, Area121_Kind5CRun anywhere else (both tail jumps).
extern "C" void __cdecl EffectKind5C_Run(void) {
    if (Game_AreaNumber == 0x68)
        SH_CALL(Area104_Kind5CRun)();
    else
        SH_CALL(Area121_Kind5CRun)();
}

// original 0x462BA0 (Effect_KindHandlers 1): EffectKind01_States by +1.
extern "C" void __cdecl EffectKind01_Run(void) { Run("EffectKind01_Run", EffectKind01_States_addr, 2, S()[1]); }
// original 0x462E50 (Effect_KindHandlers 7): EffectKind07_States by +1.
extern "C" void __cdecl EffectKind07_Run(void) { Run("EffectKind07_Run", EffectKind07_States_addr, 5, S()[1]); }
// original 0x463040 (Effect_KindHandlers 8): EffectKind08_States by +1.
extern "C" void __cdecl EffectKind08_Run(void) { Run("EffectKind08_Run", EffectKind08_States_addr, 4, S()[1]); }
// original 0x463440 (Effect_KindHandlers 9): EffectKind09_States by +1.
extern "C" void __cdecl EffectKind09_Run(void) { Run("EffectKind09_Run", EffectKind09_States_addr, 10, S()[1]); }
// original 0x463EE0 (Effect_KindHandlers 0xB): EffectKind0B_States by +1.
extern "C" void __cdecl EffectKind0B_Run(void) { Run("EffectKind0B_Run", EffectKind0B_States_addr, 5, S()[1]); }
// original 0x464660 (Effect_KindHandlers 2): EffectKind02_States by +1.
extern "C" void __cdecl EffectKind02_Run(void) { Run("EffectKind02_Run", EffectKind02_States_addr, 5, S()[1]); }
// original 0x464F20 (Effect_KindHandlers 3): EffectKind03_States by +1.
extern "C" void __cdecl EffectKind03_Run(void) { Run("EffectKind03_Run", EffectKind03_States_addr, 4, S()[1]); }
// original 0x465310 (Effect_KindHandlers 5): EffectKind05_States by +1.
extern "C" void __cdecl EffectKind05_Run(void) { Run("EffectKind05_Run", EffectKind05_States_addr, 6, S()[1]); }
// original 0x4658C0 (Effect_KindHandlers 0xA): EffectKind0A_States by +1.
extern "C" void __cdecl EffectKind0A_Run(void) { Run("EffectKind0A_Run", EffectKind0A_States_addr, 2, S()[1]); }
// original 0x465A60 (Effect_KindHandlers 0xC): EffectKind0C_States by +1.
extern "C" void __cdecl EffectKind0C_Run(void) { Run("EffectKind0C_Run", EffectKind0C_States_addr, 3, S()[1]); }
// original 0x465F10 (Effect_KindHandlers 0xD): EffectKind0D_States by +1.
extern "C" void __cdecl EffectKind0D_Run(void) { Run("EffectKind0D_Run", EffectKind0D_States_addr, 5, S()[1]); }
// original 0x4667A0 (Effect_KindHandlers 0x1A): EffectKind1A_States by +1.
extern "C" void __cdecl EffectKind1A_Run(void) { Run("EffectKind1A_Run", EffectKind1A_States_addr, 17, S()[1]); }

// original 0x466080 (Effect_KindHandlers 0xF): unless the record is record 3
// (0x7E1360) or record 3's +1 is above 3, Effect_Release (a tail jump); else
// EffectKind0F_States by +1.
extern "C" void __cdecl EffectKind0F_Run(void) {
    unsigned char* const s = S();
    if (Key(s) != (Effect_Objects_addr + 0x180) && Effect_Objects[3].state <= 3) {
        SH_CALL(Effect_Release)();
        return;
    }
    Run("EffectKind0F_Run", EffectKind0F_States_addr, 13, s[1]);
}

// ===========================================================================
// Kind 1 (EffectKind01_States: _Start, _Draw) and kind 0x10: a textured quad
// ===========================================================================

// original 0x462BC0 (EffectKind01_States[0]): +0x54 = the dword +0x54 of the
// Sprite_ObjectsExtra record the dword +0x18 names; +1 = 1.
extern "C" void __cdecl EffectKind01_Start(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->frames, rv::As<records::SpriteObject>(Extra("EffectKind01_Start", static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18)))->frames);
    S()[1] = 1;
}

// original 0x462BF0 (EffectKind01_States[1]): with Sprite_Current the extra
// record +0x18 names, its point copied into the effect record's +0x34..+0x3F;
// a RECT (0, 0xF0, 16, 16) primitive linked at it; a POLY_FT4 at the cursor:
// the extra's point plus MoveCmd_AttachOffset(+0x1C) projected (the depth to
// the effect's +0x60, Gte_PrimDepthFlat4_10), the corners x - 8 / x + 8 over
// y (the top pair's y 0) - floats; then Sprite_Current back to the effect
// record, CLUT (0xB0, 0x1E3), tpage (0, 0, 0x2C0, 0x100), u 0 / 0x10, v
// (Frame_Counter >> 2) & 0x15 on top and that plus the bottom's y through
// _ftol below, shade 0x80, Gpu_SetShadeTex(1), linked (0x48); a RECT (0, 0,
// 0x100, 0x100) primitive linked (0xC). PSX: no twin (catalog part 2).
extern "C" void __cdecl EffectKind01_Draw(void) {
    unsigned char* const e = S();
    auto* const eo = rv::As<records::EffectObject>(e);
    unsigned char* const x = Extra("EffectKind01_Draw", static_cast<std::uint32_t>(rv::As<records::EffectObject>(e)->work_18));
    Sprite_Current = x;
    eo->x = L(S() + 0x34);
    eo->z = L(S() + 0x38);
    eo->height = L(S() + 0x3C);
    AreaRect(0xF0, 0x10, 0x10);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    long out[3];
    SH_CALL(MoveCmd_AttachOffset)(out, e[0x1C]);
    alignas(4) short v[4];
    AttachVector(S(), out, v);
    long depth[2];
    const long z = SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(p + 8), depth);
    eo->depth = static_cast<U>(z);
    SH_CALL(Gte_PrimDepthFlat4_10)(p);
    SetL(p + 0x1C, 0);
    FldAdd(p + 8, At(at::kQuadEight), p + 0x18);
    FldAdd(p + 8, At(at::kQuadEight), p + 0x38);
    FldSub(p + 8, At(at::kQuadEight), p + 0x28);
    FldSub(p + 8, At(at::kQuadEight), p + 8);
    Fld(p + 0xC, p + 0x3C);
    Fld(p + 0xC, p + 0x2C);
    SetL(p + 0xC, 0);
    Sprite_Current = e;
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, SH_CALL(Gpu_GetClut)(0xB0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, SH_CALL(Gpu_GetTPage)(0, 0, 0x2C0, 0x100));
    p[0x14] = 0;
    p[0x15] = static_cast<unsigned char>((Frame_Counter >> 2) & 0x15);
    p[0x24] = 0x10;
    p[0x34] = 0;
    p[0x25] = static_cast<unsigned char>((Frame_Counter >> 2) & 0x15);
    p[0x35] = static_cast<unsigned char>(FildAddFtol((Frame_Counter >> 2) & 0x15, p + 0x2C));
    p[0x44] = 0x10;
    p[0x45] = static_cast<unsigned char>(FildAddFtol((Frame_Counter >> 2) & 0x15, p + 0x2C));
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    SH_CALL(Gpu_SetShadeTex)(p, 1);
    SH_CALL(MapView_LinkPrimAt)(L(S() + 0x34), L(S() + 0x38), 0x11, 0x48);
    AreaRect(0, 0x100, 0x100);
}

// original 0x464350 (Effect_KindHandlers 0x10; the entry a jmp over eleven
// nops to 0x464360): EffectKind01_Draw's quad between two extra records: the
// point of the one +0xC names (Sprite_Current while its half is built, copied
// into the effect's +0x34..+0x3F) plus MoveCmd_AttachOffset(+0x10) gives the
// top pair (x - 8, x + 8 at its y; the depth Gte_StoreDepthF to +0x10, +0x20
// its copy), the one +0x18 names plus MoveCmd_AttachOffset(+0x1C) the bottom
// pair (+0x28.. likewise, the depth to +0x30 / +0x40); the effect's +0x60 the
// second projection's answer. v on the bottom row is the top's plus |the
// bottom's y - the top's| through _ftol. The rest as kind 1's.
extern "C" void __cdecl EffectKind10_Run(void) {
    unsigned char* const e = S();
    auto* const eo = rv::As<records::EffectObject>(e);
    Sprite_Current = Extra("EffectKind10_Run", rv::As<records::EffectWork>(e)->work_0c);
    eo->x = L(S() + 0x34);
    eo->z = L(S() + 0x38);
    eo->height = L(S() + 0x3C);
    AreaRect(0xF0, 0x10, 0x10);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    long out[3];
    SH_CALL(MoveCmd_AttachOffset)(out, e[0x10]);
    alignas(4) short v[4];
    AttachVector(S(), out, v);
    long depth[2];
    eo->depth = static_cast<U>(SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(p + 8), depth));
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
    CopyDword(p + 0x20, p + 0x10);
    CopyDword(p + 0x1C, p + 0xC);
    FldAdd(p + 8, At(at::kQuadEight), p + 0x18);
    FldSub(p + 8, At(at::kQuadEight), p + 8);
    Sprite_Current = Extra("EffectKind10_Run", static_cast<std::uint32_t>(rv::As<records::EffectObject>(e)->work_18));
    SH_CALL(MoveCmd_AttachOffset)(out, e[0x1C]);
    AttachVector(S(), out, v);
    eo->depth = static_cast<U>(SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(p + 0x28), depth));
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x30));
    CopyDword(p + 0x40, p + 0x30);
    CopyDword(p + 0x3C, p + 0x2C);
    FldAdd(p + 0x28, At(at::kQuadEight), p + 0x38);
    FldSub(p + 0x28, At(at::kQuadEight), p + 0x28);
    Sprite_Current = e;
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, SH_CALL(Gpu_GetClut)(0xB0, 0x1E3));
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, SH_CALL(Gpu_GetTPage)(0, 0, 0x2C0, 0x100));
    p[0x14] = 0;
    const U v0 = (Frame_Counter >> 2) & 0x15;
    p[0x15] = static_cast<unsigned char>(v0);
    p[0x24] = 0x10;
    p[0x34] = 0;
    p[0x25] = static_cast<unsigned char>((Frame_Counter >> 2) & 0x15);
    U span = FldSubFtol(p + 0x2C, p + 0xC);
    if (static_cast<std::int32_t>(span) < 0) span = 0u - span;
    p[0x44] = 0x10;
    p[0x35] = static_cast<unsigned char>(((Frame_Counter >> 2) & 0x15) + span);
    p[0x45] = static_cast<unsigned char>(((Frame_Counter >> 2) & 0x15) + span);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    SH_CALL(Gpu_SetShadeTex)(p, 1);
    SH_CALL(MapView_LinkPrimAt)(L(S() + 0x34), L(S() + 0x38), 0x11, 0x48);
    AreaRect(0, 0x100, 0x100);
}

// ===========================================================================
// Kind 7 (EffectKind07_States: _Start, _FadeIn, then EffectKind07_Hold / _FadeOut,
// R4F's)
// ===========================================================================

// original 0x462E70 (EffectKind07_States[0]): +0x5F, +0x5E, +0x5D = 0, +9 =
// 0xF0, +1 = 1.
extern "C" void __cdecl EffectKind07_Start(void) {
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[9] = 0xF0;
    S()[1] = 1;
}

// original 0x462EB0 (EffectKind07_States[1]): EffectKind07_DrawSprite(1) (a sprite
// primitive); +0x5D, +0x5E, +0x5F up by 2; at +0x5D 0x80 the counter byte
// 0x903848 = 0x14, +1 = 2, +9 = 0xFF.
extern "C" void __cdecl EffectKind07_FadeIn(void) {
    SH_AT(void (__cdecl*)(U), at::kKind07Sprite)(1);
    S()[0x5D] = static_cast<unsigned char>(S()[0x5D] + 2);
    S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 2);
    S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 2);
    unsigned char* const s = S();
    if (s[0x5D] != 0x80) return;
    At(at::kCounterByte)[0] = 0x14;
    s[1] = 2;
    S()[9] = 0xFF;
}

// ===========================================================================
// The panel draws (EffectHud_*): cdecl helpers of kinds 2, 3 and 0xC, and of
// E1B's kinds (docs/effect_1a.md section 2.4)
// ===========================================================================

// original 0x464BA0 (called by kind 2's states EffectKind02_SlideIn, _Body, _SlideOut):
// the draw mode (5, 3), panel sprite 0xD at (x + 0x18, y + 8); the draw mode
// (0, 3), sprites 0xE at (x, y) and 0xF at (x + 0x100, y); the count 0x3E -
// record 0's +0x3A (0 when negative or when record 0's +1 is 0) printed
// ("%d", Boss26Fx_CountFormat) into the text scratch and drawn with
// Text_DrawFont12 at (x + 0xEE, y + 0x1A), colour 2 when the count is 0x32 or
// more, ObjTrio 0's +2 is 4 and Frame_Counter has bit 2, else 0; then
// EffectHud_DrawGauge(x, y). PSX 0x801D159C (callers).
extern "C" void __cdecl EffectHud_Draw(int x, int y) {
    const U ux = static_cast<U>(x), uy = static_cast<U>(y);
    DrawMode(5, 3);
    DrawSprite(0xD, 3, ux + 0x18, uy + 8);
    DrawMode(0, 3);
    DrawSprite(0xE, 3, ux, uy);
    DrawSprite(0xF, 3, ux + 0x100, uy);
    signed char count = 0;
    if (Effect_Objects[0].state != 0) {
        count = static_cast<signed char>(0x3E - rv::Bytes(Effect_Objects)[0x3a]);
        if (count < 0) count = 0;
    }
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kTextBuffer)), reinterpret_cast<const char*>(At(Boss26Fx_CountFormat_addr)),
                         static_cast<int>(count));
    const int colour = (count >= 0x32 && ObjTrio[2] == 4 && (Frame_Counter & 4) != 0) ? 2 : 0;
    SH_CALL(Text_DrawFont12)(x + 0xEE, y + 0x1A, colour, At(at::kTextBuffer));
    SH_CALL(EffectHud_DrawGauge)(x, y);
}

// original 0x464C70 (EffectHud_Draw's): EffectHud_Bar(x + 0x58, y + 8, 4);
// the marker's y: y less record 0's elevation (AreaMap_Elevation at its +0x34
// / +0x38, s16, / 32 toward zero) when record 0's +1 is set, never above y
// (compared as s16); EffectHud_Marker(x + 0x59, that, 4); a POLY_F4 at the
// cursor from (s16 x + 0x59, s16 marker + 0x10) to (+ 16.0, s16 y + 0x28),
// colour (0x48, 0x20, 0), committed to slot 4 (0x38); when record 0's +1 is
// 4, EffectHud_Sprite8(x + 0x69, 0xB4 - record 0's s16 +0x3E / 32 when that
// is not above 0, else 0xB4, 2). PSX 0x801D195C (callers).
extern "C" void __cdecl EffectHud_DrawGauge(int x, int y) {
    const U ux = static_cast<U>(x), uy = static_cast<U>(y);
    SH_CALL(EffectHud_Bar)(x + 0x58, y + 8, 4);
    U marker = uy;
    if (Effect_Objects[0].state != 0) {
        const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(static_cast<std::uint32_t>(Effect_Objects[0].x)),
                                                  static_cast<long>(static_cast<std::uint32_t>(Effect_Objects[0].z)));
        const std::int32_t lift = static_cast<std::int16_t>(h) / 32;
        marker = uy - static_cast<U>(lift);
        if (static_cast<std::int16_t>(marker) < static_cast<std::int16_t>(uy)) marker = uy;
    }
    SH_CALL(EffectHud_Marker)(x + 0x59, static_cast<int>(marker), 4);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    const std::int32_t left = static_cast<std::int16_t>(ux) + 0x59;
    Fild(left, p + 8);
    FildAdd(left, At(at::kQuadSixteen), p + 0x14);
    CopyDword(p + 0x2C, p + 0x14);
    Fild(left, p + 0x20);
    Fild(static_cast<std::int16_t>(marker) + 0x10, p + 0xC);
    Fild(static_cast<std::int16_t>(marker) + 0x10, p + 0x18);
    Fild(static_cast<std::int16_t>(uy) + 0x28, p + 0x24);
    Fild(static_cast<std::int16_t>(uy) + 0x28, p + 0x30);
    p[4] = 0x48;
    p[5] = 0x20;
    p[6] = 0;
    SH_CALL(Gfx_CommitPrim)(4, 0x38);
    if (Effect_Objects[0].state != 4) return;
    const std::int16_t height = static_cast<std::int16_t>(static_cast<std::uint32_t>(Effect_Objects[0].height_whole));
    if (height > 0)
        SH_CALL(EffectHud_Sprite8)(x + 0x69, 0xB4, 2);
    else
        SH_CALL(EffectHud_Sprite8)(x + 0x69, 0xB4 - height / 32, 2);
}

// original 0x464DA0: a POLY_G4 at the cursor, (s16 x, s16 y) to (x + 16.0, y
// + 32.0) - floats -, the top corners (0x64, 0x64, 0x80), the bottom (0, 0,
// 0x50), committed to `slot` (0x44). PSX 0x801D1744 (call-anchored).
extern "C" void __cdecl EffectHud_Bar(int x, int y, unsigned slot) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    const std::int32_t xs = static_cast<std::int16_t>(x), ys = static_cast<std::int16_t>(y);
    Fild(xs, p + 8);
    FildAdd(xs, At(at::kQuadSixteen), p + 0x18);
    Fild(xs, p + 0x28);
    CopyDword(p + 0x38, p + 0x18);
    Fild(ys, p + 0xC);
    Fild(ys, p + 0x1C);
    FildAdd(ys, At(at::kQuadThirtyTwo), p + 0x2C);
    FildAdd(ys, At(at::kQuadThirtyTwo), p + 0x3C);
    p[4] = 0x64;
    p[5] = 0x64;
    p[6] = 0x80;
    p[0x14] = 0x64;
    p[0x15] = 0x64;
    p[0x16] = 0x80;
    p[0x24] = 0;
    p[0x25] = 0;
    p[0x26] = 0x50;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0x50;
    SH_CALL(Gfx_CommitPrim)(slot, 0x44);
}

// original 0x464E40: the draw mode (0, slot); a SPRT_16 at the cursor at
// (s16 x, s16 y) - floats -, shade 0x80, CLUT 0x7A40, u the byte of 0x653AF8
// by (Frame_Counter >> 3) & 3, v 0x48; committed to `slot` (0x18). PSX
// 0x801D1818 (call-anchored).
extern "C" void __cdecl EffectHud_Marker(int x, int y, unsigned slot) {
    DrawMode(0, slot);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt16)(p);
    rv::Store<std::uint32_t>(rv::As<records::GpuSprt>(p)->clut, 0x7A40);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    Fild(static_cast<std::int16_t>(x), p + 8);
    Fild(static_cast<std::int16_t>(y), p + 0xC);
    p[0x15] = 0x48;
    p[0x14] = B(at::kMarkerShades + ((Frame_Counter >> 3) & 3));
    SH_CALL(Gfx_CommitPrim)(slot, 0x18);
}

// original 0x464EC0: a SPRT_8 at the cursor at (s16 x, s16 y), shade 0x80,
// CLUT 0x7A40, u 0xB8, v 0x48; committed to `slot` (0x18). PSX 0x801D18CC.
extern "C" void __cdecl EffectHud_Sprite8(int x, int y, unsigned slot) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt8)(p);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    rv::Store<std::uint32_t>(rv::As<records::GpuSprt>(p)->clut, 0x7A40);
    p[0x14] = 0xB8;
    p[0x15] = 0x48;
    Fild(static_cast<std::int16_t>(x), p + 8);
    Fild(static_cast<std::int16_t>(y), p + 0xC);
    SH_CALL(Gfx_CommitPrim)(slot, 0x18);
}

// original 0x465120 (EffectKind03_Fade's and E1B's 0x4678C0's): two POLY_G4s
// from s16 x to s16 x + s16 w: the first over (s16 y, y + 4.0), black to (0,
// 0x80, 0) at the bottom; the second over (y + 4, y + 8.0), (0, 0x80, 0) at the
// top to black; each committed to `slot` (0x44). PSX 0x801D1B88.
extern "C" void __cdecl EffectHud_TwoBars(int x, int y, int w, unsigned slot) {
    const std::int32_t xs = static_cast<std::int16_t>(x), ys = static_cast<std::int16_t>(y);
    const std::int32_t right = static_cast<std::int16_t>(w) + xs;
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    Fild(xs, p + 8);
    Fild(right, p + 0x18);
    Fild(xs, p + 0x28);
    Fild(right, p + 0x38);
    Fild(ys, p + 0xC);
    Fild(ys, p + 0x1C);
    FildAdd(ys, At(at::kQuadHalf), p + 0x2C);
    FildAdd(ys, At(at::kQuadHalf), p + 0x3C);
    for (unsigned i : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x26u, 0x34u, 0x36u}) p[i] = 0;
    p[0x25] = 0x80;
    p[0x35] = 0x80;
    SH_CALL(Gfx_CommitPrim)(slot, 0x44);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    Fild(xs, p + 8);
    Fild(right, p + 0x18);
    Fild(xs, p + 0x28);
    Fild(right, p + 0x38);
    Fild(ys + 4, p + 0xC);
    Fild(ys + 4, p + 0x1C);
    FildAdd(ys + 4, At(at::kQuadHalf), p + 0x2C);
    FildAdd(ys + 4, At(at::kQuadHalf), p + 0x3C);
    for (unsigned i : {4u, 6u, 0x14u, 0x16u, 0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[i] = 0;
    p[5] = 0x80;
    p[0x15] = 0x80;
    SH_CALL(Gfx_CommitPrim)(slot, 0x44);
}

// original 0x4652D0 (EffectKind03_ShowName's and E1B's 0x467E10's): the draw
// mode (0, slot); panel sprites 0x17 at (x, y) and 0x17 + the byte `n` at (x +
// 0x28, y). PSX 0x801D2030.
extern "C" void __cdecl EffectHud_DrawCount(int x, int y, unsigned slot, unsigned n) {
    const U ux = static_cast<U>(x), uy = static_cast<U>(y);
    DrawMode(0, slot);
    const U prim = DrawSprite(0x17, slot, ux, uy);
    DrawSprite((prim & 0xFFFFFF00u) | static_cast<unsigned char>(n + 0x17), slot, ux + 0x28, uy);
}

// original 0x465D90 (EffectKind0C_Aim's and E1B's 0x4680F0 / 0x468210's): a
// POLY_FT4 at the cursor from (s16 x - 8, s16 y) to (x + 8, y + 8.0), shade
// 0x80, CLUT 0x7A40, tpage 0x99; u 0x28 / 0x38 across (swapped when the byte
// `flip` is set), v 0x58 / 0x60; committed to `slot` (0x48). PSX 0x801D2C30.
extern "C" void __cdecl EffectHud_DrawArrow(int x, int y, unsigned flip, unsigned slot) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, 0x99);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, 0x7A40);
    const std::int32_t xs = static_cast<std::int16_t>(x), ys = static_cast<std::int16_t>(y);
    Fild(xs - 8, p + 8);
    Fild(xs + 8, p + 0x18);
    Fild(xs - 8, p + 0x28);
    Fild(xs + 8, p + 0x38);
    Fild(ys, p + 0xC);
    Fild(ys, p + 0x1C);
    FildAdd(ys, At(at::kQuadEight), p + 0x2C);
    FildAdd(ys, At(at::kQuadEight), p + 0x3C);
    if (static_cast<unsigned char>(flip) == 0) {
        p[0x14] = 0x28;
        p[0x24] = 0x38;
    } else {
        p[0x14] = 0x38;
        p[0x24] = 0x28;
    }
    p[0x15] = 0x58;
    p[0x25] = 0x58;
    p[0x34] = p[0x14];
    p[0x35] = 0x60;
    p[0x45] = 0x60;
    p[0x44] = p[0x24];
    SH_CALL(Gfx_CommitPrim)(slot, 0x48);
}

// original 0x465E50 (EffectKind0C_Aim's and E1B's): the draw mode (0, slot);
// panel sprite 0x1C at (x, y), its word +0x18 = the byte `width` and u +0x14
// += the byte `shade`; when the byte `blink` is set and Frame_Counter's bit 3
// is not: the draw mode (6, 2), a TILE at the cursor, half-transparent (1), at
// (s16 x, s16 y), width `width` by 8.0, colour (0xFF, 0, 0), committed to
// `slot` (0x1C). PSX 0x801D3240 (callers).
extern "C" void __cdecl EffectHud_DrawMark(int x, int y, unsigned width, unsigned shade, unsigned slot, unsigned blink) {
    DrawMode(0, slot);
    unsigned char* const sprite = At(DrawSprite(0x1C, slot, static_cast<U>(x), static_cast<U>(y)));
    SetW(sprite + 0x18, static_cast<unsigned char>(width));
    sprite[0x14] = static_cast<unsigned char>(sprite[0x14] + static_cast<unsigned char>(shade));
    if (static_cast<unsigned char>(blink) == 0) return;
    if (Frame_Counter & 8) return;
    DrawMode(6, 2);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    Fild(static_cast<std::int16_t>(x), p + 8);
    SetL(p + 0x18, 0x41000000);
    p[4] = 0xFF;
    Fild(static_cast<std::int16_t>(y), p + 0xC);
    p[5] = 0;
    p[6] = 0;
    Fild(static_cast<unsigned char>(width), p + 0x14);
    SH_CALL(Gfx_CommitPrim)(slot, 0x1C);
}

// ===========================================================================
// Kind 3 (EffectKind03_States: BareRet, _Start, _Fade, _ShowName)
// ===========================================================================

// original 0x464F40 (EffectKind03_States[1]): +0x18 = the first byte of the
// parameters 0x939A20 points at (the fade's step), +0xC, +0x10, +0x1C = 0, +1
// up. PSX 0x801D1B54 (table-anchored).
extern "C" void __cdecl EffectKind03_Start(void) {
    const unsigned char* const script = PtrAt(at::kPanelScript);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_18, script[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c, 0);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x464F80 (EffectKind03_States[2]): the level +0xC stepped by the
// parameters' mode byte [1] - 0: +0xC += +0x18, bouncing off 0xFF and 0 by
// negating +0x18; 1: +0xC += +0x18 to 0xFF, then back to 0; 2: +0x10 += +0x18
// (a negative +0x10 that reaches 0 or more becomes -1), +0xC += +0x10,
// bouncing off 0xFF (+0x10 negated) and stopping at 0 (+0x10 0); nothing when
// +0x18 is 0. Then the draw mode (0, 2), panel sprite 0x10 at (0x87, 0xB4),
// EffectHud_TwoBars(0x88, 0xCB, +0xC * 0x60 / 255, 2); when ObjTrio 0's +2 is
// 3, the screen word +0x2E = 0xB8, +0x30 = 0xD1, +1 up. PSX 0x801D1CF0.
extern "C" void __cdecl EffectKind03_Fade(void) {
    unsigned char* a = S();
    const U step = static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->work_18);
    if (step != 0) {
        const unsigned mode = PtrAt(at::kPanelScript)[1];
        if (mode == 2) {
            const U speed = rv::As<records::EffectWork>(a)->work_10;
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_10, speed + step);
            a = S();
            if (static_cast<std::int32_t>(speed) < 0 && SL(a + 0x10) >= 0) {
                rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_10, 0xFFFFFFFFu);
                a = S();
            }
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_0c, rv::As<records::EffectWork>(a)->work_0c + rv::As<records::EffectWork>(a)->work_10);
            a = S();
            if (SL(a + 0x10) > 0) {
                if (SL(a + 0xC) >= 0xFF) {
                    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_0c, 0xFF);
                    a = S();
                    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_10, 0u - rv::As<records::EffectWork>(a)->work_10);
                }
            } else if (SL(a + 0xC) <= 0) {
                rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_0c, 0);
                a = S();
                rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_10, 0);
            }
        } else if (mode == 1) {
            const U level = rv::As<records::EffectWork>(a)->work_0c;
            if (level == 0xFF) {
                rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_0c, 0);
            } else {
                rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_0c, level + step);
                a = S();
                if (SL(a + 0xC) > 0xFF) rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_0c, 0xFF);
            }
        } else if (mode == 0) {
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_0c, rv::As<records::EffectWork>(a)->work_0c + step);
            a = S();
            const std::int32_t s = SL(a + 0x18);
            if (s > 0) {
                if (SL(a + 0xC) >= 0xFF) {
                    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_0c, 0xFF);
                    a = S();
                    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->work_18, 0u - static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->work_18));
                }
            } else if (s < 0 && SL(a + 0xC) <= 0) {
                rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_0c, 0);
                a = S();
                rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->work_18, 0u - static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->work_18));
            }
        }
    }
    DrawMode(0, 2);
    DrawSprite(0x10, 2, 0x87, 0xB4);
    const std::int32_t scaled = static_cast<std::int32_t>(rv::As<records::EffectWork>(S())->work_0c * 0x60u);
    SH_CALL(EffectHud_TwoBars)(0x88, 0xCB, scaled / 255, 2);
    if (ObjTrio[2] != 3) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0xB8);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0xD1);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x465230 (EffectKind03_States[3]): an accessory's name (by the
// byte 0x904130 while Frame_Counter's bit 6 is set, else 0x90412E) drawn at
// (0x8A, 0xC8), eight characters; then the count record 0's +7 less the byte
// +0xF of the block 0x939A24 points at - nothing when negative or 0, at most 4
// - drawn by EffectHud_DrawCount at (record 0's word +0x2E + 0xA, its dword
// +0x30 - 0x18), slot 2. PSX 0x801D1F38 (table-anchored).
extern "C" void __cdecl EffectKind03_ShowName(void) {
    const U name = (Frame_Counter & 0x40) != 0 ? at::kAccessoryNames + (L(at::kAccessoryA) & 0xFF) * 0x18u
                                               : at::kAccessoryNames + CharStats_ExtraItem * 0x18u;
    DrawText(0x8A, 0xC8, 0, FishingText_NameCount(), name);   // DIV-0069: 12 under a Latin overlay, else 8
    const unsigned char* const set = PtrAt(at::kPanelSet);
    const auto left = static_cast<signed char>(rv::Bytes(Effect_Objects)[0x7] - set[0xF]);
    if (left < 0) return;
    unsigned n;
    if (left > 4) {
        n = 4;
    } else {
        if (left == 0) return;
        n = static_cast<unsigned>(left);
    }
    const U x = Hi(Key(set)) | ((W(at::kRecord0 + 0x2E) + 0xA) & 0xFFFF);
    SH_CALL(EffectHud_DrawCount)(static_cast<int>(x), static_cast<int>(L(at::kRecord0 + 0x30) - 0x18), 2, n);
}

// ===========================================================================
// Kind 5 (EffectKind05_States: BareRet, _Start, _Rise, _Land, _Bounce, _Follow)
// ===========================================================================

namespace {
// Field_Kind2Z clamped to 0x150000..0x3C0000 (both kind-5 rises).
void Kind2ZClamp(std::int32_t z) {
    if (z >= 0x3C0000)
        Field_Kind2Z = 0x3C0000;
    else
        Field_Kind2Z = z < 0x150000 ? 0x150000 : z;
}
// +6 as a row of 0x653B24 / 0x653B44 (four each).
U Row05(const char* who, unsigned row) {
    if (row >= at::kKind05Rows)
        bof3::Fatal("%s: +6 is %u, past the four rows of 0x653B44 (docs/effect_1a.md section 6)", who, row);
    return row * 8u;
}
}  // namespace

// original 0x465330 (EffectKind05_States[1]): placed at the leader's x and z
// - 0x8000, its height word +0x3E the elevation there + 0x100; the rise +0x10
// = the parameters' byte [2] * record 2's +0xC / 255 + 8 (signed), +6 = that
// / 15, at most 3; +0x10 = +0x38 - (+0x10 << 15) (the height to reach, as a
// z); Field_Kind2Z that clamped; MoveScript_F3Divisor 0; +0xC 0; +0x14 / +0x20
// from 0x653B24's row +6; the sprite bytes cleared; Sprite_SetAnimationBank
// (the word +0xC of the block 0x939A24 points at), Sprite_SetAnimation(1); a
// kind-0xA record spawned (+0 1, +5 0xA) when Effect_FindFree has one; +7 =
// the block's byte +0xF; +1 up. PSX 0x801D2108 (callers).
extern "C" void __cdecl EffectKind05_Start(void) {
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(rv::As<records::PartyMember>(ObjTrio)->x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(rv::As<records::PartyMember>(ObjTrio)->z) - 0x8000u);
    unsigned char* s = S();
    const long h = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, static_cast<U>(h) + 0x100);
    const unsigned char* const script = PtrAt(at::kPanelScript);
    const auto product = static_cast<std::int32_t>(script[2] * L(at::kRecord2 + 0xC));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, static_cast<U>(product / 255 + 8));
    s = S();
    s[6] = static_cast<unsigned char>(SL(s + 0x10) / 15);
    s = S();
    if (s[6] > 3) {
        s[6] = 3;
        s = S();
    }
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) - (rv::As<records::EffectWork>(s)->work_10 << 15));
    s = S();
    Kind2ZClamp(SL(s + 0x10));
    MoveScript_F3Divisor = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, L(at::kKind05Rise + S()[6] * 8u));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, L(at::kKind05Rise + 4 + S()[6] * 8u));
    S()[0x24] = 0;
    S()[0x29] = 5;
    S()[0x48] = 1;
    S()[0] = static_cast<unsigned char>(S()[0] & 0xDF);
    S()[0xB] = 0;
    S()[0x5D] = 0;
    S()[0x5E] = 0;
    S()[0x5F] = 0;
    rv::Current<records::EffectObject>()->depth = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[0], 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[1], 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[2], 0);
    S()[0x2A] = 0;
    S()[0x48] = 1;
    SH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(W(PtrAt(at::kPanelSet) + 0xC)));
    SH_CALL(Sprite_SetAnimation)(1);
    const unsigned char spawned = SH_CALL(Effect_FindFree)();
    if (spawned != 0xFF) {
        unsigned char* const e = Spawned("EffectKind05_Start", spawned);
        e[0] = 1;
        e[5] = 0xA;
    }
    S()[7] = PtrAt(at::kPanelSet)[0xF];
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x465540 (EffectKind05_States[2]): Field_Kind2Z the target +0x10
// clamped; at the target (z +0x38 equal to it): Sound_PlayEffect(0x201), the
// rise and its step +0x14 / +0x20, the height +0x3C, +9 and +0xA cleared, and
// unless Field_Kind2Hold, +0x10 0, MoveScript_F3Divisor 0 and +1 up;
// Sprite_SetAnimation(4), Sprite_UpdateScreen. Otherwise: at the row's offset
// from the target, +0x20 = the row's step; +0x14 += +0x20; past the offset
// with +0x14 negative, both 0; x += +0xC, z -= 0x4000, the height += +0x14;
// MoveScript_F3Divisor 0x40 at z 0x3C0000; Sprite_ScriptTick,
// Sprite_UpdateScreen. PSX 0x801D23F0 (callers).
extern "C" void __cdecl EffectKind05_Rise(void) {
    unsigned char* a = S();
    Kind2ZClamp(SL(a + 0x10));
    const U z = static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->z);
    const U target = rv::As<records::EffectWork>(a)->work_10;
    if (z == target) {
        SH_CALL(Sound_PlayEffect)(0x201);
        rv::Current<records::EffectObject>()->height = 0;
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0);
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, 0);
        S()[9] = 0;
        S()[0xA] = 0;
        if (Field_Kind2Hold == 0) {
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0);
            a = S();
            MoveScript_F3Divisor = 0;
            a[1] = static_cast<unsigned char>(a[1] + 1);
        }
        SH_CALL(Sprite_SetAnimation)(4);
        SH_CALL(Sprite_UpdateScreen)();
        return;
    }
    const U row = Row05("EffectKind05_Rise", a[6]);
    if (z == L(at::kKind05Bounce + row) + target) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->work_20, L(at::kKind05Bounce + 4 + row));
        a = S();
    }
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->work_20));
    a = S();
    const U row2 = Row05("EffectKind05_Rise", a[6]);
    if (rv::As<records::EffectObject>(a)->z > static_cast<std::int32_t>(L(at::kKind05Bounce + row2) + rv::As<records::EffectWork>(a)->work_10) && SL(a + 0x14) < 0) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y, 0);
        a = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->work_20, 0);
        a = S();
    }
    rv::As<records::EffectObject>(a)->x = static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->x) + rv::As<records::EffectWork>(a)->work_0c;
    a = S();
    rv::As<records::EffectObject>(a)->z = static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->z) - 0x4000u;
    a = S();
    rv::As<records::EffectObject>(a)->height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->height) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y);
    a = S();
    if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->z) == 0x3C0000) MoveScript_F3Divisor = 0x40;
    SH_CALL(Sprite_ScriptTick)();
    SH_CALL(Sprite_UpdateScreen)();
}

// original 0x4656A0 (EffectKind05_States[3]): when Sprite_ScriptTickOnce
// answers non-zero, Sprite_SetAnimation(3) and +1 up; Sprite_UpdateScreen (a
// tail jump). PSX 0x801D25EC (table-anchored).
extern "C" void __cdecl EffectKind05_Land(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0) {
        SH_CALL(Sprite_SetAnimation)(3);
        S()[1] = static_cast<unsigned char>(S()[1] + 1);
    }
    SH_CALL(Sprite_UpdateScreen)();
}

// original 0x4656C0 (EffectKind05_States[4]): the wait +0xA counted down; at
// 0, while +7 is above the block's signed +0xF, +0xA = the block's +0x10 and
// +7 down. x += +0xC, z += +0x10, +0x14 += +0x20, the height +0x3C += +0x14.
// A height above 0 with no step +0x20 and +0xB set: the rise 0x40000, the step
// -0x8000 and a kind-0xA record spawned; with +0xB clear: the height 0; with a
// step: +0xB 1. A height of 0: +0xB 0; below 0: the step 0 and +0xB 0. Then
// +0x5C 0; +0 bit 5 set when the word +0x3E is not above 0, else cleared with
// +0x5D 0; +0x5E / +0x5F = +0x5D; Sprite_EnsureAnimation(2 / 3 / 0 by +0x10
// above, at, below 0), Sprite_ScriptTick, +0x48 = 2, 0x52CD50. PSX 0x801D2644.
extern "C" void __cdecl EffectKind05_Bounce(void) {
    unsigned char* a = S();
    if (a[0xA] != 0) {
        a[0xA] = static_cast<unsigned char>(a[0xA] - 1);
        a = S();
        if (a[0xA] == 0) {
            const unsigned char* const set = PtrAt(at::kPanelSet);
            if (static_cast<std::int32_t>(a[7]) > static_cast<signed char>(set[0xF])) {
                a[0xA] = set[0x10];
                a = S();
                a[7] = static_cast<unsigned char>(a[7] - 1);
                a = S();
            }
        }
    }
    rv::As<records::EffectObject>(a)->x = static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->x) + rv::As<records::EffectWork>(a)->work_0c;
    a = S();
    rv::As<records::EffectObject>(a)->z = static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->z) + rv::As<records::EffectWork>(a)->work_10;
    a = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->work_20));
    a = S();
    rv::As<records::EffectObject>(a)->height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->height) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y);
    a = S();
    const std::int32_t height = rv::As<records::EffectObject>(a)->height;
    if (height > 0) {
        if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->work_20) != 0) {
            a[0xB] = 1;
        } else if (a[0xB] == 0) {
            rv::As<records::EffectObject>(a)->height = 0;
        } else {
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y, 0x40000);
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, 0xFFFF8000u);
            const unsigned char spawned = SH_CALL(Effect_FindFree)();
            if (spawned != 0xFF) {
                unsigned char* const e = Spawned("EffectKind05_Bounce", spawned);
                e[0] = 1;
                e[5] = 0xA;
            }
        }
    } else if (height == 0) {
        a[0xB] = 0;   // 0x4657A0 jge lands on 0x4657DA's jle, taken at 0
    } else {
        if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->work_20) != 0) {
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->work_20, 0);
            a = S();
        }
        a[0xB] = 0;
    }
    S()[0x5C] = 0;
    a = S();
    const unsigned char flags = a[0];
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole)) > 0) {
        a[0] = static_cast<unsigned char>(flags & 0xDF);
        S()[0x5D] = 0;
    } else {
        a[0] = static_cast<unsigned char>(flags | 0x20);
    }
    a = S();
    a[0x5E] = a[0x5D];
    a = S();
    a[0x5F] = a[0x5D];
    const std::int32_t rise = SL(S() + 0x10);
    SH_CALL(Sprite_EnsureAnimation)(rise > 0 ? 2 : rise == 0 ? 3 : 0);
    SH_CALL(Sprite_ScriptTick)();
    S()[0x48] = 2;
    DepthPair();
}

// original 0x465840 (EffectKind05_States[5]): Sprite_EnsureAnimation(3 while
// record 5's +0x10 is not negative, else 2); x, z and the height word from the
// Sprite_Objects record the byte 0x939A1C names; Sprite_ScriptTick; 0x52CD50
// (a tail jump). PSX 0x801D28D8.
extern "C" void __cdecl EffectKind05_Follow(void) {
    SH_CALL(Sprite_EnsureAnimation)(SL(At(Effect_Objects_addr + 0x280 + 0x10)) >= 0 ? 3 : 2);
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(rv::As<records::SpriteObject>(Object("EffectKind05_Follow", Effect1a_PanelObject))->x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(rv::As<records::SpriteObject>(Object("EffectKind05_Follow", Effect1a_PanelObject))->z));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, static_cast<std::uint32_t>(rv::As<records::SpriteObject>(Object("EffectKind05_Follow", Effect1a_PanelObject))->height_whole));
    SH_CALL(Sprite_ScriptTick)();
    DepthPair();
}

// ===========================================================================
// Kind 0xA (EffectKind0A_States: _Start, _Follow): record 0's sprite copied
// ===========================================================================

// original 0x4658E0 (EffectKind0A_States[0]): record 0's bytes +0x24..+0x28
// copied; +0x29 = 5; the height +0x3C 0; +0x48, +0x5C 0; +0x5D..+0x5F 0x9C;
// +0x60..+0x6C 0; EffectKind0A_Follow; +1 up.
extern "C" void __cdecl EffectKind0A_Start(void) {
    for (U i = 0x24; i <= 0x28; ++i) S()[i] = B(at::kRecord0 + i);
    S()[0x29] = 5;
    rv::Current<records::EffectObject>()->height = 0;
    S()[0x48] = 0;
    S()[0x5C] = 0;
    S()[0x5D] = 0x9C;
    S()[0x5E] = 0x9C;
    S()[0x5F] = 0x9C;
    rv::Current<records::EffectObject>()->depth = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[0], 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[1], 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->angle[2], 0);
    SH_CALL(EffectKind0A_Follow)();
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x4659A0 (EffectKind0A_States[1], and _Start's): while record 0's
// height word +0x3E is above 0 and its +1 is set, record 0's x and z, +0x2A,
// +0x49..+0x4B, +0x50, +0x54, +0x58, +0x5A copied and Sprite_UpdateScreen (a
// tail jump); else Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind0A_Follow(void) {
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(Effect_Objects[0].height_whole)) <= 0 || Effect_Objects[0].state == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(Effect_Objects[0].x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(Effect_Objects[0].z));
    S()[0x2A] = Effect_Objects[0].flip;
    S()[0x49] = Effect_Objects[0].script_steps;
    S()[0x4A] = Effect_Objects[0].script_hold;
    S()[0x4B] = Effect_Objects[0].animation;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->script, Effect_Objects[0].script);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->frames, Effect_Objects[0].frames);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->script_pos, static_cast<std::uint32_t>(Effect_Objects[0].script_pos));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->frame_word, static_cast<std::uint32_t>(Effect_Objects[0].frame_word));
    SH_CALL(Sprite_UpdateScreen)();
}

// ===========================================================================
// Kind 0xC (EffectKind0C_States: BareRet, _Start, _Aim)
// ===========================================================================

// original 0x465A80 (EffectKind0C_States[1]): the word +0x30, +9, +0xA, +0xB
// 0; +1 up.
extern "C" void __cdecl EffectKind0C_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    S()[9] = 0;
    S()[0xA] = 0;
    S()[0xB] = 0;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x465AB0 (EffectKind0C_States[2]): the draw mode (0, 2), panel
// sprite 0x11 at (0x87, 0xB4). With no step +0x10: when the object's (the byte
// 0x939A1C) signed +0xA lies more than the parameters' [3] * 4 + 0xC from the
// word +0x30, +9 counts to 0xF and then +0x10 = -1 / 1 toward it; else +9 0.
// +0x30 += +0x10's word, clamped to [3] * 4 - 0x24 .. (9 - [3]) * 4. A LINE_F2
// (185, 201) - (185, 213), shade 0x80, slot 2 (0x20). EffectHud_DrawMark at
// (+0x30 - [3] * 4 + 0xAD, 0xCB), width ([3] + 3) * 8, shade (+0xA & 7) * 2,
// blinking when the object's +0xA + 0xB9 lies outside the mark. When the
// object's +1 is 4: a LINE_F2 (136, 211) - (its +0x9A * 0x60 / 256 + 0x88,
// 211), red, slot 2; EffectHud_DrawArrow at (its +0xA + 0xB9 held to
// 0x90..0xE4, 0xCB), flipped while its +0x10 is not negative; and when ObjTrio
// 0's +2 is 4 and the object's +4 is not 3, Sound_PlayEffect(0x207) every
// 16th frame the arrow lies outside the mark. PSX 0x801D2DC0 (callers).
extern "C" void __cdecl EffectKind0C_Aim(void) {
    static const char kWho[] = "EffectKind0C_Aim";
    DrawMode(0, 2);
    DrawSprite(0x11, 2, 0x87, 0xB4);
    unsigned char* a = S();
    bool settled = true;
    if (rv::As<records::EffectWork>(a)->work_10 == 0) {
        const unsigned margin = PtrAt(at::kPanelScript)[3] * 4u + 0xC;
        const std::int16_t pos = static_cast<std::int16_t>(static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_30));
        const signed char aim = static_cast<signed char>(Object(kWho, Effect1a_PanelObject)[0xA]);
        const std::int32_t near_hi = pos + static_cast<std::int32_t>(margin);
        const std::int32_t near_lo = pos - static_cast<std::int32_t>(margin);
        if (aim > near_hi || aim < near_lo) {
            settled = false;
            if (a[9] < 0xF) {
                a[9] = static_cast<unsigned char>(a[9] + 1);
                a = S();
            } else if (pos > aim) {
                rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_10, 0xFFFFFFFFu);
                a = S();
            } else if (pos < aim) {
                rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_10, 1);
                a = S();
            }
        }
    }
    if (settled) {
        a[9] = 0;
        a = S();
    }
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_30, static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_30) + W(a + 0x10));
    const unsigned b3 = PtrAt(at::kPanelScript)[3];
    a = S();
    const std::int32_t pos = SW(a + 0x30);
    const std::int32_t lo = static_cast<std::int32_t>(b3 * 4) - 0x24;
    std::int32_t held;
    if (pos < lo) {
        held = lo;
    } else {
        const std::int32_t hi = (9 - static_cast<std::int32_t>(b3)) * 4;
        held = pos > hi ? hi : pos;
    }
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_30, static_cast<U>(held));
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SetL(p + 8, 0x43390000);
    SetL(p + 0x14, 0x43390000);
    SetL(p + 0xC, 0x43490000);
    SetL(p + 0x18, 0x43550000);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    SH_CALL(Gfx_CommitPrim)(2, 0x20);
    const unsigned b3b = PtrAt(at::kPanelScript)[3];
    const auto width = static_cast<unsigned char>((b3b + 3) << 3);
    a = S();
    const auto left = static_cast<std::uint16_t>(W(a + 0x30) - 4 * b3b + 0xAD);
    const auto right = static_cast<std::uint16_t>(left + 8 * b3b + 0x18);
    const auto aim = static_cast<std::int16_t>(static_cast<signed char>(Object(kWho, Effect1a_PanelObject)[0xA]) + 0xB9);
    const bool outside = aim > static_cast<std::int16_t>(right) || aim < static_cast<std::int16_t>(left);
    SH_CALL(EffectHud_DrawMark)(left, 0xCB, width, static_cast<unsigned char>((a[0xA] & 7) << 1), 2, outside ? 1 : 0);
    if (Object(kWho, Effect1a_PanelObject)[1] != 4) return;
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SetL(p + 8, 0x43080000);
    const std::int32_t scaled = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int16_t>(rv::As<records::SpriteObject>(Object(kWho, Effect1a_PanelObject))->half_d)) * 0x60u);
    p[4] = 0x80;
    p[5] = 0;
    p[6] = 0;
    SetL(p + 0xC, 0x43530000);
    SetL(p + 0x18, 0x43530000);
    Fild(scaled / 256 + 0x88, p + 0x14);
    SH_CALL(Gfx_CommitPrim)(2, 0x20);
    const unsigned flip = rv::As<records::SpriteObject>(Object(kWho, Effect1a_PanelObject))->step_z >= 0 ? 1 : 0;
    std::int16_t arrow = aim;
    if (arrow < 0x90)
        arrow = 0x90;
    else if (arrow > 0xE4)
        arrow = 0xE4;
    SH_CALL(EffectHud_DrawArrow)(static_cast<std::uint16_t>(arrow), 0xCB, flip, 2);
    if (ObjTrio[2] != 4) return;
    if (Object(kWho, Effect1a_PanelObject)[4] == 3) return;
    if (arrow <= static_cast<std::int16_t>(right) && arrow >= static_cast<std::int16_t>(left)) return;
    if ((Frame_Counter & 0xF) != 0) return;
    SH_CALL(Sound_PlayEffect)(0x207);
}

// ===========================================================================
// Kind 0xD (EffectKind0D_States: BareRet, _Start, _SlideIn, _Hold, _SlideOut)
// ===========================================================================

// original 0x465F30 (EffectKind0D_States[1]): Sprite_SetAnimationBank(0x2B);
// the screen words +0x2E = 0x186, +0x30 = 0x6E; +0x24 0x80; +0x48, +0x5D..+0x5F,
// +0x2A 0; +0x29 2; Sprite_SetAnimation(+6); Sound_PlayEffect(0x653B8C's word
// +6) unless 0xFFFF; +1 up.
extern "C" void __cdecl EffectKind0D_Start(void) {
    SH_CALL(Sprite_SetAnimationBank)(0x2B);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0x186);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0x6E);
    S()[0x24] = 0x80;
    S()[0x48] = 0;
    S()[0x29] = 2;
    S()[0x5D] = 0;
    S()[0x5E] = 0;
    S()[0x5F] = 0;
    S()[0x2A] = 0;
    SH_CALL(Sprite_SetAnimation)(S()[6]);
    unsigned char* const a = S();
    const unsigned v = a[6];
    if (v >= at::kKind0DSoundCount)
        bof3::Fatal("EffectKind0D_Start: +6 is %u, past the six words of 0x653B8C (docs/effect_1a.md section 6)", v);
    const U id = W(at::kKind0DSounds + 2 * v);
    if (id != 0xFFFF) {
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id));
        S()[1] = static_cast<unsigned char>(S()[1] + 1);
    } else {
        a[1] = static_cast<unsigned char>(a[1] + 1);
    }
}

// original 0x465FE0 (EffectKind0D_States[2]): +0x2E -= 0x1E; at 0xA0 or less
// (s16) it is held there, +9 = 0x1E and +1 up; Sprite_QueueOverlay (a tail jump).
extern "C" void __cdecl EffectKind0D_SlideIn(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e) - 0x1E);
    unsigned char* const a = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(a)->work_2e)) <= 0xA0) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e, 0xA0);
        S()[9] = 0x1E;
        S()[1] = static_cast<unsigned char>(S()[1] + 1);
    }
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x466020 (EffectKind0D_States[3]): +9 down, +1 up at 0;
// Sprite_QueueOverlay (a tail jump).
extern "C" void __cdecl EffectKind0D_Hold(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* const a = S();
    if (a[9] == 0) a[1] = static_cast<unsigned char>(a[1] + 1);
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x466050 (EffectKind0D_States[4]): +0x2E -= 0x1E; at -0x46 or less
// (s16) +0x2E = 0xA0 and +1 = 0 (nothing drawn); else Sprite_QueueOverlay.
extern "C" void __cdecl EffectKind0D_SlideOut(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e) - 0x1E);
    unsigned char* const a = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(a)->work_2e)) <= -0x46) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e, 0xA0);
        S()[1] = 0;
        return;
    }
    SH_CALL(Sprite_QueueOverlay)();
}

// ===========================================================================
// Kind 0xF (EffectKind0F_States: BareRet, _Start, _Open, _Choose, _Title,
// _TitleClose, _Frame, _Close, _LineStart, _LineType, _LineNext, _LineScroll,
// _LineFade): a message window at y +0x30 and the lines it types
// ===========================================================================

// original 0x4660B0 (EffectKind0F_States[1]): the window's y +0x30 = -0x19,
// +7 0, +1 up. PSX 0x801D364C.
extern "C" void __cdecl EffectKind0F_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0xFFE7);
    S()[7] = 0;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x4660D0 (EffectKind0F_States[2]): +0x30 += 8, held at 0x12 (s16)
// with +1 up; the window 0x469750(0x14, +0x30, 0x118, 0x13, 0). PSX 0x801D3680.
extern "C" void __cdecl EffectKind0F_Open(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30) + 8);
    unsigned char* a = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(a)->work_30)) > 0x12) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(a)->work_30, 0x12);
        a = S();
        a[1] = static_cast<unsigned char>(a[1] + 1);
        a = S();
    }
    WindowBox(0x14, Hi(Key(a)) | static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_30), 0x118, 0x13, 0);
}

// original 0x466120 (EffectKind0F_States[3]): with the spawn byte of 0x653C04's
// row +6 set, a kind-0xF record spawned at state 8 with +6 copied (+0x4A,
// +0xB 0) when Effect_FindFree has one, and +1 = 6; else +0x4B = the row's
// first text and +1 up. The window at y 0x12. PSX 0x801D36F4.
extern "C" void __cdecl EffectKind0F_Choose(void) {
    static const char kWho[] = "EffectKind0F_Choose";
    unsigned char* s = S();
    if (B(RowByte(kWho, s[6], 3)) != 0) {
        const unsigned char spawned = SH_CALL(Effect_FindFree)();
        s = S();
        if (spawned != 0xFF) {
            unsigned char* const e = Spawned(kWho, static_cast<signed char>(spawned));
            e[0] = 1;
            e[5] = 0xF;
            e[6] = s[6];
            e[0x4A] = 0;
            e[0xB] = 0;
            e[1] = 8;
        }
        s[1] = 6;
    } else {
        s[0x4B] = B(RowByte(kWho, s[6], 0));
        S()[1] = static_cast<unsigned char>(S()[1] + 1);
    }
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
}

// original 0x4661B0 (EffectKind0F_States[4]): the window at 0x12; the text
// +0x4B names drawn whole at (0x1D, 0x14). PSX 0x801D3820.
extern "C" void __cdecl EffectKind0F_Title(void) {
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    const U text = L(Text("EffectKind0F_Title", S()[0x4B]));
    DrawText(0x1D, 0x14, 0, StringCount(text), text);
}

// original 0x466200 (EffectKind0F_States[5]): EffectKind0F_Close; the text
// +0x4B names drawn whole at (0x1D, +0x30 + 2). PSX 0x801D3894.
extern "C" void __cdecl EffectKind0F_TitleClose(void) {
    SH_CALL(EffectKind0F_Close)();
    const U text = L(Text("EffectKind0F_TitleClose", S()[0x4B]));
    const U count = StringCount(text);
    DrawText(0x1D, (static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30) + 2) & 0xFFFF, 0, count, text);
}

// original 0x466240 (EffectKind0F_States[6]): the window at 0x12. PSX 0x801D3904.
extern "C" void __cdecl EffectKind0F_Frame(void) { WindowBox(0x14, 0x12, 0x118, 0x13, 0); }

// original 0x466260 (EffectKind0F_States[7], and _TitleClose's): +0x30 -= 8;
// below -0x19 (s16) +1 = 1 when +7 is set, else 0; the window at +0x30. PSX
// 0x801D3934.
extern "C" void __cdecl EffectKind0F_Close(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30) - 8);
    unsigned char* a = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(a)->work_30)) < -0x19) {
        a[1] = a[7] != 0 ? 1 : 0;
        a = S();
    }
    WindowBox(0x14, Hi(Key(a)) | static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_30), 0x118, 0x13, 0);
}

// original 0x4662B0 (EffectKind0F_States[8]): +0x4B = 0x653C04's byte at row
// +6, column +0x4A; +0x50 = that text; the word +0x58, +0x49 0; +9 6; the x
// word +0x2E 0x125; +1 up. PSX 0x801D39B4.
extern "C" void __cdecl EffectKind0F_LineStart(void) {
    static const char kWho[] = "EffectKind0F_LineStart";
    unsigned char* a = S();
    a[0x4B] = B(RowByte(kWho, a[6], a[0x4A]));
    a = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->script, L(Text(kWho, a[0x4B])));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->script_pos, 0);
    S()[0x49] = 0;
    // DIVERGENCE DIV-0069: under a Latin overlay the first character is typed
    // after half its own advance (the US module's 4 for 8 units), so it lands
    // where the flip cursor ends; 6 for a 12-unit glyph, the original's.
    S()[9] = FishingText_On() ? static_cast<unsigned char>(FishingText_HalfAdvance(TextAt(rv::As<records::EffectObject>(S())->script))) : 6;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0x125);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x466310 (EffectKind0F_States[9]): +0x2E -= 2; +9 down. At 0: the
// cursor +0x50 past one character (two when its first byte has bit 7), +0x49
// up, +9 6; with the whole text typed (+0x49 not below its count, 0x5171E0),
// +0x49 = the count, +0xA = 6 when the text has a line (+4 not 0xFF) else 0,
// +9 = the text's pause byte +5, +0x50 back to its start, +1 up. While +9
// runs: 0x469AD0(0, +0x50, 0xC - 2 * +9, 2 * +9 + 0x119) (the typed
// character's cursor). Then, with +0x49 characters, the text drawn at (+0x2E,
// record 3's +0x30 + 2). PSX 0x801D3A50.
extern "C" void __cdecl EffectKind0F_LineType(void) {
    static const char kWho[] = "EffectKind0F_LineType";
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e) - 2);
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* c = S();
    const unsigned char left = c[9];
    U eax;   // the register the original reads +0x49 into at the end
    if (left == 0) {
        const U cursor = rv::As<records::EffectObject>(c)->script;
        if (At(cursor)[0] & 0x80) {
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(c)->script, cursor + 1);
            c = S();
        }
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(c)->script, rv::As<records::EffectObject>(c)->script + 1);
        S()[0x49] = static_cast<unsigned char>(S()[0x49] + 1);
        // DIV-0069: the next character after half its advance (LineStart's
        // note); at the NUL the branch below sets the pause anyway.
        const unsigned char* const next = TextAt(rv::As<records::EffectObject>(S())->script);
        S()[9] = FishingText_On() && next[0] != 0 ? static_cast<unsigned char>(FishingText_HalfAdvance(next)) : 6;
        const U count = StringCount(L(Text(kWho, S()[0x4B])));
        c = S();
        if (static_cast<std::int32_t>(c[0x49]) < static_cast<std::int32_t>(count)) {
            eax = count;
        } else {
            const U whole = StringCount(L(Text(kWho, c[0x4B])));
            S()[0x49] = static_cast<unsigned char>(whole);
            unsigned char* a = S();
            a[0xA] = B(Text(kWho, a[0x4B]) + 4) != 0xFF ? 6 : 0;
            a = S();
            a[9] = B(Text(kWho, a[0x4B]) + 5);
            a = S();
            SetL(a + 0x50, L(Text(kWho, a[0x4B])));
            a = S();
            a[1] = static_cast<unsigned char>(a[1] + 1);
            eax = Key(a);
            c = S();
        }
    } else if (g_empty_line_none && TextAt(rv::As<records::EffectObject>(c)->script)[0] == 0) {
        // DIVERGENCE DIV-0144 (D279): an empty line types no cursor glyph. The
        // cursor rests on a NUL here only when the line has no character (at
        // the last one's NUL the branch above has set the pause and gone on),
        // and the original draws that NUL as glyph 0 - 0x26 = 0xFFDA, past the
        // font: garbage, or an access violation in Font_UnpackGlyph. The PC's
        // thirteen lines are never empty; a Japanese disc-built image's are,
        // when no layer writes them.
        if (c[0x49] != 0)
            bof3::Fatal("%s: the cursor is on a NUL with %u characters typed - an empty line has none "
                        "(docs/known-defects.md D279)", kWho, (unsigned)c[0x49]);
        return;
    } else if (FishingText_On()) {
        // DIV-0069: the flip cursor as wide as the character's advance A, its
        // right edge at 0x125 as the original's (12 wide at 2 * left + 0x119):
        // the US module's 8 - 2 * left at 2 * left + 0x11D for its 8 units.
        const U a = 2u * FishingText_HalfAdvance(TextAt(rv::As<records::EffectObject>(c)->script));
        eax = MessageLine(0, rv::As<records::EffectObject>(c)->script, static_cast<unsigned char>(a - (left << 1)), (2u * left + 0x125 - a) & 0xFFFF);
        c = S();
    } else {
        eax = MessageLine(0, rv::As<records::EffectObject>(c)->script, static_cast<unsigned char>(0xC - (left << 1)), (2u * left + 0x119) & 0xFFFF);
        c = S();
    }
    const unsigned char typed = c[0x49];
    if (typed == 0) return;
    DrawText(static_cast<std::uint32_t>(rv::As<records::EffectWork>(c)->work_2e), L(at::kRecord3 + 0x30) + 2, 0, (eax & 0xFFFFFF00u) | typed, L(Text(kWho, c[0x4B])));
}

// original 0x466460 (EffectKind0F_States[10]): +0x2E -= 2. While the line wait
// +0xA runs down: the label's line 0x469AD0(the label's pen byte 0x653C00,
// its text 0x66A2FC, 0xC - 2 * +0xA, 2 * +0xA + 0x119). Else +9 down; at 0 the
// column +0x4A up (back to 0 at 3 or an 0xFF in the row), a kind-0xF record at
// state 8 spawned with +6 and +0x4A (+0xB 0) unless record 3's +1 is 7, and +1
// up; the label drawn (one character, its pen) at (+0x2E + 12 * +0x49, record
// 3's +0x30 + 2) when the text has one. Then the typed text drawn at (+0x2E,
// record 3's +0x30 + 2). PSX 0x801D3C80.
extern "C" void __cdecl EffectKind0F_LineNext(void) {
    static const char kWho[] = "EffectKind0F_LineNext";
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e) - 2);
    unsigned char* c = S();
    if (c[0xA] != 0) {
        c[0xA] = static_cast<unsigned char>(c[0xA] - 1);
        c = S();
        const unsigned char wait = c[0xA];
        // DIV-0069 / DIV-0136: the label's icon and pen for its button now.
        const LabelDraw label = Label(kWho, c[0x4B]);
        MessageLine(label.pen, label.text, static_cast<unsigned char>(0xC - (wait << 1)), (2u * wait + 0x119) & 0xFFFF);
    } else {
        c[9] = static_cast<unsigned char>(c[9] - 1);
        unsigned char* a = S();
        if (a[9] == 0) {
            a[0x4A] = static_cast<unsigned char>(a[0x4A] + 1);
            c = S();
            const unsigned column = c[0x4A];
            if (column >= 3 || B(RowByte(kWho, c[6], column)) == 0xFF) {
                c[0x4A] = 0;
                c = S();
            }
            if (Effect_Objects[3].state != 7) {
                const unsigned char spawned = SH_CALL(Effect_FindFree)();
                c = S();
                if (spawned != 0xFF) {
                    unsigned char* const e = Spawned(kWho, static_cast<signed char>(spawned));
                    e[0] = 1;
                    e[5] = 0xF;
                    e[1] = 8;
                    e[6] = c[6];
                    e[0x4A] = c[0x4A];
                    e[0xB] = 0;
                }
            }
            c[1] = static_cast<unsigned char>(c[1] + 1);
            a = S();
        }
        if (B(Text(kWho, a[0x4B]) + 4) != 0xFF) {
            const LabelDraw label = Label(kWho, a[0x4B]);   // DIV-0069 / DIV-0136
            if (FishingText_On()) {
                // DIV-0069: after the line's real pen width, not 12 a character.
                const U width = FishingText_Width(TextAt(L(Text(kWho, a[0x4B]))), a[0x49]);
                DrawText(static_cast<std::uint16_t>(width + static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e)), L(at::kRecord3 + 0x30) + 2, label.pen, 1,
                         label.text);
            } else {
                DrawText(static_cast<std::uint16_t>(a[0x49] * 12u + static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e)), L(at::kRecord3 + 0x30) + 2,
                         label.pen, 1, label.text);
            }
        }
    }
    unsigned char* const a = S();
    DrawText(static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e), L(at::kRecord3 + 0x30) + 2, 0, a[0x49], rv::As<records::EffectObject>(a)->script);
}

// original 0x4665E0 (EffectKind0F_States[11]): +0x2E -= 2; left of 0x1D (s16)
// with characters left: the text scrolls - +0x54 = the cursor +0x50, which
// moves past one character (two with bit 7), +0x49 down, +0x2E 0x27, +9 6.
// While +9 runs down: 0x469AD0(0, +0x54, (2 * +9) | 0x80, 0x1D) (the leaving
// character, fading). With characters: the text at (+0x2E, record 3's +0x30 +
// 2) and the label at (+0x2E + 12 * +0x49, ..) when the text has one. Without:
// the label at (+0x2E, ..), then at +0x2E 0x1D +9 6 and +1 up - or, when the
// text has no label, Effect_Release (a tail jump). PSX 0x801D3F54.
extern "C" void __cdecl EffectKind0F_LineScroll(void) {
    static const char kWho[] = "EffectKind0F_LineScroll";
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e) - 2);
    unsigned char* a = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(a)->work_2e)) < 0x1D && a[0x49] != 0) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->frames, rv::As<records::EffectObject>(a)->script);
        a = S();
        const U cursor = rv::As<records::EffectObject>(a)->script;
        if (At(cursor)[0] & 0x80) {
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->script, cursor + 1);
            a = S();
        }
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->script, rv::As<records::EffectObject>(a)->script + 1);
        a = S();
        a[0x49] = static_cast<unsigned char>(a[0x49] - 1);
        if (FishingText_On()) {
            // DIV-0069: the line moves on by the leaving character's advance (an
            // even one, so +0x2E stays odd and still meets 0x1D below), and
            // it leaves over half that in frames; 0x27 = 0x1B + 12 and 6 for
            // a 12-unit glyph, the original's.
            const unsigned half = FishingText_HalfAdvance(TextAt(rv::As<records::EffectObject>(a)->frames));
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0x1B + 2 * half);
            S()[9] = static_cast<unsigned char>(half);
        } else {
            rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0x27);
            S()[9] = 6;
        }
        a = S();
    }
    if (a[9] != 0) {
        a[9] = static_cast<unsigned char>(a[9] - 1);
        a = S();
        MessageLine(0, rv::As<records::EffectObject>(a)->frames, static_cast<unsigned char>((a[9] << 1) | 0x80), 0x1D);
        a = S();
    }
    const unsigned char typed = a[0x49];
    if (typed != 0) {
        DrawText(static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e), L(at::kRecord3 + 0x30) + 2, 0, typed, rv::As<records::EffectObject>(a)->script);
        unsigned char* const c = S();
        if (B(Text(kWho, c[0x4B]) + 4) == 0xFF) return;
        const LabelDraw label = Label(kWho, c[0x4B]);   // DIV-0069 / DIV-0136
        if (FishingText_On()) {
            // DIV-0069: after the typed characters' real width.
            const U width = FishingText_Width(TextAt(rv::As<records::EffectObject>(c)->script), c[0x49]);
            DrawText(static_cast<std::uint16_t>(width + static_cast<std::uint32_t>(rv::As<records::EffectWork>(c)->work_2e)), L(at::kRecord3 + 0x30) + 2, label.pen, 1,
                     label.text);
        } else {
            DrawText(static_cast<std::uint16_t>(c[0x49] * 12u + static_cast<std::uint32_t>(rv::As<records::EffectWork>(c)->work_2e)), L(at::kRecord3 + 0x30) + 2, label.pen,
                     1, label.text);
        }
        return;
    }
    if (B(Text(kWho, a[0x4B]) + 4) == 0xFF) {
        SH_CALL(Effect_Release)();
        return;
    }
    const LabelDraw label = Label(kWho, a[0x4B]);   // DIV-0069 / DIV-0136
    DrawText(static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e), L(at::kRecord3 + 0x30) + 2, label.pen, 1, label.text);
    a = S();
    if (static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e) != 0x1D) return;
    a[9] = 6;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x466750 (EffectKind0F_States[12]): +9 down; at 0 Effect_Release
// (a tail jump); else the label's line 0x469AD0(its pen, its text, (2 * +9) |
// 0x80, 0x1D), fading. PSX 0x801D4200.
extern "C" void __cdecl EffectKind0F_LineFade(void) {
    static const char kWho[] = "EffectKind0F_LineFade";
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* const c = S();
    const unsigned char left = c[9];
    if (left == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    const LabelDraw label = Label(kWho, c[0x4B]);   // DIV-0069 / DIV-0136
    MessageLine(label.pen, label.text, static_cast<unsigned char>((left << 1) | 0x80), 0x1D);
}

// ===========================================================================
// Kind 0x1A (EffectKind1A_States: BareRet, _Start, _Open, _ItemsA, _ItemsB,
// _Close, _FindKind, _PanelIn, _Panel, _PanelNext, _PanelBack, _PanelOut,
// _Reset, then E1B's 0x4672F0..): the window with option boxes, member rows
// and item lists, and the kind panel sliding by +9
// ===========================================================================

namespace {
// The window's slide by +9 (0..4): the x of the option boxes and the member
// rows, the item lists' x - each from the byte +9 loaded into the low half of a
// register the original held a pointer or an answer in (the high half kept).
U Slide(U reg, unsigned char nine) { return Hi(reg) | nine; }
// The window, the option boxes, the member rows and one of the item lists at
// +9's slide (_Open, _Close): the window and boxes sliding until +0xB is set.
void WindowAndLists() {
    unsigned char* a = S();
    U answer;
    if (a[0xB] == 0) {
        const U v = Slide(Key(a), a[9]);
        answer = WindowBox(0x14, 0x12 - (v << 4), 0x118, 0x13, 0);
        const U w = Slide(answer, S()[9]);
        answer = OptionBoxes(w * 45 + 0x58, 0x28, 0);
    } else {
        WindowBox(0x14, 0x12, 0x118, 0x13, 0);
        answer = OptionBoxes(0x58, 0x28, OptionBits());
    }
    const U rows = Slide(answer, S()[9]);
    MemberRows(0x10 - rows * 45, 0x40);
    a = S();
    const unsigned char seven = a[7];
    const U lists = Slide(Key(a), a[9]) * 45 + 0xA4;
    if ((seven & 1) == 0)
        ItemListA(lists, 0x40);
    else
        ItemListB(lists, 0x40);
}
// The panel's kind icon at (0x80 + 48 * +9, 0x5B) and its row (_PanelIn,
// _PanelOut): +0x3E's count byte 0x9040EC decides whether the kind or 0xFF is
// drawn.
void PanelKind() {
    unsigned char* a = S();
    const unsigned char count = B(at::kKindCounts + static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole))));
    const U icon_x = a[9] * 48u + 0x80;
    if (count != 0) {
        SH_CALL(FieldPanel_DrawKindIcon)(static_cast<int>(icon_x), 0x5B, a[0x3E]);
        a = S();
        SH_CALL(FieldPanel_DrawKindRow)(a[0x3E], B(at::kKindCounts + static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole)))), a[9]);
    } else {
        SH_CALL(FieldPanel_DrawKindIcon)(static_cast<int>(icon_x), 0x5B, 0xFF);
        SH_CALL(FieldPanel_DrawKindRow)(0xFF, 0, S()[9]);
    }
}
// The panel's header and row in place (_Panel, _PanelNext, _PanelBack's head):
// the header at (0, 0x4E), the row at 0; whether the kind has a count (_Panel
// goes on by it, read before the row's call).
bool PanelRow() {
    SH_CALL(FieldPanel_DrawHeader)(0, 0x4E);
    unsigned char* const a = S();
    const unsigned char count = B(at::kKindCounts + static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole))));
    if (count != 0)
        SH_CALL(FieldPanel_DrawKindRow)(a[0x3E], count, 0);
    else
        SH_CALL(FieldPanel_DrawKindRow)(0xFF, 0, 0);
    return count != 0;
}
// The panel's tail: the total at (0xB0, 0x9C), the message 0xFFFF at (8,
// 0x94), the title 0x468A40, the window at 0x12 and the option boxes.
void PanelTail() {
    SH_CALL(FieldPanel_DrawTotal)(0xB0, 0x9C);
    SH_CALL(FieldPanel_DrawMessage)(8, 0x94, 0xFFFF);
    PanelTitle();
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
}
}  // namespace

// original 0x4667C0 (EffectKind1A_States[1]): +8, +6 0; +7 0x80; +9 4; +0xA 0;
// x, z, the height, +0xC, +0x10 0; +0xB 0; the word +0x2C 0xFFFF; +1 up. PSX
// 0x801D4300.
extern "C" void __cdecl EffectKind1A_Start(void) {
    S()[8] = 0;
    S()[6] = 0;
    S()[7] = 0x80;
    S()[9] = 4;
    S()[0xA] = 0;
    rv::Current<records::EffectObject>()->x = 0;
    rv::Current<records::EffectObject>()->z = 0;
    rv::Current<records::EffectObject>()->height = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0);
    S()[0xB] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->column, 0xFFFF);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x466840 (EffectKind1A_States[2]): +9 down; at 0 +0xB = 1 and +1
// up (to 4 when +7 has bit 0). The window sliding in by +9 (its y 0x12 - 16 *
// +9, the option boxes at 0x58 + 45 * +9 with none set) until +0xB, then in
// place with ObjTrio 0's option bit; the member rows at 0x10 - 45 * +9; item
// list A (+7 bit 0 clear) or B at 0xA4 + 45 * +9. PSX 0x801D4394.
extern "C" void __cdecl EffectKind1A_Open(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* const a = S();
    if (a[9] == 0) {
        a[0xB] = 1;
        unsigned char* const b = S();
        if (b[7] & 1)
            b[1] = 4;
        else
            b[1] = static_cast<unsigned char>(b[1] + 1);
    }
    WindowAndLists();
}

// original 0x466950 (EffectKind1A_States[3]): the window at 0x12, the option
// boxes with ObjTrio 0's option bit, the member rows at 0x10, item list A at
// 0xA4. PSX 0x801D458C.
extern "C" void __cdecl EffectKind1A_ItemsA(void) {
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
    MemberRows(0x10, 0x40);
    ItemListA(0xA4, 0x40);
}

// original 0x4669B0 (EffectKind1A_States[4]): as _ItemsA with item list B.
// PSX 0x801D4614.
extern "C" void __cdecl EffectKind1A_ItemsB(void) {
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
    MemberRows(0x10, 0x40);
    ItemListB(0xA4, 0x40);
}

// original 0x466A10 (EffectKind1A_States[5]): +9 up and the window and lists
// as _Open's; at +9 4: +1 = 0 without +0xB, else +1 up when +6 is 1, 0xC when
// it is 2. PSX 0x801D469C.
extern "C" void __cdecl EffectKind1A_Close(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    WindowAndLists();
    unsigned char* const a = S();
    if (a[9] != 4) return;
    if (a[0xB] == 0) {
        a[1] = 0;
        return;
    }
    if (a[6] == 1)
        a[1] = static_cast<unsigned char>(a[1] + 1);
    else if (a[6] == 2)
        a[1] = 0xC;
}

// original 0x466B30 (EffectKind1A_States[6]): the word +0x3E walks 0..0x1F
// from 0 to the first kind whose count byte 0x9040EC is set - +0x3C = 1 when
// none is (the walk back at 0); the window at 0x12, the option boxes; +9 = 4,
// +1 up. PSX 0x801D48A0.
extern "C" void __cdecl EffectKind1A_FindKind(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_frac, 0);
    unsigned char* a = S();
    for (;;) {
        const U kind = static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->height_whole);
        if (B(at::kKindCounts + static_cast<U>(static_cast<std::int16_t>(kind))) != 0) break;
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->height_whole, kind + 1);
        a = S();
        if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole)) >= 0x20) {
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->height_whole, 0);
            a = S();
        }
        if (static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->height_whole) == 0) {
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->height_frac, 1);
            break;
        }
    }
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
    S()[9] = 4;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x466BE0 (EffectKind1A_States[7]): +9 down; the panel sliding in by
// it: the header at (-80 * +9, 0x4E), the kind icon at (0x80 + 48 * +9,
// 0x5B) and its row by +9, the total at (0xB0, 0x9C + 30 * +9), the message
// 0xFFFF at (8, 0x94 + 30 * +9); the window at 0x12 and the option boxes; +1
// up at +9 0. PSX 0x801D49A0.
extern "C" void __cdecl EffectKind1A_PanelIn(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    const unsigned char nine = S()[9];
    SH_CALL(FieldPanel_DrawHeader)(-80 * static_cast<int>(nine), 0x4E);
    PanelKind();
    SH_CALL(FieldPanel_DrawTotal)(0xB0, static_cast<int>(S()[9] * 30u + 0x9C));
    SH_CALL(FieldPanel_DrawMessage)(8, static_cast<int>(S()[9] * 30u + 0x94), 0xFFFF);
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
    unsigned char* const a = S();
    if (a[9] == 0) a[1] = static_cast<unsigned char>(a[1] + 1);
}

// original 0x466D30 (EffectKind1A_States[8]): the panel in place - header, the
// kind's row at 0, its icon at (0x80, 0x5B), its message (the word +0x3E) at
// (8, 0x94) - or 0xFF / 0xFFFF without a count; the total, the title, the
// window and the option boxes. PSX 0x801D4B7C.
extern "C" void __cdecl EffectKind1A_Panel(void) {
    if (PanelRow()) {
        SH_CALL(FieldPanel_DrawKindIcon)(0x80, 0x5B, S()[0x3E]);
        SH_CALL(FieldPanel_DrawMessage)(8, 0x94, static_cast<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole));
    } else {
        SH_CALL(FieldPanel_DrawKindIcon)(0x80, 0x5B, 0xFF);
        SH_CALL(FieldPanel_DrawMessage)(8, 0x94, 0xFFFF);
    }
    SH_CALL(FieldPanel_DrawTotal)(0xB0, 0x9C);
    PanelTitle();
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
}

namespace {
// _PanelNext / _PanelBack: +9 moved by `delta`, the header and row, the icon
// at 0x80 -/+ 48 * +9 by +8 (`toward` picks which sign +8 == 0 takes), the
// tail; at +9 `end`, `done`.
void PanelSlide(int delta, bool minus_when_eight_clear) {
    S()[9] = static_cast<unsigned char>(S()[9] + delta);
    PanelRow();
    unsigned char* const a = S();
    const bool minus = (a[8] == 0) == minus_when_eight_clear;
    const U slide = a[9] * 48u;
    const U x = minus ? 0x80u - slide : slide + 0x80u;
    if (B(at::kKindCounts + static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole)))) != 0)
        SH_CALL(FieldPanel_DrawKindIcon)(static_cast<int>(x), 0x5B, a[0x3E]);
    else
        SH_CALL(FieldPanel_DrawKindIcon)(static_cast<int>(x), 0x5B, 0xFF);
    PanelTail();
}
}  // namespace

// original 0x466E10 (EffectKind1A_States[9]): +9 up; the panel's header and
// row, the icon at 0x80 - 48 * +9 (+8 clear) or 0x80 + 48 * +9, the total, the
// message 0xFFFF, the title, the window and option boxes; at +9 4 the word
// +0x3E = +0x3C and +1 up. PSX 0x801D4CA0.
extern "C" void __cdecl EffectKind1A_PanelNext(void) {
    PanelSlide(1, true);
    unsigned char* const a = S();
    if (a[9] != 4) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->height_whole, static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->height_frac));
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x466F70 (EffectKind1A_States[10]): +9 down; as _PanelNext with the
// icon's sign the other way; at +9 0 +1 = 8. PSX 0x801D4E8C.
extern "C" void __cdecl EffectKind1A_PanelBack(void) {
    PanelSlide(-1, false);
    unsigned char* const a = S();
    if (a[9] == 0) a[1] = 8;
}

// original 0x4670C0 (EffectKind1A_States[11]): the window and option boxes
// sliding by +9 as _Open's (in place once +0xB); +9 up; the panel sliding out
// by it as _PanelIn's; at +9 4: +1 = 0 without +0xB, 2 when +6 is 0, up when
// it is 2. PSX 0x801D5064.
extern "C" void __cdecl EffectKind1A_PanelOut(void) {
    unsigned char* a = S();
    if (a[0xB] == 0) {
        const U v = Slide(Key(a), a[9]);
        const U answer = WindowBox(0x14, 0x12 - (v << 4), 0x118, 0x13, 0);
        OptionBoxes(Slide(answer, S()[9]) * 45 + 0x58, 0x28, 0);
    } else {
        WindowBox(0x14, 0x12, 0x118, 0x13, 0);
        OptionBoxes(0x58, 0x28, OptionBits());
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    const unsigned char nine = S()[9];
    SH_CALL(FieldPanel_DrawHeader)(-80 * static_cast<int>(nine), 0x4E);
    PanelKind();
    SH_CALL(FieldPanel_DrawTotal)(0xB0, static_cast<int>(S()[9] * 30u + 0x9C));
    SH_CALL(FieldPanel_DrawMessage)(8, static_cast<int>(S()[9] * 30u + 0x94), 0xFFFF);
    a = S();
    if (a[9] != 4) return;
    if (a[0xB] == 0) {
        a[1] = 0;
        return;
    }
    if (a[6] == 0)
        a[1] = 2;
    else if (a[6] == 2)
        a[1] = static_cast<unsigned char>(a[1] + 1);
}

// original 0x467270 (EffectKind1A_States[12]): the window at 0x12 and the
// option boxes; +8 0; the height, +0x14, +0x20 0; +0xA 0; +9 4; +1 up. PSX
// 0x801D52F0.
extern "C" void __cdecl EffectKind1A_Reset(void) {
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
    S()[8] = 0;
    rv::Current<records::EffectObject>()->height = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, 0);
    S()[0xA] = 0;
    S()[9] = 4;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

void Effect1A_Inject() {
    if (bof3::WantsShadow("effect_1a")) effect_1a::SelfTest();
    BOF3_INJECT(EffectKind0E_WorldMap);
    BOF3_INJECT(EffectKind16_WorldMap);
    BOF3_INJECT(EffectKind5C_Run);
    BOF3_INJECT(EffectKind01_Run);
    BOF3_INJECT(EffectKind01_Start);
    BOF3_INJECT(EffectKind01_Draw);
    BOF3_INJECT(EffectKind07_Run);
    BOF3_INJECT(EffectKind07_Start);
    BOF3_INJECT(EffectKind07_FadeIn);
    BOF3_INJECT(EffectKind08_Run);
    BOF3_INJECT(EffectKind09_Run);
    BOF3_INJECT(EffectKind0B_Run);
    BOF3_INJECT(EffectKind10_Run);
    BOF3_INJECT(EffectKind02_Run);
    BOF3_INJECT(EffectHud_Draw);
    BOF3_INJECT(EffectHud_DrawGauge);
    BOF3_INJECT(EffectHud_Bar);
    BOF3_INJECT(EffectHud_Marker);
    BOF3_INJECT(EffectHud_Sprite8);
    BOF3_INJECT(EffectKind03_Run);
    BOF3_INJECT(EffectKind03_Start);
    BOF3_INJECT(EffectKind03_Fade);
    BOF3_INJECT(EffectHud_TwoBars);
    BOF3_INJECT(EffectKind03_ShowName);
    BOF3_INJECT(EffectHud_DrawCount);
    BOF3_INJECT(EffectKind05_Run);
    BOF3_INJECT(EffectKind05_Start);
    BOF3_INJECT(EffectKind05_Rise);
    BOF3_INJECT(EffectKind05_Land);
    BOF3_INJECT(EffectKind05_Bounce);
    BOF3_INJECT(EffectKind05_Follow);
    BOF3_INJECT(EffectKind0A_Run);
    BOF3_INJECT(EffectKind0A_Start);
    BOF3_INJECT(EffectKind0A_Follow);
    BOF3_INJECT(EffectKind0C_Run);
    BOF3_INJECT(EffectKind0C_Start);
    BOF3_INJECT(EffectKind0C_Aim);
    BOF3_INJECT(EffectHud_DrawArrow);
    BOF3_INJECT(EffectHud_DrawMark);
    BOF3_INJECT(EffectKind0D_Run);
    BOF3_INJECT(EffectKind0D_Start);
    BOF3_INJECT(EffectKind0D_SlideIn);
    BOF3_INJECT(EffectKind0D_Hold);
    BOF3_INJECT(EffectKind0D_SlideOut);
    BOF3_INJECT(EffectKind0F_Run);
    BOF3_INJECT(EffectKind0F_Start);
    BOF3_INJECT(EffectKind0F_Open);
    BOF3_INJECT(EffectKind0F_Choose);
    BOF3_INJECT(EffectKind0F_Title);
    BOF3_INJECT(EffectKind0F_TitleClose);
    BOF3_INJECT(EffectKind0F_Frame);
    BOF3_INJECT(EffectKind0F_Close);
    BOF3_INJECT(EffectKind0F_LineStart);
    BOF3_INJECT(EffectKind0F_LineType);
    BOF3_INJECT(EffectKind0F_LineNext);
    BOF3_INJECT(EffectKind0F_LineScroll);
    BOF3_INJECT(EffectKind0F_LineFade);
    BOF3_INJECT(EffectKind1A_Run);
    BOF3_INJECT(EffectKind1A_Start);
    BOF3_INJECT(EffectKind1A_Open);
    BOF3_INJECT(EffectKind1A_ItemsA);
    BOF3_INJECT(EffectKind1A_ItemsB);
    BOF3_INJECT(EffectKind1A_Close);
    BOF3_INJECT(EffectKind1A_FindKind);
    BOF3_INJECT(EffectKind1A_PanelIn);
    BOF3_INJECT(EffectKind1A_Panel);
    BOF3_INJECT(EffectKind1A_PanelNext);
    BOF3_INJECT(EffectKind1A_PanelBack);
    BOF3_INJECT(EffectKind1A_PanelOut);
    BOF3_INJECT(EffectKind1A_Reset);
}

// DIVERGENCE DIV-0144 (D279): the fishing banner declines an empty string.
namespace {

// BOF3X_SHADOW=fishing_empty: an empty line and a NUL label planted in the
// image (text record 1 and the six 0x66A2FC entries, restored after), a
// record of ours as Sprite_Current; nothing may be drawn - Gfx_PacketNext
// unmoved - and the calls that reach EffectKind0F_DrawGlyph are counted, so
// the scroller's switch is seen to stop its call and the glyph's switch to
// decline the label's. Run before FishingText_Arm: the label is then the
// original's read (the line byte's 0x66A2FC entry), not an icon or a disc's.
void FishingEmptySelfTest() {
    constexpr unsigned kText = 1;   // not the cast's text 0, whose label reads word 7
    unsigned char* const record = At(at::kKind0FTexts + 8 * kText);
    unsigned char saved_record[8], saved_labels[24];
    std::memcpy(saved_record, record, sizeof saved_record);
    std::memcpy(saved_labels, At(at::kLabelTexts), sizeof saved_labels);
    unsigned char* const saved_current = Sprite_Current;
    unsigned char* const saved_packet = Gfx_PacketNext;
    const std::uint8_t saved_line = g_empty_line_none, saved_glyph = g_low_glyph_none;
    static const unsigned char kEmpty[4] = {0, 0, 0, 0};
    static const unsigned char kSpace[2] = {0x20, 0};
    SetL(record, Key(kEmpty));
    record[4] = 0;   // line byte 0: the label is entry 0
    record[5] = 1;   // its pause
    for (unsigned e = 0; e < 6; ++e) SetL(At(at::kLabelTexts + 4 * e), Key(kEmpty));
    unsigned char object[0x80];
    unsigned bad = 0, cases = 0;
    auto check = [&](bool ok, const char* what, unsigned got, unsigned want) {
        ++cases;
        if (ok) return;
        ++bad;
        bof3::Log("shadow      fishing_empty MISMATCH %s: %u, expected %u", what, got, want);
    };
    auto start = [&](unsigned char state, unsigned char wait, unsigned char line_wait) {
        std::memset(object, 0, sizeof object);
        object[0] = 1;
        object[1] = state;
        object[5] = 0xF;
        object[9] = wait;
        object[0xA] = line_wait;
        object[0x4B] = kText;
        SetL(object + 0x50, Key(kEmpty));
        SetW(object + 0x2E, 0x125);
        Sprite_Current = object;
    };
    auto run = [&](void (*state)(), const char* what, unsigned want_calls) {
        const unsigned calls = EffectKind0F_GlyphCalls();
        state();
        check(Gfx_PacketNext == saved_packet, what, Gfx_PacketNext == saved_packet ? 0 : 1, 0);
        check(EffectKind0F_GlyphCalls() - calls == want_calls, what, EffectKind0F_GlyphCalls() - calls, want_calls);
    };
    g_empty_line_none = 1;
    g_low_glyph_none = 1;
    // LineType 0x466310 typing an empty line: no cursor glyph is asked for.
    start(9, 6, 0);
    run(EffectKind0F_LineType, "LineType over an empty line (both on): packet / calls", 0);
    check(object[1] == 9 && object[0x49] == 0, "LineType over an empty line: the state", object[1], 9);
    // Its last frame: the whole line typed at once, the pause set, the next state.
    start(9, 1, 0);
    run(EffectKind0F_LineType, "LineType's last frame over an empty line: packet / calls", 0);
    check(object[1] == 10 && object[0x49] == 0 && object[9] == 1, "LineType's last frame: the state, +0x49, +9",
          object[1], 10);
    // The control: the scroller's switch off, the call reaches the draw, which
    // declines the NUL.
    g_empty_line_none = 0;
    start(9, 6, 0);
    run(EffectKind0F_LineType, "LineType over an empty line (the scroller's switch off): packet / calls", 1);
    g_empty_line_none = 1;
    // LineNext 0x466460 while the label waits: the label's NUL reaches the
    // draw (0x66A2FC's entry 0), which draws nothing.
    start(10, 6, 6);
    run(EffectKind0F_LineNext, "LineNext's label over a NUL label: packet / calls", 1);
    check(object[0xA] == 5, "LineNext's label wait", object[0xA], 5);
    // The draw itself (0x469AD0, which Inject aimed at ours): a NUL and a space.
    const unsigned before = EffectKind0F_GlyphCalls();
    MessageLine(0, Key(kEmpty), 0xC, 0x100);
    MessageLine(0, Key(kSpace), 0xC, 0x100);
    check(Gfx_PacketNext == saved_packet, "DrawGlyph of a NUL and a space: packet", 1, 0);
    check(EffectKind0F_GlyphCalls() - before == 2, "DrawGlyph of a NUL and a space: calls",
          EffectKind0F_GlyphCalls() - before, 2);
    std::memcpy(record, saved_record, sizeof saved_record);
    std::memcpy(At(at::kLabelTexts), saved_labels, sizeof saved_labels);
    Sprite_Current = saved_current;
    g_empty_line_none = saved_line;
    g_low_glyph_none = saved_glyph;
    bof3::Log("shadow      fishing_empty DIV-0144 self-test: an empty line typed (two frames) and its control, a NUL "
              "label, a NUL and a space drawn: %u checks, nothing drawn, %u MISMATCHES",
              cases, bad);
    if (bad) bof3::Fatal("fishing_empty DIV-0144 self-test: %u mismatches (bof3x.log)", bad);
}

}  // namespace

bool FishingEmpty_LowGlyphNone() { return g_low_glyph_none != 0; }

void FishingEmpty_Arm() {
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("FishingEmptyLine", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_empty_line_none)),
                     &was, &is, 1);
    bof3::PatchBytes("FishingLowGlyph", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_low_glyph_none)),
                     &was, &is, 1);
    bof3::Log("DIV-0144    the fishing banner draws no glyph for an empty line (%s) nor for a one-byte code below "
              "0x26 (%s)", g_empty_line_none ? "on" : "OFF", g_low_glyph_none ? "on" : "OFF");
    if (!bof3::WantsShadow("fishing_empty")) return;
    if (!bof3::IsEnabled(bof3::addr::EffectKind0F_DrawGlyph)) {
        bof3::Log("shadow      fishing_empty DIV-0144 self-test: not run, EffectKind0F_DrawGlyph is Capcom's "
                  "(BOF3X_ORIGINAL), which draws the planted NUL");
        return;
    }
    FishingEmptySelfTest();
}
