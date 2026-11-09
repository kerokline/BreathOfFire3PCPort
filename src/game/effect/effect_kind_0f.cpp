// Moved here by tools/move_funcs.py from effect_1a.cpp, effect_1b.cpp, rest_4f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_0f.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/config_text.h"
#include "game/effect/effect_kind_0f_callees.h"
#include "game/effect_1a.h"
#include "game/effect_1a_callees.h"
#include "game/effect_1b.h"
#include "game/effect_1b_callees.h"
#include "game/fishing_text.h"
#include "game/menu_frame.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_4f.h"
#include "game/rest_4f_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_kind_0f::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

unsigned char* S() { return Sprite_Current; }

U Key(const void* p) { return Low32(p); }

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
                    who, state, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * state)))();
}

// Sprite_Objects record `index` (the byte 0x939A1C): thirty of them.
unsigned char* Object(const char* who, U index) {
    if (index >= effect_1a::at::kObjectCount)
        bof3::Fatal("%s: object index %u (0x939A1C), past the thirty Sprite_Objects records - the original reads what "
                    "follows (docs/effect_1a.md section 6)",
                    who, (unsigned)index);
    return At(::bof3::Img(Sprite_Objects_addr) + index * at::kObjectStride);
}

// Effect_FindFree's record: a Fatal on an answer past the twenty (the callee
// answers 0xFF or 0..19; the original would write past the pool). `index` is
// the answer as the caller widened it (a movsx in kind 0xF's spawns).
unsigned char* Spawned(const char* who, std::int32_t index) {
    if (index < 0 || index >= static_cast<std::int32_t>(effect_1a::at::kEffectCount))
        bof3::Fatal("%s: Effect_FindFree answered %d, past the 20 records - the original writes past the pool "
                    "(docs/effect_1a.md section 6)",
                    who, (int)index);
    return At(::bof3::Img(Effect_Objects_addr) + static_cast<U>(index) * effect_1a::at::kEffectStride);
}

// Kind 0xF's text record `index` (+0x4B): thirteen of 8 bytes.
U Text(const char* who, unsigned index) {
    if (index >= at::kKind0FTextCount)
        bof3::Fatal("%s: text index +0x4B is %u, past the thirteen records of 0x653B98 (docs/effect_1a.md section 6)",
                    who, index);
    return ::bof3::Img(at::kKind0FTexts.v) + index * 8u;
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
    return {l.pen, L(::bof3::Img(at::kLabelTexts.v) + 4 * l.entry)};
}

// Kind 0xF's row table 0x653C04: nine rows of four bytes, by +6 and a column.
U RowByte(const char* who, unsigned row, unsigned column) {
    const U offset = row * 4u + column;
    if (offset >= at::kKind0FRowBytes)
        bof3::Fatal("%s: row %u column %u, past the table 0x653C04 (docs/effect_1a.md section 6)", who, row, column);
    return ::bof3::Img(at::kKind0FRows.v) + offset;
}

// The raw callees (effect_1a_callees.h): E1F's two draw helpers answer eax
// (0x52CFE0 the primitive); E1B's are typed to answer eax too, because the
// original loads a byte or a word into the low half of the register the
// answer left and pushes it whole (docs/effect_1a.md section 5).
U DrawMode(U id, U slot) { return SH_AT(U (__cdecl*)(U, U), at::kDrawModeRecord.v)(id, slot); }

U DrawSprite(U id, U slot, U x, U y) { return SH_AT(U (__cdecl*)(U, U, U, U), at::kDrawSpriteRecord.v)(id, slot, x, y); }

void DepthPair() { SH_AT(void (__cdecl*)(), at::kDepthPair.v)(); }

U WindowBox(U x, U y, U w, U h, U colour) {
    return SH_AT(U (__cdecl*)(U, U, U, U, U), at::kWindowBox.v)(x, y, w, h, colour);
}

U OptionBoxes(U x, U y, U bits) { return SH_AT(U (__cdecl*)(U, U, U), at::kOptionBoxes.v)(x, y, bits); }

U MessageLine(U pen, U text, U width, U x) {
    return SH_AT(U (__cdecl*)(U, U, U, U), at::kMessageLine.v)(pen, text, width, x);
}

U MemberRows(U x, U y) { return SH_AT(U (__cdecl*)(U, U), at::kMemberRows.v)(x, y); }

U ItemListA(U x, U y) { return SH_AT(U (__cdecl*)(U, U), at::kItemListA.v)(x, y); }

U ItemListB(U x, U y) { return SH_AT(U (__cdecl*)(U, U), at::kItemListB.v)(x, y); }

void PanelTitle() { SH_AT(void (__cdecl*)(), at::kPanelTitle.v)(); }

U StringCount(U text) { return SH_AT(U (__cdecl*)(U), at::kStringCount.v)(text); }

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

void CopyDword(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, 4); }

// kinds 0xF's windows: ObjTrio 0's +3 at 3 or more names the option bit.
U OptionBits() {
    if (ObjTrio[0].state3 < 3) return 0;
    return 1u << (S()[6] & 31);
}

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
    const unsigned char count = B(::bof3::Img(at::kKindCounts.v) + static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole))));
    const U icon_x = a[9] * 48u + 0x80;
    if (count != 0) {
        SH_CALL(FieldPanel_DrawKindIcon)(static_cast<int>(icon_x), 0x5B, a[0x3E]);
        a = S();
        SH_CALL(FieldPanel_DrawKindRow)(a[0x3E], B(::bof3::Img(at::kKindCounts.v) + static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole)))), a[9]);
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
    const unsigned char count = B(::bof3::Img(at::kKindCounts.v) + static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole))));
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
    if (B(::bof3::Img(at::kKindCounts.v) + static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole)))) != 0)
        SH_CALL(FieldPanel_DrawKindIcon)(static_cast<int>(x), 0x5B, a[0x3E]);
    else
        SH_CALL(FieldPanel_DrawKindIcon)(static_cast<int>(x), 0x5B, 0xFF);
    PanelTail();
}

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* Sc() { return Sprite_Current; }

U ScKey() { return Low32(Sprite_Current.Native()); }

unsigned char& BEffect1b(U a) { return *At(a); }

unsigned char Style() { return static_cast<unsigned char>(Config_WindowColour); }

const unsigned char* TextEffect1b(U a) { return At(a); }

char* Print() { return reinterpret_cast<char*>(At(::bof3::Img(at::kPrint.v))); }

// A float stored as the originals' fild / fstp store it: every value here is a
// whole number well inside a float's 24 bits, so exact.
void F(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

Handler StepAt(U table, unsigned index, unsigned count, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: index %u past the %u entries of 0x%X (the original jumps through what follows)", who, index,
                    count, (unsigned)::bof3::VaOf(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))));
}

// A byte of one of the image's scripts or tables: past its end the original
// reads what follows, and ours aborts there. The steps read "the record before
// +0xA", so with +0xA 0 the original reads up to four bytes BEFORE the script -
// the last pointer of the step table in front of it, image constants - and ours
// reads the same bytes (the steps' own start never gets there: +9 0 advances
// +0xA before the read; a record left mid-step does, docs/effect_1b.md section 7).
unsigned char ByteOf(U table, unsigned bytes, int index, const char* who) {
    if (index < -4 || (index >= 0 && static_cast<unsigned>(index) >= bytes))
        bof3::Fatal("%s: byte %d of the %u-byte table 0x%X (the original reads past it)", who, index, bytes,
                    (unsigned)::bof3::VaOf(table));
    return BEffect1b(static_cast<U>(static_cast<std::int32_t>(table) + index));
}

std::uint16_t WordOf(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: word %u of the %u-word table 0x%X (the original reads past it)", who, index, count,
                    (unsigned)::bof3::VaOf(table));
    return Word(At(table + 2 * index));
}

// The record Effect_FindFree answered, written without the original checking
// the answer (0xFF, none free, would write 0x7E91E0.., past the pool): ours
// aborts on any index outside the 20.
unsigned char* SpawnedEffect1b(unsigned char k, const char* who) {
    if (k >= effect_1b::at::kEffectCount)
        bof3::Fatal("%s: Effect_FindFree answered %u, and the original writes that record unchecked (past the pool)",
                    who, (unsigned)k);
    return rv::Bytes(&Effect_Objects[k]);
}

// A kind-0x1A record in state 0x10, as five of the states spawn it.
unsigned char* SpawnMark(unsigned char k, const char* who) {
    unsigned char* const r = SpawnedEffect1b(k, who);
    r[0] = 1;
    r[5] = 0x1A;
    r[1] = 0x10;
    return r;
}

// An accessory's category byte (+0x12 of NameTable_Accessories' 24-byte records).
unsigned char Category(unsigned char id, const char* who) {
    if (id >= at::kAccessoryCount)
        bof3::Fatal("%s: accessory id %u past the %u records (the original reads past NameTable_Accessories)", who,
                    (unsigned)id, at::kAccessoryCount);
    return BEffect1b(::bof3::Img(at::kAccessoryCategory.v) + at::kAccessoryStride * id);
}

const unsigned char* AccessoryName(unsigned char id) {
    return At(::bof3::Img(bof3::addr::NameTable_Accessories) + at::kAccessoryStride * id);
}

// The characters a name is drawn to: the original's 8, or 12 under a Latin
// overlay (DIVERGENCE DIV-0069: the US name field; 8 cut "Wooden Rod" to "Wooden R").
int NameCount() { return static_cast<int>(FishingText_NameCount()); }

// A script-pool message: MessagePools + its u16 offset.
const unsigned char* Pool(unsigned id) { return At(::bof3::Img(at::kPools.v) + Word(At(::bof3::Img(at::kPools.v) + 2 * id))); }

// --- the callees by address (E1F's, E1A's, E1G's) ------------------------------------
void DrawModeEffect1b(unsigned id, unsigned slot) { SH_AT(void (__cdecl*)(unsigned, unsigned), at::kDrawMode.v)(id, slot); }

void DrawSpriteEffect1b(unsigned id, unsigned slot, int x, int y) {
    SH_AT(unsigned char* (__cdecl*)(unsigned, unsigned, int, int), at::kDrawSprite.v)(id, slot, x, y);
}

void Call3(U address, U a, U b, U c) { SH_WORD(void (__cdecl*)(U, U, U), address)(a, b, c); }

void Call4(U address, U a, U b, U c, U d) { SH_WORD(void (__cdecl*)(U, U, U, U), address)(a, b, c, d); }

void Call5(U address, U a, U b, U c, U d, U e) { SH_WORD(void (__cdecl*)(U, U, U, U, U), address)(a, b, c, d, e); }

void Call6(U address, U a, U b, U c, U d, U e, U f) {
    SH_WORD(void (__cdecl*)(U, U, U, U, U, U), address)(a, b, c, d, e, f);
}

void DrawAt(int x, int y, int colour, int count, const unsigned char* text) {
    SH_CALL(Text_DrawAt)(x, y, colour, count, text);
}

// The toggles' bits the list states hand EffectKind0F_DrawToggles: 1 << +6
// (the shift count masked to 5 bits, as shl masks it) once the leader's +3 is
// 3 or more, else none.
U ToggleBits() { return ObjTrio[0].state3 >= 3 ? 1u << (Sc()[6] & 31) : 0u; }

// The pose every sub-kind's first step gives the record (Sprite_* read it as a
// sprite): +0x24 = 0x80, +0x29, +0x2A, the screen place +0x2E / +0x30, and the
// counters and tint bytes cleared. The stores have no call between them, so
// their order is not observable.
void Place(unsigned char b29, unsigned char b2A, unsigned x, unsigned y) {
    unsigned char* const s = Sc();
    s[0x24] = 0x80;
    s[0x29] = b29;
    s[0x2A] = b2A;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, x);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, y);
    s[0x48] = 0;
    s[0x5D] = s[0x5E] = s[0x5F] = 0;
}

// The move scripts of sub-kinds 4 and 5 (0x653D5C, 0x653DAC): records of
// (animation | 0x80 no script tick, frames, dx, dy); an animation of 0xFF goes
// to record `frames`. The step's record: Sprite_EnsureAnimation(animation &
// 0x7F), +9 its frames, +0xA + 1 - returned so sub-kind 5 can test it - or +9 -
// 1 and a tick unless the current record (the one before +0xA) says not; then
// the cursor (dwords +0xC, +0x10) moved by its dx, dy and +6 less dx.
signed char MoveByte(U script, int index, const char* who) {
    return static_cast<signed char>(ByteOf(script, at::kMoveScriptBytes, index, who));
}

void MoveCursor(U script, const char* who) {
    unsigned char* s = Sc();
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c)) +
                                               static_cast<U>(static_cast<std::int32_t>(MoveByte(script, 4 * s[0xA] - 2, who)))));
    s = Sc();
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_10, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10)) +
                                                static_cast<U>(static_cast<std::int32_t>(MoveByte(script, 4 * s[0xA] - 1, who)))));
    s = Sc();
    s[6] = static_cast<unsigned char>(s[6] - static_cast<unsigned char>(MoveByte(script, 4 * s[0xA] - 2, who)));
}

}  // namespace

// D286 (DIV-0069's tag 3, docs/install-gap-labels.md): heading `i` drawn
// where the original draws `count` characters of `label` at `x`, y. Once an
// overlay has given the heading (FishingText_Heading), the disc's word whole,
// centred on the original's centre, x + 6 * count (its 12-unit glyphs), by
// its real width - the US module's x + 0x3A / 0x37 / 0x33 for its 8-unit
// ROD / LURE / EQUIP put the same centre within a unit. For a 12-unit word
// of `count` glyphs it is the original's x.
static void DrawHeading(unsigned i, int x, int y, int count, U label) {
    if (const unsigned char* const word = FishingText_Heading(i)) {
        const int width = static_cast<int>(TextAdvance_Width(word));
        DrawAt(x + 6 * count - width / 2, y, 0, 0xFF, word);
        return;
    }
    DrawAt(x, y, 0, count, TextEffect1b(label));
}

namespace {

// The window style's colour: the first word of its 0x40 bytes at 0x80B7A8 (the
// style byte signed), as 5:5:5 (r, g, b) bytes shifted up by 3.
struct Colour { unsigned char r, g, b; };

Colour StyleColour() {
    const std::uint16_t c = Word(At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x228 + static_cast<U>(64 * static_cast<int>(static_cast<signed char>(Style())))));
    return {static_cast<unsigned char>((c & 0x1F) << 3), static_cast<unsigned char>(((c >> 5) & 0x1F) << 3),
            static_cast<unsigned char>(((c >> 10) & 0x1F) << 3)};
}

void Shade(unsigned char* p, const Colour& c) {
    p[4] = c.r;
    p[5] = c.g;
    p[6] = c.b;
}

using HandlerRest4f = void (__cdecl*)();

U LRest4f(U a) { return static_cast<U>(Long(move_script::At(a))); }

std::int32_t SLRest4f(const unsigned char* p) { return Long(p); }

U WRest4f(U a) { return move_script::Word(move_script::At(a)); }

void SetWRest4f(U a, U v) { move_script::SetWord(move_script::At(a), v); }

// jmp / call [table + byte * 4]: the table's `entries` handlers read in place (the
// fuzz swaps the cells for recorders); a Fatal past them, where the original
// goes through the dword after - the next table's cell or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u handlers of 0x%X - the original jumps through the dword after "
                    "(docs/rest_4f.md section 7)",
                    who, byte, index, entries, (unsigned)::bof3::VaOf(table));
    reinterpret_cast<HandlerRest4f>(static_cast<std::uintptr_t>(LRest4f(table + 4 * index)))();
}

// EffectHud_Draw(0xC, y): y the word +0x10 under `upper` (the original pushes eax
// after `mov ax, [+0x10]` - Sprite_Current's high half, or the sub-state
// handler's leftover; the panel reads the word).
void Panel(U upper) { SH_CALL(EffectHud_Draw)(0xC, static_cast<int>(upper | Word(S() + 0x10))); }

U HiRest4f(const void* p) { return Key(p) & 0xFFFF0000u; }

// Palette 0x7B: entries 1..30 the leader's palette (ObjTrio 0's +0x27) with bit
// 15 set, entries 0 and 31 zero; Gfx_ClutStripDirty is set by the callers.
void TintPalette() {
    const U from = ::bof3::Img(Gfx_ClutStrip_addr) + 2 + static_cast<std::uint32_t>(ObjTrio[0].clut) * at::kClutBytes;
    for (U i = 0; i < 30; ++i) SetWRest4f(::bof3::Img(at::kClut7B.v) + 2 + 2 * i, WRest4f(from + 2 * i) | 0x8000);
    Gfx_ClutStrip[0xf60] = 0;
    Gfx_ClutStrip[0xf7f] = 0;
}

// The record shows palette 0x7B: +0xB the palette it had, +0 bit 5 set, +0x27 0x7B.
void UseTint(unsigned char* s) {
    s[0xB] = s[0x27];
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    s[0x27] = 0x7B;
}

// Back to its own palette: the colour 0, +0 bit 5 clear, +0x27 from +0xB.
void DropTint(unsigned char* s) {
    s[0x5D] = s[0x5E] = s[0x5F] = 0;
    s[0] = static_cast<unsigned char>(s[0] & 0xDF);
    s[0x27] = s[0xB];
}

bool Above38() { return static_cast<std::int16_t>(WRest4f(::bof3::Img(at::kKind02Word.v))) > 0x38; }

}  // namespace

// original 0x464660 (Effect_KindHandlers 2): EffectKind02_States by +1.
extern "C" void __cdecl EffectKind02_Run(void) { Run("EffectKind02_Run", ::bof3::Img(EffectKind02_States_addr), 5, S()[1]); }

// original 0x464680 (EffectKind02_States[0]): the leader's +0x25..+0x28 copied;
// the slot +0x29 2; +0x24 0x80; the words +0x2E 0x39, +0x30 0xC8; +0x48, +0x5C,
// the colour 0; the panel's y +0x10 0xF0; +1 up.
extern "C" void __cdecl EffectKind02_Start(void) {
    unsigned char* const s = S();
    s[0x25] = ObjTrio[0].tex_slot;
    s[0x26] = ObjTrio[0].tex_row;
    s[0x27] = ObjTrio[0].clut;
    s[0x28] = ObjTrio[0].clut_mode;
    s[0x29] = 2;
    s[0x24] = 0x80;
    rv::As<records::EffectObject>(s)->screen_x = 0x39;
    rv::As<records::EffectObject>(s)->screen_y = 0xC8;
    s[0x48] = s[0x5C] = s[0x5D] = s[0x5E] = s[0x5F] = 0;
    rv::As<records::EffectWork>(s)->work_10 = 0xF0;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x464730 (EffectKind02_States[2]): the panel's y down 8, at below 0xAE
// held there and +1 up; EffectHud_Draw(0xC, y).
extern "C" void __cdecl EffectKind02_SlideIn(void) {
    unsigned char* const s = S();
    rv::As<records::EffectWork>(s)->work_10 = rv::As<records::EffectWork>(s)->work_10 + 0xFFFFFFF8u;
    if (SLRest4f(s + 0x10) < 0xAE) {
        rv::As<records::EffectWork>(s)->work_10 = 0xAE;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
    Panel(HiRest4f(s));
}

// original 0x464780 (EffectKind02_States[3]): call [EffectKind02_Modes + +2 * 4]
// (unbounded: ours aborts past 4), then the panel at y +0x10.
extern "C" void __cdecl EffectKind02_Body(void) {
    Dispatch("EffectKind02_Body", ::bof3::Img(EffectKind02_Modes_addr), at::kKind02ModeCount, S()[2], "the sub-state +2");
    Panel(0);
}

// original 0x4647B0 (EffectKind02_States[4]): the panel's y up 8; past 0xF0 held
// there, +1 = 1, +2 = +3 = 0; the panel drawn.
extern "C" void __cdecl EffectKind02_SlideOut(void) {
    unsigned char* const s = S();
    rv::As<records::EffectWork>(s)->work_10 = rv::As<records::EffectWork>(s)->work_10 + 8;
    if (SLRest4f(s + 0x10) > 0xF0) {
        rv::As<records::EffectWork>(s)->work_10 = 0xF0;
        s[1] = 1;
        s[2] = s[3] = 0;
    }
    Panel(HiRest4f(s));
}

// original 0x464810 (EffectKind02_Modes[1]): jmp [EffectKind02_Mode1Steps + +3 *
// 4] (unbounded: ours aborts past 2).
extern "C" void __cdecl EffectKind02_RunMode1(void) {
    Dispatch("EffectKind02_RunMode1", ::bof3::Img(EffectKind02_Mode1Steps_addr), at::kKind02Mode1Count, S()[3], "the step +3");
}

// original 0x464830 (EffectKind02_Mode1Steps[0]): palette 0x7B tinted from the
// leader's; Gfx_ClutStripDirty 1; +0x5C 1. With the word 0x905E62 above 0x38 (s16)
// +2 = 3 and +3 = 2; else the record on palette 0x7B, the colour 0x80, +3 up.
extern "C" void __cdecl EffectKind02_Mode1Tint(void) {
    TintPalette();
    unsigned char* const s = S();
    Gfx_ClutStripDirty = 1;
    s[0x5C] = 1;
    if (Above38()) {
        s[2] = 3;
        s[3] = 2;
        return;
    }
    UseTint(s);
    s[0x5D] = s[0x5E] = s[0x5F] = 0x80;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x4648F0 (EffectKind02_Mode1Steps[1]): the colour up 0x10; once +0x5D
// is at least 0xC0 as an s8 (0xC0..0x7F), the record back on its palette, +3 0
// and +2 up; then (a tail jmp) EffectKind02_CopyLeader.
extern "C" void __cdecl EffectKind02_Mode1Brighten(void) {
    unsigned char* const s = S();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0x10);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0x10);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0x10);
    if (static_cast<signed char>(s[0x5D]) >= static_cast<signed char>(0xC0)) {
        DropTint(s);
        s[3] = 0;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind02_CopyLeader)();
}

// original 0x464970 (EffectKind02_Modes[2], and the tail of _Mode1Brighten and
// _Mode3Darken; inside the catalog's extent of 0x4648F0, in no list): with the
// leader's +2 at 1, +2 = 0 and nothing more; +2 = 3 when the word 0x905E62 is
// above 0x38; the leader's +0x2A, +0x49..+0x4B, +0x50, +0x54, the words +0x58,
// +0x5A copied; Sprite_QueueOverlay (a tail jmp).
extern "C" void __cdecl EffectKind02_CopyLeader(void) {
    unsigned char* const s = S();
    if (ObjTrio[0].step == 1) {
        s[2] = 0;
        return;
    }
    if (Above38()) s[2] = 3;
    s[0x2A] = ObjTrio[0].flip;
    s[0x49] = ObjTrio[0].script_steps;
    s[0x4A] = ObjTrio[0].script_hold;
    s[0x4B] = ObjTrio[0].animation;
    rv::As<records::EffectObject>(s)->script = ObjTrio[0].script;
    rv::As<records::EffectObject>(s)->frames = ObjTrio[0].frames;
    rv::As<records::EffectObject>(s)->script_pos = ObjTrio[0].script_pos;
    rv::As<records::EffectObject>(s)->frame_word = ObjTrio[0].frame_word;
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x464A20 (EffectKind02_Modes[3]): jmp [EffectKind02_Mode3Steps + +3 *
// 4] (unbounded: ours aborts past 3).
extern "C" void __cdecl EffectKind02_RunMode3(void) {
    Dispatch("EffectKind02_RunMode3", ::bof3::Img(EffectKind02_Mode3Steps_addr), at::kKind02Mode3Count, S()[3], "the step +3");
}

// original 0x464A40 (EffectKind02_Mode3Steps[0]): palette 0x7B tinted; the record
// on it, +0x5C 1, the colour 0xC0; Gfx_ClutStripDirty 1; +3 up;
// Sprite_QueueOverlay (a tail jmp).
extern "C" void __cdecl EffectKind02_Mode3Tint(void) {
    TintPalette();
    unsigned char* const s = S();
    UseTint(s);
    s[0x5C] = 1;
    s[0x5D] = s[0x5E] = s[0x5F] = 0xC0;
    Gfx_ClutStripDirty = 1;
    s[3] = static_cast<unsigned char>(s[3] + 1);
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x464AF0 (EffectKind02_Mode3Steps[1]): the colour down 0x10; at +0x5D
// 0x80 the record back on its palette and +3 up; else (a tail jmp)
// EffectKind02_CopyLeader.
extern "C" void __cdecl EffectKind02_Mode3Darken(void) {
    unsigned char* const s = S();
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + 0xF0);
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + 0xF0);
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + 0xF0);
    if (s[0x5D] != 0x80) {
        SH_CALL(EffectKind02_CopyLeader)();
        return;
    }
    DropTint(s);
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x464B60 (EffectKind02_Mode3Steps[2]): with the leader's +2 at 1, +2 =
// +3 = 0; else while the word 0x905E62 is below 0x38 (s16), +2 = 1 and +3 = 0.
extern "C" void __cdecl EffectKind02_Mode3Wait(void) {
    unsigned char* const s = S();
    if (ObjTrio[0].step == 1) {
        s[2] = 0;
        s[3] = 0;
        return;
    }
    if (static_cast<std::int16_t>(WRest4f(::bof3::Img(at::kKind02Word.v))) >= 0x38) return;
    s[2] = 1;
    s[3] = 0;
}

// original 0x464BA0 (called by kind 2's states EffectKind02_SlideIn, _Body, _SlideOut; PSX 0x801D159C):
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
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(::bof3::Img(at::kTextBuffer.v))), reinterpret_cast<const char*>(At(::bof3::Img(Boss26Fx_CountFormat_addr))),
                         static_cast<int>(count));
    const int colour = (count >= 0x32 && ObjTrio[0].step == 4 && (Frame_Counter & 4) != 0) ? 2 : 0;
    SH_CALL(Text_DrawFont12)(x + 0xEE, y + 0x1A, colour, At(::bof3::Img(at::kTextBuffer.v)));
    SH_CALL(EffectHud_DrawGauge)(x, y);
}

// original 0x464C70 (EffectHud_Draw's; PSX 0x801D195C): EffectHud_Bar(x + 0x58, y + 8, 4);
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
    const std::int16_t height = static_cast<std::int16_t>(static_cast<std::uint32_t>(static_cast<std::uint16_t>(Effect_Objects[0].height_whole)));
    if (height > 0)
        SH_CALL(EffectHud_Sprite8)(x + 0x69, 0xB4, 2);
    else
        SH_CALL(EffectHud_Sprite8)(x + 0x69, 0xB4 - height / 32, 2);
}

// original 0x464DA0 (PSX 0x801D1744): a POLY_G4 at the cursor, (s16 x, s16 y) to (x + 16.0, y
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

// original 0x464E40 (PSX 0x801D1818): the draw mode (0, slot); a SPRT_16 at the cursor at
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
    p[0x14] = B(::bof3::Img(at::kMarkerShades.v) + ((Frame_Counter >> 3) & 3));
    SH_CALL(Gfx_CommitPrim)(slot, 0x18);
}

// original 0x464EC0 (PSX 0x801D18CC): a SPRT_8 at the cursor at (s16 x, s16 y), shade 0x80,
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

// original 0x464F20 (Effect_KindHandlers 3): EffectKind03_States by +1.
extern "C" void __cdecl EffectKind03_Run(void) { Run("EffectKind03_Run", ::bof3::Img(EffectKind03_States_addr), 4, S()[1]); }

// original 0x464F40 (EffectKind03_States[1]; PSX 0x801D1B54): +0x18 = the first byte of the
// parameters 0x939A20 points at (the fade's step), +0xC, +0x10, +0x1C = 0, +1
// up. PSX 0x801D1B54 (table-anchored).
extern "C" void __cdecl EffectKind03_Start(void) {
    const unsigned char* const script = PtrAt(::bof3::Img(at::kPanelScript.v));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_18, script[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c, 0);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x464F80 (EffectKind03_States[2]; PSX 0x801D1CF0): the level +0xC stepped by the
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
        const unsigned mode = PtrAt(::bof3::Img(at::kPanelScript.v))[1];
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
    if (ObjTrio[0].step != 3) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_2e, 0xB8);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0xD1);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x465120 (EffectKind03_Fade's and E1B's 0x4678C0's; PSX 0x801D1B88): two POLY_G4s
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

// original 0x465230 (EffectKind03_States[3]): an accessory's name (by the
// byte 0x904130 while Frame_Counter's bit 6 is set, else 0x90412E) drawn at
// (0x8A, 0xC8), eight characters; then the count record 0's +7 less the byte
// +0xF of the block 0x939A24 points at - nothing when negative or 0, at most 4
// - drawn by EffectHud_DrawCount at (record 0's word +0x2E + 0xA, its dword
// +0x30 - 0x18), slot 2. PSX 0x801D1F38 (table-anchored).
extern "C" void __cdecl EffectKind03_ShowName(void) {
    const U name = (Frame_Counter & 0x40) != 0 ? ::bof3::Img(at::kAccessoryNames) + (L(::bof3::Img(at::kAccessoryA.v)) & 0xFF) * 0x18u
                                               : ::bof3::Img(at::kAccessoryNames) + CharStats_ExtraItem * 0x18u;
    DrawText(0x8A, 0xC8, 0, FishingText_NameCount(), name);   // DIV-0069: 12 under a Latin overlay, else 8
    const unsigned char* const set = PtrAt(::bof3::Img(at::kPanelSet.v));
    const auto left = static_cast<signed char>(rv::Bytes(Effect_Objects)[0x7] - set[0xF]);
    if (left < 0) return;
    unsigned n;
    if (left > 4) {
        n = 4;
    } else {
        if (left == 0) return;
        n = static_cast<unsigned>(left);
    }
    const U x = Hi(Key(set)) | ((W(::bof3::Img(at::kRecord0.v) + 0x2E) + 0xA) & 0xFFFF);
    SH_CALL(EffectHud_DrawCount)(static_cast<int>(x), static_cast<int>(L(::bof3::Img(at::kRecord0.v) + 0x30) - 0x18), 2, n);
}

// original 0x4652D0 (EffectKind03_ShowName's and E1B's 0x467E10's; PSX 0x801D2030): the draw
// mode (0, slot); panel sprites 0x17 at (x, y) and 0x17 + the byte `n` at (x +
// 0x28, y). PSX 0x801D2030.
extern "C" void __cdecl EffectHud_DrawCount(int x, int y, unsigned slot, unsigned n) {
    const U ux = static_cast<U>(x), uy = static_cast<U>(y);
    DrawMode(0, slot);
    const U prim = DrawSprite(0x17, slot, ux, uy);
    DrawSprite((prim & 0xFFFFFF00u) | static_cast<unsigned char>(n + 0x17), slot, ux + 0x28, uy);
}

// original 0x465310 (Effect_KindHandlers 5): EffectKind05_States by +1.
extern "C" void __cdecl EffectKind05_Run(void) { Run("EffectKind05_Run", ::bof3::Img(EffectKind05_States_addr), 6, S()[1]); }

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
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(ObjTrio[0].x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(ObjTrio[0].z) - 0x8000u);
    unsigned char* s = S();
    const long h = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, static_cast<U>(h) + 0x100);
    const unsigned char* const script = PtrAt(::bof3::Img(at::kPanelScript.v));
    const auto product = static_cast<std::int32_t>(script[2] * L(::bof3::Img(at::kRecord2.v) + 0xC));
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
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, L(::bof3::Img(at::kKind05Rise.v) + S()[6] * 8u));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, L(::bof3::Img(at::kKind05Rise.v) + 4 + S()[6] * 8u));
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
    SH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(W(PtrAt(::bof3::Img(at::kPanelSet.v)) + 0xC)));
    SH_CALL(Sprite_SetAnimation)(1);
    const unsigned char spawned = SH_CALL(Effect_FindFree)();
    if (spawned != 0xFF) {
        unsigned char* const e = Spawned("EffectKind05_Start", spawned);
        e[0] = 1;
        e[5] = 0xA;
    }
    S()[7] = PtrAt(::bof3::Img(at::kPanelSet.v))[0xF];
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x465540 (EffectKind05_States[2]; PSX 0x801D23F0): Field_Kind2Z the target +0x10
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
    if (z == L(::bof3::Img(at::kKind05Bounce.v) + row) + target) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->work_20, L(::bof3::Img(at::kKind05Bounce.v) + 4 + row));
        a = S();
    }
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->step_y) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(a)->work_20));
    a = S();
    const U row2 = Row05("EffectKind05_Rise", a[6]);
    if (rv::As<records::EffectObject>(a)->z > static_cast<std::int32_t>(L(::bof3::Img(at::kKind05Bounce.v) + row2) + rv::As<records::EffectWork>(a)->work_10) && SL(a + 0x14) < 0) {
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

// original 0x4656A0 (EffectKind05_States[3]; PSX 0x801D25EC): when Sprite_ScriptTickOnce
// answers non-zero, Sprite_SetAnimation(3) and +1 up; Sprite_UpdateScreen (a
// tail jump). PSX 0x801D25EC (table-anchored).
extern "C" void __cdecl EffectKind05_Land(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0) {
        SH_CALL(Sprite_SetAnimation)(3);
        S()[1] = static_cast<unsigned char>(S()[1] + 1);
    }
    SH_CALL(Sprite_UpdateScreen)();
}

// original 0x4656C0 (EffectKind05_States[4]; PSX 0x801D2644): the wait +0xA counted down; at
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
            const unsigned char* const set = PtrAt(::bof3::Img(at::kPanelSet.v));
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
    SH_CALL(Sprite_EnsureAnimation)(SL(At(::bof3::Img(Effect_Objects_addr) + 0x280 + 0x10)) >= 0 ? 3 : 2);
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(rv::As<records::SpriteObject>(Object("EffectKind05_Follow", Effect1a_PanelObject))->x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(rv::As<records::SpriteObject>(Object("EffectKind05_Follow", Effect1a_PanelObject))->z));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, static_cast<std::uint32_t>(rv::As<records::SpriteObject>(Object("EffectKind05_Follow", Effect1a_PanelObject))->height_whole));
    SH_CALL(Sprite_ScriptTick)();
    DepthPair();
}

// original 0x4658C0 (Effect_KindHandlers 0xA): EffectKind0A_States by +1.
extern "C" void __cdecl EffectKind0A_Run(void) { Run("EffectKind0A_Run", ::bof3::Img(EffectKind0A_States_addr), 2, S()[1]); }

// original 0x4658E0 (EffectKind0A_States[0]): record 0's bytes +0x24..+0x28
// copied; +0x29 = 5; the height +0x3C 0; +0x48, +0x5C 0; +0x5D..+0x5F 0x9C;
// +0x60..+0x6C 0; EffectKind0A_Follow; +1 up.
extern "C" void __cdecl EffectKind0A_Start(void) {
    for (U i = 0x24; i <= 0x28; ++i) S()[i] = B(::bof3::Img(at::kRecord0.v) + i);
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

// original 0x465A60 (Effect_KindHandlers 0xC): EffectKind0C_States by +1.
extern "C" void __cdecl EffectKind0C_Run(void) { Run("EffectKind0C_Run", ::bof3::Img(EffectKind0C_States_addr), 3, S()[1]); }

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
        const unsigned margin = PtrAt(::bof3::Img(at::kPanelScript.v))[3] * 4u + 0xC;
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
    const unsigned b3 = PtrAt(::bof3::Img(at::kPanelScript.v))[3];
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
    const unsigned b3b = PtrAt(::bof3::Img(at::kPanelScript.v))[3];
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
    if (ObjTrio[0].step != 4) return;
    if (Object(kWho, Effect1a_PanelObject)[4] == 3) return;
    if (arrow <= static_cast<std::int16_t>(right) && arrow >= static_cast<std::int16_t>(left)) return;
    if ((Frame_Counter & 0xF) != 0) return;
    SH_CALL(Sound_PlayEffect)(0x207);
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

// original 0x465E50 (EffectKind0C_Aim's and E1B's; PSX 0x801D3240): the draw mode (0, slot);
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

// original 0x465F10 (Effect_KindHandlers 0xD): EffectKind0D_States by +1.
extern "C" void __cdecl EffectKind0D_Run(void) { Run("EffectKind0D_Run", ::bof3::Img(EffectKind0D_States_addr), 5, S()[1]); }

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
    const U id = W(::bof3::Img(at::kKind0DSounds.v) + 2 * v);
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
    Widescreen_SweepMark(S());   // DIV-0041 (I38): drawn mapped onto the wide picture; nothing when narrow
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x466020 (EffectKind0D_States[3]): +9 down, +1 up at 0;
// Sprite_QueueOverlay (a tail jump).
extern "C" void __cdecl EffectKind0D_Hold(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* const a = S();
    if (a[9] == 0) a[1] = static_cast<unsigned char>(a[1] + 1);
    Widescreen_SweepMark(S());   // DIV-0041 (I38): at 160, the map's fixed point
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
    Widescreen_SweepMark(S());   // DIV-0041 (I38): drawn mapped onto the wide picture; nothing when narrow
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x466080 (Effect_KindHandlers 0xF): unless the record is record 3
// (0x7E1360) or record 3's +1 is above 3, Effect_Release (a tail jump); else
// EffectKind0F_States by +1.
extern "C" void __cdecl EffectKind0F_Run(void) {
    unsigned char* const s = S();
    if (Key(s) != (::bof3::Img(Effect_Objects_addr) + 0x180) && Effect_Objects[3].state <= 3) {
        SH_CALL(Effect_Release)();
        return;
    }
    Run("EffectKind0F_Run", ::bof3::Img(EffectKind0F_States_addr), 13, s[1]);
}

// original 0x4660B0 (EffectKind0F_States[1]; PSX 0x801D364C): the window's y +0x30 = -0x19,
// +7 0, +1 up. PSX 0x801D364C.
extern "C" void __cdecl EffectKind0F_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0xFFE7);
    S()[7] = 0;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x4660D0 (EffectKind0F_States[2]; PSX 0x801D3680): +0x30 += 8, held at 0x12 (s16)
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

// original 0x466120 (EffectKind0F_States[3]; PSX 0x801D36F4): with the spawn byte of 0x653C04's
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

// original 0x4661B0 (EffectKind0F_States[4]; PSX 0x801D3820): the window at 0x12; the text
// +0x4B names drawn whole at (0x1D, 0x14). PSX 0x801D3820.
extern "C" void __cdecl EffectKind0F_Title(void) {
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    const U text = L(Text("EffectKind0F_Title", S()[0x4B]));
    DrawText(0x1D, 0x14, 0, StringCount(text), text);
}

// original 0x466200 (EffectKind0F_States[5]; PSX 0x801D3894): EffectKind0F_Close; the text
// +0x4B names drawn whole at (0x1D, +0x30 + 2). PSX 0x801D3894.
extern "C" void __cdecl EffectKind0F_TitleClose(void) {
    SH_CALL(EffectKind0F_Close)();
    const U text = L(Text("EffectKind0F_TitleClose", S()[0x4B]));
    const U count = StringCount(text);
    DrawText(0x1D, (static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30) + 2) & 0xFFFF, 0, count, text);
}

// original 0x466240 (EffectKind0F_States[6]): the window at 0x12. PSX 0x801D3904.
extern "C" void __cdecl EffectKind0F_Frame(void) { WindowBox(0x14, 0x12, 0x118, 0x13, 0); }

// original 0x466260 (EffectKind0F_States[7], and _TitleClose's; PSX 0x801D3934): +0x30 -= 8;
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
    } else if (effect_1a::g_empty_line_none && TextAt(rv::As<records::EffectObject>(c)->script)[0] == 0) {
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
    DrawText(static_cast<std::uint32_t>(rv::As<records::EffectWork>(c)->work_2e), L(::bof3::Img(at::kRecord3.v) + 0x30) + 2, 0, (eax & 0xFFFFFF00u) | typed, L(Text(kWho, c[0x4B])));
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
                DrawText(static_cast<std::uint16_t>(width + static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e)), L(::bof3::Img(at::kRecord3.v) + 0x30) + 2, label.pen, 1,
                         label.text);
            } else {
                DrawText(static_cast<std::uint16_t>(a[0x49] * 12u + static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e)), L(::bof3::Img(at::kRecord3.v) + 0x30) + 2,
                         label.pen, 1, label.text);
            }
        }
    }
    unsigned char* const a = S();
    DrawText(static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e), L(::bof3::Img(at::kRecord3.v) + 0x30) + 2, 0, a[0x49], rv::As<records::EffectObject>(a)->script);
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
        DrawText(static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e), L(::bof3::Img(at::kRecord3.v) + 0x30) + 2, 0, typed, rv::As<records::EffectObject>(a)->script);
        unsigned char* const c = S();
        if (B(Text(kWho, c[0x4B]) + 4) == 0xFF) return;
        const LabelDraw label = Label(kWho, c[0x4B]);   // DIV-0069 / DIV-0136
        if (FishingText_On()) {
            // DIV-0069: after the typed characters' real width.
            const U width = FishingText_Width(TextAt(rv::As<records::EffectObject>(c)->script), c[0x49]);
            DrawText(static_cast<std::uint16_t>(width + static_cast<std::uint32_t>(rv::As<records::EffectWork>(c)->work_2e)), L(::bof3::Img(at::kRecord3.v) + 0x30) + 2, label.pen, 1,
                     label.text);
        } else {
            DrawText(static_cast<std::uint16_t>(c[0x49] * 12u + static_cast<std::uint32_t>(rv::As<records::EffectWork>(c)->work_2e)), L(::bof3::Img(at::kRecord3.v) + 0x30) + 2, label.pen,
                     1, label.text);
        }
        return;
    }
    if (B(Text(kWho, a[0x4B]) + 4) == 0xFF) {
        SH_CALL(Effect_Release)();
        return;
    }
    const LabelDraw label = Label(kWho, a[0x4B]);   // DIV-0069 / DIV-0136
    DrawText(static_cast<std::uint32_t>(rv::As<records::EffectWork>(a)->work_2e), L(::bof3::Img(at::kRecord3.v) + 0x30) + 2, label.pen, 1, label.text);
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

// original 0x4667A0 (Effect_KindHandlers 0x1A): EffectKind1A_States by +1.
extern "C" void __cdecl EffectKind1A_Run(void) { Run("EffectKind1A_Run", ::bof3::Img(EffectKind1A_States_addr), 17, S()[1]); }

// original 0x4667C0 (EffectKind1A_States[1]; PSX 0x801D4300): +8, +6 0; +7 0x80; +9 4; +0xA 0;
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

// original 0x466840 (EffectKind1A_States[2]; PSX 0x801D4394): +9 down; at 0 +0xB = 1 and +1
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

// original 0x466950 (EffectKind1A_States[3]; PSX 0x801D458C): the window at 0x12, the option
// boxes with ObjTrio 0's option bit, the member rows at 0x10, item list A at
// 0xA4. PSX 0x801D458C.
extern "C" void __cdecl EffectKind1A_ItemsA(void) {
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
    MemberRows(0x10, 0x40);
    ItemListA(0xA4, 0x40);
}

// original 0x4669B0 (EffectKind1A_States[4]; PSX 0x801D4614): as _ItemsA with item list B.
// PSX 0x801D4614.
extern "C" void __cdecl EffectKind1A_ItemsB(void) {
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
    MemberRows(0x10, 0x40);
    ItemListB(0xA4, 0x40);
}

// original 0x466A10 (EffectKind1A_States[5]; PSX 0x801D469C): +9 up and the window and lists
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

// original 0x466B30 (EffectKind1A_States[6]; PSX 0x801D48A0): the word +0x3E walks 0..0x1F
// from 0 to the first kind whose count byte 0x9040EC is set - +0x3C = 1 when
// none is (the walk back at 0); the window at 0x12, the option boxes; +9 = 4,
// +1 up. PSX 0x801D48A0.
extern "C" void __cdecl EffectKind1A_FindKind(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_frac, 0);
    unsigned char* a = S();
    for (;;) {
        const U kind = static_cast<std::uint32_t>(static_cast<std::uint16_t>(rv::As<records::EffectObject>(a)->height_whole));
        if (B(::bof3::Img(at::kKindCounts.v) + static_cast<U>(static_cast<std::int16_t>(kind))) != 0) break;
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->height_whole, kind + 1);
        a = S();
        if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(a)->height_whole)) >= 0x20) {
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->height_whole, 0);
            a = S();
        }
        if (static_cast<std::uint32_t>(static_cast<std::uint16_t>(rv::As<records::EffectObject>(a)->height_whole)) == 0) {
            rv::Store<std::uint32_t>(rv::As<records::EffectObject>(a)->height_frac, 1);
            break;
        }
    }
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
    S()[9] = 4;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x466BE0 (EffectKind1A_States[7]; PSX 0x801D49A0): +9 down; the panel sliding in by
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

// original 0x466D30 (EffectKind1A_States[8]; PSX 0x801D4B7C): the panel in place - header, the
// kind's row at 0, its icon at (0x80, 0x5B), its message (the word +0x3E) at
// (8, 0x94) - or 0xFF / 0xFFFF without a count; the total, the title, the
// window and the option boxes. PSX 0x801D4B7C.
extern "C" void __cdecl EffectKind1A_Panel(void) {
    if (PanelRow()) {
        SH_CALL(FieldPanel_DrawKindIcon)(0x80, 0x5B, S()[0x3E]);
        SH_CALL(FieldPanel_DrawMessage)(8, 0x94, static_cast<std::uint32_t>(static_cast<std::uint16_t>(rv::As<records::EffectObject>(S())->height_whole)));
    } else {
        SH_CALL(FieldPanel_DrawKindIcon)(0x80, 0x5B, 0xFF);
        SH_CALL(FieldPanel_DrawMessage)(8, 0x94, 0xFFFF);
    }
    SH_CALL(FieldPanel_DrawTotal)(0xB0, 0x9C);
    PanelTitle();
    WindowBox(0x14, 0x12, 0x118, 0x13, 0);
    OptionBoxes(0x58, 0x28, OptionBits());
}

// original 0x466E10 (EffectKind1A_States[9]; PSX 0x801D4CA0): +9 up; the panel's header and
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

// original 0x466F70 (EffectKind1A_States[10]; PSX 0x801D4E8C): +9 down; as _PanelNext with the
// icon's sign the other way; at +9 0 +1 = 8. PSX 0x801D4E8C.
extern "C" void __cdecl EffectKind1A_PanelBack(void) {
    PanelSlide(-1, false);
    unsigned char* const a = S();
    if (a[9] == 0) a[1] = 8;
}

// original 0x4670C0 (EffectKind1A_States[11]; PSX 0x801D5064): the window and option boxes
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

// original 0x467270 (EffectKind1A_States[12]; PSX 0x801D52F0): the window at 0x12 and the
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

// original 0x4672F0 (kind 0xF state 26; PSX 0x801D53B4): the window (0x14, 0x12, 0x118, 0x13, 0),
// the toggles at (0x58, 0x28), +9 - 1, the list at (0x27, 60 * +9 + 0x40) - the
// y built on the record pointer's upper half (movzx ax), which only its low 16
// bits carry on; at +9 = 0 a kind-0x1A record in state 0x10, sub-state the
// record's +0x3E, taken unchecked from Effect_FindFree, and +1 up.
extern "C" void __cdecl EffectKind0F_ListOpen() {
    SH_CALL(Panel_DrawWindow)(0x14, 0x12, 0x118, 0x13, 0);
    SH_CALL(EffectKind0F_DrawToggles)(0x58, 0x28, ToggleBits());
    unsigned char* s = Sc();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    const U y = ((ScKey() & 0xFFFF0000u) | Sc()[9]) * 60u + 0x40u;
    SH_CALL(EffectKind0F_DrawMessageList)(0x27, static_cast<int>(y));
    if (Sc()[9] != 0) return;
    const unsigned char k = SH_CALL(Effect_FindFree)();
    s = Sc();
    unsigned char* const r = SpawnMark(k, "EffectKind0F_ListOpen");
    r[2] = s[0x3E];
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4673B0 (kind 0xF state 27; PSX 0x801D5514): the window, the toggles and the list at
// rest (0x27, 0x40).
extern "C" void __cdecl EffectKind0F_ListShow() {
    SH_CALL(Panel_DrawWindow)(0x14, 0x12, 0x118, 0x13, 0);
    SH_CALL(EffectKind0F_DrawToggles)(0x58, 0x28, ToggleBits());
    SH_CALL(EffectKind0F_DrawMessageList)(0x27, 0x40);
}

// original 0x467400 (kind 0xF state 28; PSX 0x801D5590): +9 + 1. With +0xB 0 the window rises
// (y 0x12 - 16 * +9, on the pointer's upper half as in state 26) and the
// toggles slide right (x 45 * +9 + 0x58 over a callee's eax: its low 16 bits),
// all unlit; else both at rest and lit. The list slides down (60 * +9 + 0x40,
// low 16 bits). At +9 = 4: +1 = 0 with +0xB 0, else 2 for +6 = 0, 6 for +6 = 1.
extern "C" void __cdecl EffectKind0F_ListClose() {
    unsigned char* s = Sc();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = Sc();
    if (s[0xB] == 0) {
        const U y = 0x12u - (((ScKey() & 0xFFFF0000u) | s[9]) << 4);
        SH_CALL(Panel_DrawWindow)(0x14, static_cast<int>(y), 0x118, 0x13, 0);
        SH_CALL(EffectKind0F_DrawToggles)(static_cast<int>(Sc()[9] * 45u + 0x58u), 0x28, 0);
    } else {
        SH_CALL(Panel_DrawWindow)(0x14, 0x12, 0x118, 0x13, 0);
        SH_CALL(EffectKind0F_DrawToggles)(0x58, 0x28, ToggleBits());
    }
    SH_CALL(EffectKind0F_DrawMessageList)(0x27, static_cast<int>(Sc()[9] * 60u + 0x40u));
    s = Sc();
    if (s[9] != 4) return;
    if (s[0xB] == 0) s[1] = 0;
    else if (s[6] == 0) s[1] = 2;
    else if (s[6] == 1) s[1] = 6;
}

// original 0x4674F0 (kind 0xF state 29; PSX 0x801D5710): while record 6's +8 is 0 and its +1 is
// 0xE, EffectKind0F_Children[+2] (a tail jump); else Effect_Release.
extern "C" void __cdecl EffectKind0F_Child() {
    if (rv::Bytes(&Effect_Objects[6])[0x8] == 0 && Effect_Objects[6].state == 0xE) {
        StepAt(::bof3::Img(EffectKind0F_Children_addr), Sc()[2], at::kChildrenCount, "EffectKind0F_Child")();
        return;
    }
    SH_CALL(Effect_Release)();
}

// originals 0x467520, 0x4677F0, 0x467A60, 0x467D60, 0x468020, 0x468320 (kind 0xF
// states 30..35, EffectKind0F_Children 0..5): each sub-kind's step table by +3.
extern "C" void __cdecl EffectKind0F_Child0() {
    StepAt(::bof3::Img(EffectKind0F_Child0Steps_addr), Sc()[3], at::kChild0Count, "EffectKind0F_Child0")();
}

// original 0x467540 (PSX 0x801D57CC): two kind-0x1A records in state 0x10, +3 = 2 and 4 (each
// only if Effect_FindFree found one); bank 0x47, placed at (0x5E, 0xBA), +9 =
// +0xA = 0; +3 + 1.
extern "C" void __cdecl EffectKind0F_Child0Start() {
    unsigned char k = SH_CALL(Effect_FindFree)();
    if (k != 0xFF) SpawnMark(k, "EffectKind0F_Child0Start")[3] = 2;
    k = SH_CALL(Effect_FindFree)();
    if (k != 0xFF) SpawnMark(k, "EffectKind0F_Child0Start")[3] = 4;
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0x5E, 0xBA);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x467630 (PSX 0x801D5974): +9 0: the next record of the script 0x653CCC (+0xA; an
// animation of 0xFF goes to record `frames` first): Sprite_SetAnimation, +9 its
// frames, +0xA + 1; else +9 - 1 and Sprite_ScriptTick. Then Sprite_QueueOverlay.
extern "C" void __cdecl EffectKind0F_Child0Animate() {
    static const char kWho[] = "EffectKind0F_Child0Animate";
    auto at_ = [](int i) { return ByteOf(::bof3::Img(at::kChild0Script.v), at::kChild0ScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(2 * s[0xA]) == 0xFF) s[0xA] = at_(2 * s[0xA] + 1);
        SH_CALL(Sprite_SetAnimation)(at_(2 * Sc()[0xA]));
        s = Sc();
        s[9] = at_(2 * s[0xA] + 1);
        s = Sc();
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        SH_CALL(Sprite_ScriptTick)();
    }
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x4676A0 (PSX 0x801D5A64): bank 0x21, placed at (0xA2, 0xBA) with +0x2A 1; +6, +9,
// +0xA 0, dword +0xC 0, dword +0x18 4; Sprite_SetAnimation(0); +3 + 1.
extern "C" void __cdecl EffectKind0F_Child0Second() {
    SH_CALL(Sprite_SetAnimationBank)(0x21);
    Place(1, 1, 0xA2, 0xBA);
    unsigned char* s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[6] = 0;
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, 0);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(s)->work_18, 4);
    SH_CALL(Sprite_SetAnimation)(0);
    s = Sc();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x467750 (PSX 0x801D5B64): Sprite_ScriptTick, then Sprite_QueueOverlay (a tail jump).
extern "C" void __cdecl EffectKind0F_Child0Tick() {
    SH_CALL(Sprite_ScriptTick)();
    SH_CALL(Sprite_QueueOverlay)();
}

// original 0x467760 (PSX 0x801D5B8C): bank 0x47, placed at (0xE8, 0xBA), +9 = +0xA = 0,
// Sprite_SetAnimation(0xA), +3 - 1.
extern "C" void __cdecl EffectKind0F_Child0Back() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xE8, 0xBA);
    unsigned char* s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    SH_CALL(Sprite_SetAnimation)(0xA);
    s = Sc();
    s[3] = static_cast<unsigned char>(s[3] - 1);
}

extern "C" void __cdecl EffectKind0F_Child1() {
    StepAt(::bof3::Img(EffectKind0F_Child1Steps_addr), Sc()[3], at::kChild1Count, "EffectKind0F_Child1")();
}

// original 0x467810 (PSX 0x801D5CA8): bank 0x47, placed at (0xEC, 0xA2); +6, +9, +0xA 0, dword
// +0xC 0, dword +0x18 4; +3 + 1.
extern "C" void __cdecl EffectKind0F_Child1Start() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xEC, 0xA2);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[6] = 0;
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, 0);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(s)->work_18, 4);
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x4678C0 (PSX 0x801D5DA4): the script 0x653CEC of (animation, frames, mode) as
// sub-kind 0's (an animation of 0xFF goes to record `frames` and zeroes +0xC,
// +6 and sets +0x18 4); Sprite_QueueOverlay; the draw mode (0, 1) and sprites
// 0x15, 0x16, 0x10. The current record's mode (the one before +0xA): 0 bounces
// dword +0xC by +0x18 between 0 and 0x60 (the step negated at each end); 1
// counts +6 up on even frames. Then 0x465120(0x39, 0x97, word +0xC, 1), +6
// printed at (0xAA, 0x94) and Sprite_QueueOverlay again.
extern "C" void __cdecl EffectKind0F_Child1Meter() {
    static const char kWho[] = "EffectKind0F_Child1Meter";
    auto at_ = [](int i) { return ByteOf(::bof3::Img(at::kChild1Script.v), at::kChild1ScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(3 * s[0xA]) == 0xFF) {
            s[0xA] = at_(3 * s[0xA] + 1);
            rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, 0);
            rv::Store<std::int32_t>(rv::As<records::EffectObject>(s)->work_18, 4);
            s[6] = 0;
        }
        SH_CALL(Sprite_SetAnimation)(at_(3 * Sc()[0xA]));
        s = Sc();
        s[9] = at_(3 * s[0xA] + 1);
        s = Sc();
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        SH_CALL(Sprite_ScriptTick)();
    }
    SH_CALL(Sprite_QueueOverlay)();
    DrawModeEffect1b(0, 1);
    DrawSpriteEffect1b(0x15, 1, 0x32, 0x7A);
    DrawSpriteEffect1b(0x16, 1, 0xBC, 0x7A);
    DrawSpriteEffect1b(0x10, 1, 0x36, 0x80);
    s = Sc();
    const unsigned char mode = at_(3 * s[0xA] - 1);
    if (mode == 0) {
        rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c)) + static_cast<U>(rv::As<records::EffectObject>(s)->work_18)));
        if (rv::As<records::EffectObject>(s)->work_18 > 0) {
            if (static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c) > 0x60) {
                rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, 0x60);
                rv::Store<std::int32_t>(rv::As<records::EffectObject>(s)->work_18, static_cast<std::int32_t>(0u - static_cast<U>(rv::As<records::EffectObject>(s)->work_18)));
            }
        } else if (static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c) < 0) {
            rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, 0);
            rv::Store<std::int32_t>(rv::As<records::EffectObject>(s)->work_18, static_cast<std::int32_t>(0u - static_cast<U>(rv::As<records::EffectObject>(s)->work_18)));
        }
    } else if (mode == 1 && (Frame_Counter & 1) == 0) {
        s[6] = static_cast<unsigned char>(s[6] + 1);
    }
    Call4(at::kE1aBarG4.v, 0x39, 0x97, Word(Sc() + 0xC), 1);
    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Boss26Fx_CountFormat), static_cast<unsigned>(Sc()[6]));
    SH_CALL(Text_DrawFont12)(0xAA, 0x94, 0, TextEffect1b(::bof3::Img(at::kPrint.v)));
    SH_CALL(Sprite_QueueOverlay)();
}

extern "C" void __cdecl EffectKind0F_Child2() {
    StepAt(::bof3::Img(EffectKind0F_Child2Steps_addr), Sc()[3], at::kChild2Count, "EffectKind0F_Child2")();
}

// original 0x467A80 (PSX 0x801D6060): bank 0x47, placed at (0xE8, 0xBA), +6 = +9 = +0xA = 0; +3 + 1.
extern "C" void __cdecl EffectKind0F_Child2Start() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xE8, 0xBA);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[6] = 0;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x467B10 (PSX 0x801D6144): the script 0x653D08 of (animation, frames, step) - an
// animation of 0 goes to record `frames` and zeroes +6; a record whose step is
// 0x50 spawns a kind-0x1A record in state 0x10, +2 = +3 = 2, placed at (0xE8,
// 0x9E), from Effect_FindFree unchecked. Sprite_QueueOverlay. Every fourth frame
// the current step (not 0x50) is added to the gauge +6, held to 0..0x18 (a sum
// with bit 7 set is 0). E1A's gauge draws, the draw mode (0, 1), sprite 0x14,
// and with a step the mark at (0xF4, +6 + 0x68 on the pointer's upper half).
extern "C" void __cdecl EffectKind0F_Child2Gauge() {
    static const char kWho[] = "EffectKind0F_Child2Gauge";
    auto at_ = [](int i) { return ByteOf(::bof3::Img(at::kChild2Script.v), at::kChild2ScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(3 * s[0xA]) == 0) {
            s[0xA] = at_(3 * s[0xA] + 1);
            s[6] = 0;
        }
        SH_CALL(Sprite_SetAnimation)(at_(3 * Sc()[0xA]));
        s = Sc();
        s[9] = at_(3 * s[0xA] + 1);
        s = Sc();
        if (at_(3 * s[0xA] + 2) == 0x50) {
            const unsigned char k = SH_CALL(Effect_FindFree)();
            unsigned char* const r = SpawnMark(k, kWho);
            r[2] = 2;
            r[3] = 2;
            s = Sc();
            SetWord(r + 0x2E, 0xE8);
            SetWord(r + 0x30, 0x9E);
        }
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        SH_CALL(Sprite_ScriptTick)();
    }
    SH_CALL(Sprite_QueueOverlay)();
    s = Sc();
    const unsigned char step = at_(3 * s[0xA] - 1);
    if (step != 0x50 && (Frame_Counter & 3) == 0) {
        const auto v = static_cast<unsigned char>(s[6] + step);
        s[6] = (v & 0x80) != 0 ? 0 : (v > 0x18 ? 0x18 : v);
    }
    Call3(at::kE1aGaugeA.v, 0xE4, 0x68, 1);
    Call3(at::kE1aGaugeB.v, 0xE4, 0x78, 1);
    DrawModeEffect1b(0, 1);
    DrawSpriteEffect1b(0x14, 1, 0xDC, 0x60);
    s = Sc();
    if (at_(3 * s[0xA] - 1) != 0) Call3(at::kE1aGaugeMark.v, 0xF4, ((ScKey() & 0xFFFF0000u) | s[6]) + 0x68u, 1);
}

// original 0x467CA0 (PSX 0x801D6430): bank 0x2B; +0x24 0x80, +0x29 = +0x2A = 0, +0x48 2, dwords
// +0x40 = +0x44 = 0x8000, the tint bytes 0, +9 0x40, +6 0;
// Sprite_SetAnimation(3); +3 + 1.
extern "C" void __cdecl EffectKind0F_Child2Fade() {
    SH_CALL(Sprite_SetAnimationBank)(0x2B);
    unsigned char* s = Sc();
    s[0x24] = 0x80;
    s[0x29] = 0;
    s[0x2A] = 0;
    s[0x48] = 2;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(s)->scale_x, 0x8000);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(s)->scale_y, 0x8000);
    s[0x5D] = s[0x5E] = s[0x5F] = 0;
    s[9] = 0x40;
    s[6] = 0;
    SH_CALL(Sprite_SetAnimation)(3);
    s = Sc();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x467D30 (PSX 0x801D6504): +9 - 1; at 0 Effect_Release, else with bit 3 of +9
// Sprite_QueueOverlay (both tail jumps).
extern "C" void __cdecl EffectKind0F_Child2Blink() {
    unsigned char* s = Sc();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    const unsigned char n = Sc()[9];
    if (n == 0) SH_CALL(Effect_Release)();
    else if ((n & 8) != 0) SH_CALL(Sprite_QueueOverlay)();
}

extern "C" void __cdecl EffectKind0F_Child3() {
    StepAt(::bof3::Img(EffectKind0F_Child3Steps_addr), Sc()[3], at::kChild3Count, "EffectKind0F_Child3")();
}

// original 0x467D80 (PSX 0x801D65B4): bank 0x47, placed at (0xCA, 0xBA), +9 = +0xA = +0xB = 0;
// +3 + 1, then Sprite_SetAnimation(3).
extern "C" void __cdecl EffectKind0F_Child3Start() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xCA, 0xBA);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[0xB] = 0;
    s[3] = static_cast<unsigned char>(s[3] + 1);
    SH_CALL(Sprite_SetAnimation)(3);
}

// original 0x467E10: the grid 0x653D34 (four rows of marks, each to an 0xFF).
// While +0xA counts down: E1A's 0x4652D0(0xD4, 0x8A, 0, row + 1 over the
// pointer's upper bytes); else +9 + 1 and the lit column is +9 / 12 - at its
// row's 0xFF +0xA = 0x32. With +0xA 0 and the lit mark 1, the sprite script
// ticks for 10 frames of 12. Every mark drawn: the lit one (the row +0xB, the
// column +9 / 12, 10 frames of 12) by Text_DrawAt with the string of index 2 *
// mark, the rest by Text_DrawSmall (the row's colour 3 * mark over the string
// pointer's upper half, the others' 7 - mark). Sprite_QueueOverlay; +0xA down,
// at 0 +9 = 0 and the next row (after 3, row 0).
extern "C" void __cdecl EffectKind0F_Child3Grid() {
    static const char kWho[] = "EffectKind0F_Child3Grid";
    auto grid = [](int i) { return ByteOf(::bof3::Img(at::kChild3Grid.v), at::kChild3GridBytes, i, kWho); };
    unsigned char* s = Sc();
    unsigned column;
    if (s[0xA] != 0) {
        Call4(at::kE1aDigits.v, 0xD4, 0x8A, 0, (ScKey() & 0xFFFFFF00u) | static_cast<unsigned char>(s[0xB] + 1));
        s = Sc();
        column = s[9] / 12u;
    } else {
        s[9] = static_cast<unsigned char>(s[9] + 1);
        s = Sc();
        column = s[9] / 12u;
        if (grid(s[0xB] * 8 + static_cast<int>(column)) == 0xFF) s[0xA] = 0x32;
    }
    if (s[0xA] == 0 && grid(s[0xB] * 8 + static_cast<int>(column)) == 1 && s[9] % 12u < 10) {
        SH_CALL(Sprite_ScriptTick)();
    }
    for (unsigned row = 0; row < 4; ++row) {
        for (unsigned col = 0; col < 8; ++col) {
            const unsigned char mark = grid(static_cast<int>(row * 8 + col));
            if (mark == 0xFF) break;
            s = Sc();
            if (row == s[0xB]) {
                if (s[9] % 12u < 10 && col == column) {
                    const U text = static_cast<U>(Long(At(::bof3::Img(at::kGridText.v) + 8u * mark)));
                    DrawAt(static_cast<int>(12 * col + 0x84), static_cast<int>(13 * row + 0x9B),
                           static_cast<unsigned char>(mark << 1), 1, TextEffect1b(text));
                } else {
                    const U text = static_cast<U>(Long(At(::bof3::Img(at::kGridText.v) + 4u * mark)));
                    const auto colour = static_cast<std::uint16_t>(static_cast<signed char>(mark) * 3);
                    SH_CALL(Text_DrawSmall)(static_cast<int>(12 * col + 0x86), static_cast<int>(13 * row + 0x9E),
                                            (text & 0xFFFF0000u) | colour, 1, TextEffect1b(text));
                }
            } else {
                const U text = static_cast<U>(Long(At(::bof3::Img(at::kGridText.v) + 4u * mark)));
                SH_CALL(Text_DrawSmall)(static_cast<int>(12 * col + 0x86), static_cast<int>(13 * row + 0x9E),
                                        static_cast<unsigned char>(7 - mark), 1, TextEffect1b(text));
            }
        }
    }
    SH_CALL(Sprite_QueueOverlay)();
    s = Sc();
    if (s[0xA] == 0) return;
    s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
    s = Sc();
    if (s[0xA] != 0) return;
    s[9] = 0;
    s = Sc();
    s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    s = Sc();
    if (s[0xB] > 3) s[0xB] = 0;
}

extern "C" void __cdecl EffectKind0F_Child4() {
    StepAt(::bof3::Img(EffectKind0F_Child4Steps_addr), Sc()[3], at::kChild4Count, "EffectKind0F_Child4")();
}

// original 0x468040 (PSX 0x801D6A74): bank 0x47, placed at (0xF0, 0xBA); +6 = +9 = +0xA = 0,
// dwords +0xC = +0x10 = 0x6A, +0x4B 0xFF; +3 + 1. Also called by
// EffectKind0F_Child5Start.
extern "C" void __cdecl EffectKind0F_Child4Start() {
    SH_CALL(Sprite_SetAnimationBank)(0x47);
    Place(1, 0, 0xF0, 0xBA);
    unsigned char* const s = Sc();
    s[9] = 0;
    s[0xA] = 0;
    s[6] = 0;
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, 0x6A);
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_10, 0x6A);
    s[0x4B] = 0xFF;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x4680F0: the move script 0x653D5C stepped and the cursor moved (as
// above); EffectKind0F_DrawCursorPanel(dy not negative); E1A's
// 0x465D90(0x7E, 0x5C, 0, 1) and 0x465E50(0x8F, 0x69, 0x20, 0, 1, 0).
extern "C" void __cdecl EffectKind0F_Child4Move() {
    static const char kWho[] = "EffectKind0F_Child4Move";
    auto at_ = [](int i) { return ByteOf(::bof3::Img(at::kChild4Script.v), at::kMoveScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(4 * s[0xA]) == 0xFF) s[0xA] = at_(4 * s[0xA] + 1);
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(at_(4 * Sc()[0xA]) & 0x7F));
        s = Sc();
        s[9] = at_(4 * s[0xA] + 1);
        s = Sc();
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        s = Sc();
        if ((at_(4 * s[0xA] - 4) & 0x80) == 0) SH_CALL(Sprite_ScriptTick)();
    }
    MoveCursor(::bof3::Img(at::kChild4Script.v), kWho);
    SH_CALL(EffectKind0F_DrawCursorPanel)(MoveByte(::bof3::Img(at::kChild4Script.v), 4 * Sc()[0xA] - 1, kWho) >= 0 ? 1u : 0u);
    Call4(at::kE1aMarker.v, 0x7E, 0x5C, 0, 1);
    Call6(at::kE1aRange.v, 0x8F, 0x69, 0x20, 0, 1, 0);
}

// original 0x468210 (down, a byte): Sprite_QueueOverlay; the draw mode (0, 1),
// sprites 0x15, 0x16, 0x11; the cursor held to x 0x39..0x79 and y 0x41..0x94;
// E1A's range 0x465E50(x, 0xB3, 0x20, (+6 & 7) * 2, 1, y outside x..x + 0x20)
// and marker 0x465D90(y, 0xB3, down 0 ? 0 : 1, 1).
extern "C" void __cdecl EffectKind0F_DrawCursorPanel(unsigned down) {
    SH_CALL(Sprite_QueueOverlay)();
    DrawModeEffect1b(0, 1);
    DrawSpriteEffect1b(0x15, 1, 0x32, 0x96);
    DrawSpriteEffect1b(0x16, 1, 0xBC, 0x96);
    DrawSpriteEffect1b(0x11, 1, 0x36, 0x9C);
    unsigned char* s = Sc();
    std::int32_t v = static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c);
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, v < 0x39 ? 0x39 : (v > 0x79 ? 0x79 : v));
    s = Sc();
    v = static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10);
    rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_10, v < 0x41 ? 0x41 : (v > 0x94 ? 0x94 : v));
    s = Sc();
    const std::int32_t cx = static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c), cy = static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10);
    const U outside = (cy >= cx && cy <= cx + 0x20) ? 0u : 1u;
    Call6(at::kE1aRange.v, Word(s + 0xC), 0xB3, 0x20, static_cast<unsigned char>((s[6] & 7) << 1), 1, outside);
    Call4(at::kE1aMarker.v, Word(Sc() + 0x10), 0xB3, (down & 0xFF) == 0 ? 0u : 1u, 1);
}

extern "C" void __cdecl EffectKind0F_Child5() {
    StepAt(::bof3::Img(EffectKind0F_Child5Steps_addr), Sc()[3], at::kChild5Count, "EffectKind0F_Child5")();
}

// original 0x468340 (PSX 0x801D6F50): EffectKind0F_Child4Start, then +0xB = 0x37.
extern "C" void __cdecl EffectKind0F_Child5Start() {
    SH_CALL(EffectKind0F_Child4Start)();
    Sc()[0xB] = 0x37;
}

// original 0x468350 (PSX 0x801D6F7C): the move script 0x653DAC as sub-kind 4's (going back to
// record `frames` also sets +0xB 0x37 and the cursor to (0x6A, 0x6A)); a record
// of animation 9 spawns a kind-0x1A record in state 0x10, +2 = +3 = 2, at
// (0xF2, 0x9E), when Effect_FindFree finds one. The cursor moved; the panel
// drawn; every 16th frame a positive dy counts +0xB up to 0x3C; +0xB printed at
// (0xA9, 0xB0) in colour 2 on frames with bit 2, else 0.
extern "C" void __cdecl EffectKind0F_Child5Move() {
    static const char kWho[] = "EffectKind0F_Child5Move";
    auto at_ = [](int i) { return ByteOf(::bof3::Img(at::kChild5Script.v), at::kMoveScriptBytes, i, kWho); };
    unsigned char* s = Sc();
    if (s[9] == 0) {
        if (at_(4 * s[0xA]) == 0xFF) {
            s[0xA] = at_(4 * s[0xA] + 1);
            s[0xB] = 0x37;
            rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, 0x6A);
            rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_10, 0x6A);
        }
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(at_(4 * Sc()[0xA]) & 0x7F));
        s = Sc();
        if ((at_(4 * s[0xA]) & 0x7F) == 9) {
            const unsigned char k = SH_CALL(Effect_FindFree)();
            if (k != 0xFF) {
                unsigned char* const r = SpawnMark(k, kWho);
                r[2] = 2;
                r[3] = 2;
                SetWord(r + 0x2E, 0xF2);
                SetWord(r + 0x30, 0x9E);
            }
        }
        s = Sc();
        s[9] = at_(4 * s[0xA] + 1);
        s = Sc();
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        s = Sc();
        if ((at_(4 * s[0xA] - 4) & 0x80) == 0) SH_CALL(Sprite_ScriptTick)();
    }
    MoveCursor(::bof3::Img(at::kChild5Script.v), kWho);
    SH_CALL(EffectKind0F_DrawCursorPanel)(MoveByte(::bof3::Img(at::kChild5Script.v), 4 * Sc()[0xA] - 1, kWho) >= 0 ? 1u : 0u);
    if ((Frame_Counter & 0xF) == 0) {
        s = Sc();
        if (MoveByte(::bof3::Img(at::kChild5Script.v), 4 * s[0xA] - 1, kWho) > 0) {
            s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
            if (s[0xB] > 0x3C) s[0xB] = 0x3C;
        }
    }
    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Boss26Fx_CountFormat), static_cast<unsigned>(Sc()[0xB]));
    SH_CALL(Text_DrawFont12)(0xA9, 0xB0, (Frame_Counter & 4) != 0 ? 2 : 0, TextEffect1b(::bof3::Img(at::kPrint.v)));
}

// original 0x468560 (x, y): the window (x, y, 0xF2, 0x9F); with +8 (the scroll's
// direction, signed) not 0 the list scrolls: at +9 0 a new title (+0x3C) sets
// the target +0x20 = 9 * +0x3C, a step down for +8 > 0 (+0x14 wraps below 0 to
// 0x35), +9 and +0xA up; then +0xA 0: at +0x14 = +0x20 the scroll ends (+8 =
// +9 = 0, a kind-0x1A record in state 0x10, sub-state +0x3E, from
// Effect_FindFree unchecked), else another step down and +0xA up; +0xA not 0:
// +0xA up, at 4 back to 0 with a step up for +8 < 0 (past 0x35 to 0). Nine or
// ten lines of the script pool from +0x14 (the line table 0x653E00, ids 0xFFFF
// skipped), offset by 3 * +0xA; the two bars, the title, the frame and
// "+0x3C + 1" printed.
extern "C" void __cdecl EffectKind0F_DrawMessageList(int x, int y) {
    static const char kWho[] = "EffectKind0F_DrawMessageList";
    SH_CALL(Menu_DrawBox)(x, y, 0xF2, 0x9F, 0xF0, Style());
    unsigned char* s = Sc();
    auto step_down = [](unsigned char* r) {
        if (static_cast<signed char>(r[8]) <= 0) return r;
        SetLong(r + 0x14, Long(r + 0x14) - 1);
        r = Sc();
        if (Long(r + 0x14) < 0) {
            SetLong(r + 0x14, 0x35);
            r = Sc();
        }
        return r;
    };
    if (s[8] != 0) {
        if (s[9] == 0) {
            if (static_cast<int>(static_cast<short>(Word(s + 0x3E))) != static_cast<int>(Word(s + 0x3C))) {
                SetLong(s + 0x20, static_cast<std::int32_t>(Word(s + 0x3C) * 9u));
                s = Sc();
                SetWord(s + 0x3E, Word(s + 0x3C));
                s = Sc();
            }
            s = step_down(s);
            s[9] = static_cast<unsigned char>(s[9] + 1);
            s = Sc();
            s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
            s = Sc();
        }
        if (s[0xA] == 0) {
            if (Long(s + 0x14) == Long(s + 0x20)) {
                s[9] = 0;
                Sc()[8] = 0;
                const unsigned char k = SH_CALL(Effect_FindFree)();
                s = Sc();
                SpawnMark(k, kWho)[2] = s[0x3E];
            } else {
                s = step_down(s);
                s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
            }
        } else {
            s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
            s = Sc();
            if (s[0xA] == 4) {
                s[0xA] = 0;
                s = Sc();
                if (static_cast<signed char>(s[8]) < 0) {
                    SetLong(s + 0x14, Long(s + 0x14) + 1);
                    s = Sc();
                    if (Long(s + 0x14) > 0x35) SetLong(s + 0x14, 0);
                }
            }
        }
    }
    s = Sc();
    U top;
    unsigned lines;
    if (s[0xA] != 0) {
        top = s[8] == 1 ? static_cast<U>(y) + 3u * (s[0xA] + 4u) : static_cast<U>(y) - 3u * s[0xA] + 0x19u;
        lines = 10;
    } else {
        top = static_cast<U>(y) + 0x19u;
        lines = 9;
    }
    auto line = static_cast<unsigned char>(Long(s + 0x14));
    for (unsigned n = 0; n < lines; ++n) {
        const std::uint16_t id = WordOf(::bof3::Img(EffectKind0F_RuleLineIds_addr), at::kLineCount, line, kWho);
        if (id != 0xFFFF) DrawAt(x + 6, static_cast<int>(top + 13 * n), 0, 0xFF, Pool(id));
        line = static_cast<unsigned char>(line + 1);
        if (line > 0x35) line = 0;
    }
    SH_CALL(Menu_DrawBox)(x, y + 6, 0xF2, 0xE, 0, Style());
    SH_CALL(Menu_DrawBox)(x, y + 0x94, 0xF2, 0xB, 0x30, Style());
    DrawAt(x + 0x3D, y + 6, 0, 0xFF, Pool(WordOf(::bof3::Img(EffectKind0F_RuleTitleIds_addr), at::kTitleCount, rv::As<records::EffectObject>(Sc())->height_frac, kWho)));
    SH_CALL(EffectKind0F_DrawListFrame)(x, y);
    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(TextEffect1b(::bof3::Img(EffectKind0F_PageFormat_addr))), rv::As<records::EffectObject>(Sc())->height_frac + 1u);
    SH_CALL(Text_DrawFont8)(x + 0xCA, y + 0x92, 0, TextEffect1b(::bof3::Img(at::kPrint.v)));
}

// original 0x468840 (x, y; PSX 0x801D77CC): the draw mode (9, 1); the top edge (sprite 0x25, 23
// of 0x26 every 8 from x + 0x20, 0x27, 0x28), the two side quads (0x78 high,
// edges 0 and 1), the bottom edge (0x2A, 23 of 0x2B from x + 8, 0x2C, three
// 0x2D, 0x2E, 0x2F) at y + 0x90.
extern "C" void __cdecl EffectKind0F_DrawListFrame(int x, int y) {
    DrawModeEffect1b(9, 1);
    DrawSpriteEffect1b(0x25, 1, x, y);
    for (int i = 0; i < 0x17; ++i) DrawSpriteEffect1b(0x26, 1, x + 8 * i + 0x20, y);
    DrawSpriteEffect1b(0x27, 1, x + 0xD8, y);
    DrawSpriteEffect1b(0x28, 1, x + 0xF0, y);
    SH_CALL(Panel_DrawEdgeQuad)(x, y + 0x18, 0x78, 0);
    SH_CALL(Panel_DrawEdgeQuad)(x + 0xF0, y + 0x18, 0x78, 1);
    DrawSpriteEffect1b(0x2A, 1, x, y + 0x90);
    for (int i = 0; i < 0x17; ++i) DrawSpriteEffect1b(0x2B, 1, x + 8 * i + 8, y + 0x90);
    DrawSpriteEffect1b(0x2C, 1, x + 0xC0, y + 0x90);
    for (int i = 0; i < 3; ++i) DrawSpriteEffect1b(0x2D, 1, x + 8 * i + 0xC8, y + 0x90);
    DrawSpriteEffect1b(0x2E, 1, x + 0xE0, y + 0x90);
    DrawSpriteEffect1b(0x2F, 1, x + 0xE7, y + 0x90);
}

// original 0x468950 (x, y, height, which): a POLY_FT4 at the packet cursor (the
// pointer read once, before Gpu_SetPolyFT4, as the original keeps it): tpage
// word 0x7887, the CLUT word ((c & 0x3C0) | 0x400) >> 6 of the record's c; the
// corners (x, y), (x + w, y), (x, y + height), (x + w, y + height) as floats (x,
// y s16, height a byte, w the record's word); u, u + w, v, v + h from the
// record's bytes; shade 0x80; committed (1, 0x48). The records 0x653E6C, five of
// ten bytes, by the low byte of `which`.
extern "C" void __cdecl Panel_DrawEdgeQuad(int x, int y, unsigned height, unsigned which) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    const unsigned k = which & 0xFF;
    if (k >= at::kEdgeQuadCount)
        bof3::Fatal("Panel_DrawEdgeQuad: quad %u past the %u records of 0x%X (the original reads past them)", k,
                    at::kEdgeQuadCount, at::kEdgeQuads.v);
    const unsigned char* const r = At(::bof3::Img(at::kEdgeQuads.v) + 10 * k);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, 0x7887);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, ((Word(r + 8) & 0x3C0u) | 0x400u) >> 6);
    const int sx = static_cast<short>(x), sy = static_cast<short>(y);
    F(p + 8, sx);
    F(p + 0x18, static_cast<int>(Word(r + 4)) + sx);
    F(p + 0x28, sx);
    F(p + 0x38, static_cast<int>(Word(r + 4)) + sx);
    F(p + 0xC, sy);
    F(p + 0x1C, sy);
    F(p + 0x2C, sy + static_cast<int>(height & 0xFF));
    F(p + 0x3C, sy + static_cast<int>(height & 0xFF));
    const auto u2 = static_cast<unsigned char>(r[0] + r[4]);
    p[0x14] = r[0];
    p[0x24] = u2;
    p[0x34] = p[0x14];
    p[0x44] = u2;
    p[0x15] = r[2];
    p[0x25] = r[2];
    p[0x35] = p[0x45] = static_cast<unsigned char>(r[6] + r[2]);
    p[4] = p[5] = p[6] = 0x80;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x468A40: the draw mode (0, 1), sprite 0x12 at (0xF0, 0x42), the
// label 0x653EBC at (0x119, 0x45); s16 +0x3E + 1 printed and drawn at (0xF7,
// 0x45), two characters. (A first character of 0x20 is written back as 0x20:
// the original's store changes nothing.)
extern "C" void __cdecl EffectKind0F_DrawCountHeader() {
    DrawModeEffect1b(0, 1);
    DrawSpriteEffect1b(0x12, 1, 0xF0, 0x42);
    SH_CALL(Text_DrawFont12)(0x119, 0x45, 0, TextEffect1b(::bof3::Img(EffectKind0F_CountLabel_addr)));
    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(Boss26Fx_CountFormat),
                         static_cast<int>(static_cast<short>(rv::As<records::EffectObject>(Sc())->height_whole)) + 1);
    if (BEffect1b(::bof3::Img(at::kPrint.v)) == 0x20) BEffect1b(::bof3::Img(at::kPrint.v)) = 0x20;
    DrawAt(0xF7, 0x45, 0, 2, TextEffect1b(::bof3::Img(at::kPrint.v)));
}

// original 0x468AC0 (x, y, bits): three toggle boxes at x + 0x30 i, each lit by
// bit i of the byte; the three labels (the pointers 0x66A088..0x66A090) at x +
// 0xA, 0x35, 0x6A, y + 4, two, three and two characters: colour 0 when the byte
// is 0, else 0 for a lit box and 7 for the others. (The original keeps the
// three bits in its first argument's slot.)
extern "C" void __cdecl EffectKind0F_DrawToggles(int x, int y, unsigned bits) {
    const auto byte = static_cast<unsigned char>(bits);
    unsigned char lit[3];
    for (int i = 0; i < 3; ++i) {
        lit[i] = static_cast<unsigned char>((byte >> i) & 1);
        SH_CALL(EffectKind0F_DrawToggle)(x + 0x30 * i, y, lit[i]);
    }
    static const int kX[3] = {0xA, 0x35, 0x6A}, kCount[3] = {2, 3, 2};
    for (int i = 0; i < 3; ++i) {
        const auto text = static_cast<U>(Long(At(::bof3::Img(Effect1b_ToggleText_addr) + 4u * i)));
        const int colour = byte == 0 ? 0 : static_cast<unsigned char>(7 - 7 * lit[i]);
        if (FishingText_On()) {
            // DIVERGENCE DIV-0069: under a Latin overlay the label whole, centred
            // in its box (x + 0x30 i + 2, 0x28 wide) by its real width - the US
            // module's x + 6 + 0x30 i for its four letters.
            const int width = static_cast<int>(TextAdvance_Width(TextEffect1b(text)));
            DrawAt(x + 0x30 * i + 0x16 - width / 2, y + 4, colour, 0xFF, TextEffect1b(text));
        } else {
            DrawAt(x + kX[i], y + 4, colour, kCount[i], TextEffect1b(text));
        }
    }
}

// original 0x468BB0 (x, y, lit; PSX 0x801D7E34): a box (x + 2, y + 2, 0x28, 0x10); the draw mode
// (8, 1); sprites 4 * lit + 0x1D at (x, y), + 0x1E at (x, y + 0x10), four of +
// 0x1F from x + 8 and (lit + 8) * 4 at x + 0x28, all byte ids.
extern "C" void __cdecl EffectKind0F_DrawToggle(int x, int y, unsigned lit) {
    SH_CALL(Menu_DrawBox)(x + 2, y + 2, 0x28, 0x10, 0, Style());
    DrawModeEffect1b(8, 1);
    const auto l4 = static_cast<unsigned char>(lit << 2);
    DrawSpriteEffect1b(static_cast<unsigned char>(l4 + 0x1D), 1, x, y);
    DrawSpriteEffect1b(static_cast<unsigned char>(l4 + 0x1E), 1, x, y + 0x10);
    for (int i = 0; i < 4; ++i) DrawSpriteEffect1b(static_cast<unsigned char>(l4 + 0x1F), 1, x + 8 * i + 8, y + 0x10);
    DrawSpriteEffect1b(static_cast<unsigned char>((lit + 8) << 2), 1, x + 0x28, y + 0x10);
}

// original 0x468C50 (x, y): the window (x, y + 1, 0x8B, 0x9E); every accessory of
// the inventory's 0x80 ids whose category is 0xB, a row of 13 each: the one at
// dword +0xC gives its index to word +0x36 and, with the leader's +4 2 and +7
// bit 0 clear, is drawn lit (its name at +0x19 in colour 7 over +0x17 in 0, a
// count above 1 likewise); the others their name and count in 0. Then +0x18 the
// number drawn and +0xC held below it; the frame, the scroll mark (x + 0x80, y +
// 0x1C, 0x72) and the label 0x653EA0.
extern "C" void __cdecl EffectKind0F_DrawItemsB(int x, int y) {
    static const char kWho[] = "EffectKind0F_DrawItemsB";
    SH_CALL(Menu_DrawBox)(x, y + 1, 0x8B, 0x9E, 0xF0, Style());
    unsigned drawn = 0;
    for (unsigned n = 0; n < 0x80; ++n) {
        if (Category(BEffect1b(::bof3::Img(at::kItemIds.v) + n), kWho) != 0xB) continue;
        unsigned char* s = Sc();
        if (static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c) == static_cast<std::int32_t>(drawn)) rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, n);
        s = Sc();
        const int row = y + 13 * static_cast<int>(drawn);
        if (ObjTrio[0].state4 == 2 && (s[7] & 1) == 0 && static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c) == static_cast<std::int32_t>(drawn)) {
            DrawAt(x + 5, row + 0x19, 7, NameCount(), AccessoryName(BEffect1b(::bof3::Img(at::kItemIds.v) + n)));
            DrawAt(x + 5, row + 0x17, 0, NameCount(), AccessoryName(BEffect1b(::bof3::Img(at::kItemIds.v) + n)));
            const unsigned char count = BEffect1b(::bof3::Img(at::kItemCounts.v) + n);
            if (count > 1) {
                SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(TextEffect1b(::bof3::Img(at::kCountFormat.v))), static_cast<unsigned>(count));
                SH_CALL(Text_DrawFont8)(x + 0x68, row + 0x1D, 7, TextEffect1b(::bof3::Img(at::kPrint.v)));
                SH_CALL(Text_DrawFont8)(x + 0x68, row + 0x1B, 0, TextEffect1b(::bof3::Img(at::kPrint.v)));
            }
        } else {
            DrawAt(x + 5, row + 0x19, 0, NameCount(), AccessoryName(BEffect1b(::bof3::Img(at::kItemIds.v) + n)));
            const unsigned char count = BEffect1b(::bof3::Img(at::kItemCounts.v) + n);
            if (count > 1) {
                SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(TextEffect1b(::bof3::Img(at::kCountFormat.v))), static_cast<unsigned>(count));
                SH_CALL(Text_DrawFont8)(x + 0x68, row + 0x1D, 0, TextEffect1b(::bof3::Img(at::kPrint.v)));
            }
        }
        ++drawn;
    }
    unsigned char* s = Sc();
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(s)->work_18, static_cast<std::int32_t>(drawn));
    s = Sc();
    if (static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c) >= rv::As<records::EffectObject>(s)->work_18) rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectObject>(s)->work_18 == 0 ? 0 : rv::As<records::EffectObject>(s)->work_18 - 1);
    SH_CALL(EffectKind0F_DrawItemFrame)(x, y);
    SH_CALL(EffectKind0F_DrawScrollMark)(x + 0x80, y + 0x1C, 0x72);
    DrawHeading(0, x + 0x35, y + 7, 3, ::bof3::Img(EffectKind0F_PanelTitles_addr));
}

// original 0x468E50 (x, y, height; PSX 0x801D82F4): a POLY_FT4 (the cursor read once, before
// Gpu_SetPolyFT4): tpage 0x7887, CLUT word 0x1D; corners (x, y), (x + 8, y), (x,
// y + height), (x + 8, y + height) as floats (x, y s16, height a byte); u 0x58,
// 0x60, v 0x9A, 0x9E; shade 0x80; committed (1, 0x48).
extern "C" void __cdecl EffectKind0F_DrawScrollMark(int x, int y, unsigned height) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    const int sx = static_cast<short>(x), sy = static_cast<short>(y);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->tpage, 0x1D);
    rv::Store<std::uint32_t>(rv::As<records::GpuPolyFT4>(p)->clut, 0x7887);
    F(p + 8, sx);
    F(p + 0x18, sx + 8);   // fadd 8.0f (0x5C41CC): a whole number
    F(p + 0x28, sx);
    F(p + 0x38, sx + 8);
    F(p + 0xC, sy);
    F(p + 0x1C, sy);
    F(p + 0x2C, sy + static_cast<int>(height & 0xFF));
    F(p + 0x3C, sy + static_cast<int>(height & 0xFF));
    p[0x24] = p[0x44] = 0x60;
    p[0x14] = p[0x34] = 0x58;
    p[0x15] = p[0x25] = 0x9A;
    p[0x35] = p[0x45] = 0x9E;
    p[4] = p[5] = p[6] = 0x80;
    SH_CALL(Gfx_CommitPrim)(1, 0x48);
}

// original 0x468F00 (x, y): the window (x, y + 1, 0x8B, 0x9E, 0x80). With +0xA
// (a scroll's frames) not 0: +0xA - 1, ten rows from +0x38 (+8, the direction,
// 0: from +0x38 - 1), offset 0x19 - 4 * +0xA (+8 not 0) or 4 * +0xA + 0xD; else
// nine from +0x38 at 0x19. Every accessory of category 0xA from that one on, a
// row of 13: dword +0x10's is lit (leader's +4 2 and +7 bit 0 set) and gives its
// index to word +0x3A. +0x1C the category's count less 1; +0x10 held below the
// rows drawn. The frame; the scroll mark at y + 0x1C + 6 * +0x38 (+-2 * +0xA),
// its height 0xA8 - 6 * count (0x72, at 0x1C, for nine or fewer); the label
// 0x653EA4.
extern "C" void __cdecl EffectKind0F_DrawItemsA(int x, int y) {
    static const char kWho[] = "EffectKind0F_DrawItemsA";
    SH_CALL(Menu_DrawBox)(x, y + 1, 0x8B, 0x9E, 0x80, Style());
    unsigned char* s = Sc();
    unsigned char top, count, offset;
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        s = Sc();
        const unsigned char held = s[8];
        top = held != 0 ? s[0x38] : static_cast<unsigned char>(s[0x38] - 1);
        count = 10;
        offset = held != 0 ? static_cast<unsigned char>(0x19 - static_cast<unsigned char>(s[0xA] << 2))
                           : static_cast<unsigned char>((s[0xA] << 2) + 0xD);
    } else {
        top = s[0x38];
        count = 9;
        offset = 0x19;
    }
    unsigned drawn = 0;
    std::int32_t seen = 0;
    for (unsigned n = 0; n < 0x80; ++n) {
        const unsigned char id = BEffect1b(::bof3::Img(at::kItemIds.v) + n);
        if (Category(id, kWho) != 0xA) continue;
        if (seen >= static_cast<std::int32_t>(top) && drawn < count) {
            s = Sc();
            const int row = y + 13 * static_cast<int>(drawn) + offset;
            if (ObjTrio[0].state4 == 2 && (s[7] & 1) != 0 && static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10) == static_cast<std::int32_t>(drawn)) {
                DrawAt(x + 5, row, 7, NameCount(), AccessoryName(id));
                DrawAt(x + 5, row - 2, 0, NameCount(), AccessoryName(BEffect1b(::bof3::Img(at::kItemIds.v) + n)));
                const unsigned char c = BEffect1b(::bof3::Img(at::kItemCounts.v) + n);
                if (c > 1) {
                    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(TextEffect1b(::bof3::Img(at::kCountFormat.v))), static_cast<unsigned>(c));
                    SH_CALL(Text_DrawFont8)(x + 0x68, row + 4, 7, TextEffect1b(::bof3::Img(at::kPrint.v)));
                    SH_CALL(Text_DrawFont8)(x + 0x68, row + 2, 0, TextEffect1b(::bof3::Img(at::kPrint.v)));
                }
            } else {
                DrawAt(x + 5, row, 0, NameCount(), AccessoryName(id));
                const unsigned char c = BEffect1b(::bof3::Img(at::kItemCounts.v) + n);
                if (c > 1) {
                    SH_CALL(Crt_sprintf)(Print(), reinterpret_cast<const char*>(TextEffect1b(::bof3::Img(at::kCountFormat.v))), static_cast<unsigned>(c));
                    SH_CALL(Text_DrawFont8)(x + 0x68, row + 4, 0, TextEffect1b(::bof3::Img(at::kPrint.v)));
                }
            }
            s = Sc();
            if (static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10) == static_cast<std::int32_t>(drawn)) rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, n);
            ++drawn;
        }
        ++seen;
    }
    s = Sc();
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(s)->work_1c, seen - 1);
    s = Sc();
    if (static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10) >= static_cast<std::int32_t>(drawn))
        rv::Store<std::int32_t>(rv::As<records::EffectWork>(s)->work_10, rv::As<records::EffectObject>(s)->work_1c == 0 || drawn == 0 ? 0 : static_cast<std::int32_t>(drawn - 1));
    SH_CALL(EffectKind0F_DrawItemFrame)(x, y);
    int total = 0;
    for (unsigned n = 0; n < 0x80; ++n)
        if (Category(BEffect1b(::bof3::Img(at::kItemIds.v) + n), kWho) == 0xA) ++total;
    unsigned char height, mark;
    if (total <= 9) {
        height = 0x72;
        mark = 0x1C;
    } else {
        height = static_cast<unsigned char>(0xA8 - static_cast<unsigned char>(total * 6));
        s = Sc();
        mark = static_cast<unsigned char>(s[0x38] * 6 + 0x1C);
        const unsigned char a = s[0xA];
        if (a != 0) mark = static_cast<unsigned char>(mark + (s[8] != 0 ? a * 2 : a * 0xFE));
    }
    SH_CALL(EffectKind0F_DrawScrollMark)(x + 0x80, y + mark, height);
    DrawHeading(1, x + 0x2F, y + 7, 4, ::bof3::Img(EffectKind0F_PanelTitles_addr) + 0x4);
}

// original 0x469210 (x, y): the twin frame; the label 0x653EA8 at (x + 0x2A, y
// + 7). By +7: bit 7 - both equipped accessories (0x904130, 0x90412E) with
// their icons (E1G's 0x594D50) and names in colour 0; else bit 0 clear - the
// first lit (icon flag 1, name in 7 over the icon and name again, the last in
// +7's colour), the second plain; bit 0 set - the first plain, the second lit
// (its last name in colour +7 & 2). The label 0x653EAD at (x + 0x2A, y + 0x47);
// unless word +0x2C is 0xFFFF, that script-pool message at (0x19, 0x9A).
extern "C" void __cdecl EffectKind0F_DrawEquipped(int x, int y) {
    SH_CALL(EffectKind0F_DrawTwinFrame)(x, y);
    DrawHeading(2, x + 0x2A, y + 7, 5, ::bof3::Img(EffectKind0F_PanelTitles_addr) + 0x8);
    const unsigned char bits = Sc()[7];
    const U ix = static_cast<U>(x) + 0x10u;
    auto icon = [ix](int iy, unsigned char item, U flag) { Call5(at::kE1gItemIcon.v, ix, static_cast<U>(iy), item, 3, flag); };
    if ((bits & 0x80) != 0) {
        icon(y + 0x1C, BEffect1b(::bof3::Img(at::kEquipA.v)), 0);
        DrawAt(x + 0x1C, y + 0x1A, 0, NameCount(), AccessoryName(BEffect1b(::bof3::Img(at::kEquipA.v))));
        icon(y + 0x2C, CharStats_ExtraItem, 0);
        DrawAt(x + 0x1C, y + 0x2A, 0, NameCount(), AccessoryName(CharStats_ExtraItem));
    } else if ((bits & 1) == 0) {
        icon(y + 0x1C, BEffect1b(::bof3::Img(at::kEquipA.v)), 1);
        DrawAt(x + 0x1C, y + 0x1A, 7, NameCount(), AccessoryName(BEffect1b(::bof3::Img(at::kEquipA.v))));
        icon(y + 0x1A, BEffect1b(::bof3::Img(at::kEquipA.v)), 0);
        DrawAt(x + 0x1C, y + 0x18, Sc()[7], NameCount(), AccessoryName(BEffect1b(::bof3::Img(at::kEquipA.v))));
        icon(y + 0x2C, CharStats_ExtraItem, 0);
        DrawAt(x + 0x1C, y + 0x2A, 0, NameCount(), AccessoryName(CharStats_ExtraItem));
    } else {
        icon(y + 0x1C, BEffect1b(::bof3::Img(at::kEquipA.v)), 0);
        DrawAt(x + 0x1C, y + 0x1A, 0, NameCount(), AccessoryName(BEffect1b(::bof3::Img(at::kEquipA.v))));
        icon(y + 0x2C, CharStats_ExtraItem, 1);
        DrawAt(x + 0x1C, y + 0x2A, 7, NameCount(), AccessoryName(CharStats_ExtraItem));
        icon(y + 0x2A, CharStats_ExtraItem, 0);
        DrawAt(x + 0x1C, y + 0x28, Sc()[7] & 2, NameCount(), AccessoryName(CharStats_ExtraItem));
    }
    DrawHeading(3, x + 0x2A, y + 0x47, 5, ::bof3::Img(EffectKind0F_PanelTitles_addr) + 0xd);
    const std::uint16_t message = rv::As<records::EffectObject>(Sc())->column;
    if (message != 0xFFFF) DrawAt(0x19, 0x9A, 0, 0xFF, Pool(message));
}

// original 0x469490 (x, y): windows (x, y + 2, 0x8A, 0x3A) and (x, y + 0x42,
// 0x8A, 0x5C); the draw mode (9, 1); two sprite frames, each a top edge (0x25,
// ten of 0x26 from x + 0x20, 0x27 at x + 0x70, 0x28 at x + 0x88), two side quads
// (0x1E high, then 0x40) and a bottom edge (0x3C, sixteen of 0x3D from x + 8,
// 0x3E at x + 0x88), at y and y + 0x36, then y + 0x40 and y + 0x98.
extern "C" void __cdecl EffectKind0F_DrawTwinFrame(int x, int y) {
    SH_CALL(Menu_DrawBox)(x, y + 2, 0x8A, 0x3A, 0x80, Style());
    SH_CALL(Menu_DrawBox)(x, y + 0x42, 0x8A, 0x5C, 0x80, Style());
    DrawModeEffect1b(9, 1);
    DrawSpriteEffect1b(0x25, 1, x, y);
    for (int i = 0; i < 10; ++i) DrawSpriteEffect1b(0x26, 1, x + 8 * i + 0x20, y);
    DrawSpriteEffect1b(0x27, 1, x + 0x70, y);
    DrawSpriteEffect1b(0x28, 1, x + 0x88, y);
    SH_CALL(Panel_DrawEdgeQuad)(x, y + 0x18, 0x1E, 0);
    SH_CALL(Panel_DrawEdgeQuad)(x + 0x88, y + 0x18, 0x1E, 1);
    DrawSpriteEffect1b(0x3C, 1, x, y + 0x36);
    for (int i = 0; i < 16; ++i) DrawSpriteEffect1b(0x3D, 1, x + 8 * i + 8, y + 0x36);
    DrawSpriteEffect1b(0x3E, 1, x + 0x88, y + 0x36);
    DrawSpriteEffect1b(0x25, 1, x, y + 0x40);
    for (int i = 0; i < 10; ++i) DrawSpriteEffect1b(0x26, 1, x + 8 * i + 0x20, y + 0x40);
    DrawSpriteEffect1b(0x27, 1, x + 0x70, y + 0x40);
    DrawSpriteEffect1b(0x28, 1, x + 0x88, y + 0x40);
    SH_CALL(Panel_DrawEdgeQuad)(x, y + 0x58, 0x40, 0);
    SH_CALL(Panel_DrawEdgeQuad)(x + 0x88, y + 0x58, 0x40, 1);
    DrawSpriteEffect1b(0x3C, 1, x, y + 0x98);
    for (int i = 0; i < 16; ++i) DrawSpriteEffect1b(0x3D, 1, x + 8 * i + 8, y + 0x98);
    DrawSpriteEffect1b(0x3E, 1, x + 0x88, y + 0x98);
}

// original 0x469630 (x, y): the window (x, y + 2, 0x8B, 0x14); the draw mode (9,
// 1); the top edge (0x3F, 0x40, 0x41, eight of 0x26 from x + 0x28, 0x42, 0x43,
// 0x44), a side quad (0x78 high, edge 0), sprite 0x46 at (x + 0x80, y + 0x18),
// a quad (0x68 high, edge 4) at (x + 0x80, y + 0x28), the bottom edge (0x2A,
// fifteen of 0x2B from x + 8) at y + 0x90 and 0x45 at (x + 0x80, y + 0x88).
extern "C" void __cdecl EffectKind0F_DrawItemFrame(int x, int y) {
    SH_CALL(Menu_DrawBox)(x, y + 2, 0x8B, 0x14, 0, Style());
    DrawModeEffect1b(9, 1);
    DrawSpriteEffect1b(0x3F, 1, x, y);
    DrawSpriteEffect1b(0x40, 1, x + 8, y);
    DrawSpriteEffect1b(0x41, 1, x + 0x10, y);
    for (int i = 0; i < 8; ++i) DrawSpriteEffect1b(0x26, 1, x + 8 * i + 0x28, y);
    DrawSpriteEffect1b(0x42, 1, x + 0x68, y);
    DrawSpriteEffect1b(0x43, 1, x + 0x78, y);
    DrawSpriteEffect1b(0x44, 1, x + 0x80, y);
    SH_CALL(Panel_DrawEdgeQuad)(x, y + 0x18, 0x78, 0);
    DrawSpriteEffect1b(0x46, 1, x + 0x80, y + 0x18);
    SH_CALL(Panel_DrawEdgeQuad)(x + 0x80, y + 0x28, 0x68, 4);
    DrawSpriteEffect1b(0x2A, 1, x, y + 0x90);
    for (int i = 0; i < 15; ++i) DrawSpriteEffect1b(0x2B, 1, x + 8 * i + 8, y + 0x90);
    DrawSpriteEffect1b(0x45, 1, x + 0x80, y + 0x88);
}

// original 0x469750 (x, y, w, h, colour; PSX 0x801D9580): the bevel (x, y, w, h), then the edges
// (x + 2, y + 2, w - 5, h - 5, colour).
extern "C" void __cdecl Panel_DrawWindow(int x, int y, int w, int h, unsigned colour) {
    SH_CALL(Panel_DrawWindowBevel)(x, y, w, h);
    SH_CALL(Panel_DrawWindowEdges)(x + 2, y + 2, w - 5, h - 5, colour);
}

// original 0x469790 (x, y, w, h; each its low 16 bits): the style's colour; the
// draw mode (6, 2); a POLY_F4 (x + 2, y), (x + w - 2, y), (x, y + 2), (x + w, y +
// 2), a TILE (x, y + 2, w, h - 4) and a POLY_F4 (x, y + h - 2), (x + w, y + h -
// 2), (x + 2, y + h), (x + w - 2, y + h), each semi-transparent (1) and
// committed to slot 2 (0x38, 0x1C, 0x38). Each primitive's pointer is read once,
// before its Gpu_Set* call, as the original keeps it.
extern "C" void __cdecl Panel_DrawWindowBevel(int x, int y, int w, int h) {
    const Colour c = StyleColour();
    DrawModeEffect1b(6, 2);
    const int X = x & 0xFFFF, Y = y & 0xFFFF, W = w & 0xFFFF, H = h & 0xFFFF;
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    F(p + 8, X + 2);
    F(p + 0x14, X + W - 2);
    F(p + 0x20, X);
    F(p + 0x2C, X + W);
    F(p + 0xC, Y);
    F(p + 0x18, Y);
    F(p + 0x24, Y + 2);
    F(p + 0x30, Y + 2);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x38);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    F(p + 8, X);
    F(p + 0xC, Y + 2);
    F(p + 0x14, W);
    F(p + 0x18, H - 4);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x1C);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    F(p + 8, X);
    F(p + 0x14, X + W);
    F(p + 0x20, X + 2);
    F(p + 0x2C, X + W - 2);
    F(p + 0xC, Y + H - 2);
    F(p + 0x18, Y + H - 2);
    F(p + 0x24, Y + H);
    F(p + 0x30, Y + H);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x38);
}

// original 0x469960 (x, y, w, h, colour; each of x..h its low 16 bits, colour a
// byte): the style's colour; the draw mode (7 with a colour, else 6; slot 2), a
// LINE_F3 (x, y + h), (x + w, y + h), (x + w, y); the draw mode (6 or 7), a
// LINE_F3 (x, y + h), (x, y), (x + w, y); each semi-transparent and committed (2,
// 0x2C).
extern "C" void __cdecl Panel_DrawWindowEdges(int x, int y, int w, int h, unsigned colour) {
    const Colour c = StyleColour();
    const bool lit = (colour & 0xFF) != 0;
    DrawModeEffect1b(lit ? 7 : 6, 2);
    const int X = x & 0xFFFF, Y = y & 0xFFFF, W = w & 0xFFFF, H = h & 0xFFFF;
    unsigned char* p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF3)(p);
    F(p + 8, X);
    F(p + 0xC, Y + H);
    F(p + 0x14, X + W);
    F(p + 0x18, Y + H);
    F(p + 0x20, X + W);
    F(p + 0x24, Y);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x2C);
    DrawModeEffect1b(lit ? 6 : 7, 2);
    p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF3)(p);
    F(p + 8, X);
    F(p + 0xC, Y + H);
    F(p + 0x14, X);
    F(p + 0x18, Y);
    F(p + 0x20, X + W);
    F(p + 0x24, Y);
    Shade(p, c);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x2C);
}

// original 0x469AD0 (clut, text, width, x): a code-0x6C glyph primitive at the
// cursor (read once, before Gpu_SetCode6C): the glyph of the text's first
// character (two bytes with bit 7: ((b0 & 0x7F) << 8) + b1, else b0 - 0x26) to
// word +0x16; u from 0xC - (width & 0x7F) when width has bit 7, else 0, to u +
// (width & 0x7F); v 0..0xC; x, x + (width & 0x7F) (words); y record 3's dword
// +0x30 + 3, + 0xC more; shade 0x80; CLUT 0x7800 | (clut & 0xF); committed (1,
// 0x28).
//
// DIV-0069: under the Latin layout a space draws nothing. The one-byte branch
// makes glyph 0x20 - 0x26 = 0xFFFA of it, far past the font (Capcom's lines
// hold no one-byte space; the disc's English ones do - the crash the live check
// met at the first banner, 2026-10-03). The callers move the pen themselves.
//
// DIVERGENCE DIV-0144 (D279): with "FishingLowGlyph" armed (FishingEmpty_Arm,
// after every self-test) a one-byte code below 0x26 draws nothing, under any
// layer. Its glyph c - 0x26 wraps to 0xFFDA.. past the font. A NUL is the
// character of an empty string (the label behind 0x66A2FC and the lines on a
// Japanese disc-built image that no layer wrote), a space what a Japanese
// layer's fishing line holds. The PC's own strings hold neither.
extern "C" void __cdecl EffectKind0F_DrawGlyph(unsigned clut, const unsigned char* text, unsigned width, int x) {
    ++effect_1b::g_glyph_calls;
    if (FishingText_On() && text[0] == 0x20) return;
    if (FishingEmpty_LowGlyphNone() && text[0] < 0x26) return;
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetCode6C)(p);
    const unsigned char c0 = text[0];
    const auto glyph = static_cast<std::uint16_t>((c0 & 0x80) != 0 ? ((c0 & 0x7Fu) << 8) + text[1] : c0 - 0x26u);
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->glyph, glyph);
    const auto wb = static_cast<unsigned char>(width);
    p[0xC] = 0;
    p[0xD] = 0;
    if ((wb & 0x80) != 0) p[0xC] = static_cast<unsigned char>(0xC - (wb & 0x7F));
    const unsigned char u0 = p[0xC];
    const auto w7 = static_cast<unsigned char>(wb & 0x7F);
    p[0x1C] = u0;
    p[0x14] = p[0x24] = static_cast<unsigned char>(u0 + w7);
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->x0, static_cast<unsigned>(x));
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->x2, static_cast<unsigned>(x));
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->x1, static_cast<unsigned>(x) + w7);
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->x3, static_cast<unsigned>(x) + w7);
    p[0x15] = 0;
    p[0x1D] = 0xC;
    p[0x25] = 0xC;
    const U top = static_cast<U>(Long(At(::bof3::Img(Effect_Objects_addr) + 0x1b0))) + 3u;
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->y0, top);
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->y1, top);
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->y2, top + 0xC);
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->y3, top + 0xC);
    p[4] = p[5] = p[6] = 0x80;
    rv::Store<std::uint32_t>(rv::As<records::GpuGlyph>(p)->clut, 0x7800u | (clut & 0xF));
    SH_CALL(Gfx_CommitPrim)(1, 0x28);
}

void EffectKind0fFromEffect1a_Inject() {
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

void EffectKind0fFromEffect1b_Inject() {
    BOF3_INJECT(EffectKind0F_ListOpen);
    BOF3_INJECT(EffectKind0F_ListShow);
    BOF3_INJECT(EffectKind0F_ListClose);
    BOF3_INJECT(EffectKind0F_Child);
    BOF3_INJECT(EffectKind0F_Child0);
    BOF3_INJECT(EffectKind0F_Child0Start);
    BOF3_INJECT(EffectKind0F_Child0Animate);
    BOF3_INJECT(EffectKind0F_Child0Second);
    BOF3_INJECT(EffectKind0F_Child0Tick);
    BOF3_INJECT(EffectKind0F_Child0Back);
    BOF3_INJECT(EffectKind0F_Child1);
    BOF3_INJECT(EffectKind0F_Child1Start);
    BOF3_INJECT(EffectKind0F_Child1Meter);
    BOF3_INJECT(EffectKind0F_Child2);
    BOF3_INJECT(EffectKind0F_Child2Start);
    BOF3_INJECT(EffectKind0F_Child2Gauge);
    BOF3_INJECT(EffectKind0F_Child2Fade);
    BOF3_INJECT(EffectKind0F_Child2Blink);
    BOF3_INJECT(EffectKind0F_Child3);
    BOF3_INJECT(EffectKind0F_Child3Start);
    BOF3_INJECT(EffectKind0F_Child3Grid);
    BOF3_INJECT(EffectKind0F_Child4);
    BOF3_INJECT(EffectKind0F_Child4Start);
    BOF3_INJECT(EffectKind0F_Child4Move);
    BOF3_INJECT(EffectKind0F_DrawCursorPanel);
    BOF3_INJECT(EffectKind0F_Child5);
    BOF3_INJECT(EffectKind0F_Child5Start);
    BOF3_INJECT(EffectKind0F_Child5Move);
    BOF3_INJECT(EffectKind0F_DrawMessageList);
    BOF3_INJECT(EffectKind0F_DrawListFrame);
    BOF3_INJECT(Panel_DrawEdgeQuad);
    BOF3_INJECT(EffectKind0F_DrawCountHeader);
    BOF3_INJECT(EffectKind0F_DrawToggles);
    BOF3_INJECT(EffectKind0F_DrawToggle);
    BOF3_INJECT(EffectKind0F_DrawItemsB);
    BOF3_INJECT(EffectKind0F_DrawScrollMark);
    BOF3_INJECT(EffectKind0F_DrawItemsA);
    BOF3_INJECT(EffectKind0F_DrawEquipped);
    BOF3_INJECT(EffectKind0F_DrawTwinFrame);
    BOF3_INJECT(EffectKind0F_DrawItemFrame);
    BOF3_INJECT(Panel_DrawWindow);
    BOF3_INJECT(Panel_DrawWindowBevel);
    BOF3_INJECT(Panel_DrawWindowEdges);
    BOF3_INJECT(EffectKind0F_DrawGlyph);
}

void EffectKind0fFromRest4f_Inject() {
    BOF3_INJECT(EffectKind02_Start);
    BOF3_INJECT(EffectKind02_SlideIn);
    BOF3_INJECT(EffectKind02_Body);
    BOF3_INJECT(EffectKind02_SlideOut);
    BOF3_INJECT(EffectKind02_RunMode1);
    BOF3_INJECT(EffectKind02_Mode1Tint);
    BOF3_INJECT(EffectKind02_Mode1Brighten);
    BOF3_INJECT(EffectKind02_CopyLeader);
    BOF3_INJECT(EffectKind02_RunMode3);
    BOF3_INJECT(EffectKind02_Mode3Tint);
    BOF3_INJECT(EffectKind02_Mode3Darken);
    BOF3_INJECT(EffectKind02_Mode3Wait);
}
