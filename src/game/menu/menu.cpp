// Moved here by tools/move_funcs.py from field_o.cpp, map_field_objects.cpp, menu_windows.cpp (docs/source-layout-round.md).
#include "game/menu/menu.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/field_o.h"
#include "game/field_o_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/map_field_objects.h"
#include "game/map_field_objects_callees.h"
#include "game/map_rim.h"
#include "game/menu_verbs.h"
#include "game/menu_windows.h"
#include "game/menu_windows_callees.h"
#include "game/move_script_bytes.h"
#include "game/msgbox.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/shoulder_glyphs.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = field_o::at;

}  // namespace

namespace {

using namespace map_field_objects;

}  // namespace

using namespace menu_windows;

namespace {

constexpr auto& AtFieldO = move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

using U = std::uint32_t;

const char* Str(U a) { return reinterpret_cast<const char*>(AtFieldO(a)); }

const unsigned char* Text(U a) { return AtFieldO(a); }

unsigned char* Buf() { return AtFieldO(::bof3::Img(field_o::at::kTextScratch)); }

char* CBuf() { return reinterpret_cast<char*>(AtFieldO(::bof3::Img(field_o::at::kTextScratch))); }

unsigned char* Record(unsigned id) { return AtFieldO(::bof3::Img(CharacterRecords_addr) + (id & 0xFF) * field_o::at::kRecordStride); }

unsigned char Style() { return static_cast<unsigned char>(Config_WindowColour); }

// A signed 16-bit word, as `movsx` / a 16-bit compare reads it.
std::int32_t S16(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

std::int32_t Sb(unsigned v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// A float from a whole number, as `fild dword` / `fstp dword` store it.
void PutFloat(unsigned char* at, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// Crt_sprintf into the text scratch, through the harness.
template <typename... T> void Sprintf(U format, T... v) {
    SH_CALL(Crt_sprintf)(CBuf(), Str(format), v...);
}

// Menu_DrawIconWheel's shade of a corner by its z: ((3 z + 0x24) << 6) / 24
// (signed, truncating - the imul 0x2AAAAAAB / sar 2 idiom) + 0x3F, a byte.
unsigned char WheelShade(std::int32_t z) {
    const auto v = static_cast<std::int32_t>((static_cast<U>(z) * 3u + 0x24u) << 6);
    return static_cast<unsigned char>(v / 24 + 0x3F);
}

// (c X - s Z) >> 11 and (c Z + s X) >> 12: a point turned by the angle, x
// counted double; the four calls in the original's order.
void Turn(int angle, std::int32_t px, std::int32_t pz, std::int32_t& ox, std::int32_t& oz) {
    const auto c0 = static_cast<U>(SH_CALL(Math_Cos)(angle));
    const auto s0 = static_cast<U>(SH_CALL(Math_Sin)(angle));
    ox = static_cast<std::int32_t>(c0 * static_cast<U>(px) - s0 * static_cast<U>(pz)) >> 11;
    const auto c1 = static_cast<U>(SH_CALL(Math_Cos)(angle));
    const auto s1 = static_cast<U>(SH_CALL(Math_Sin)(angle));
    oz = static_cast<std::int32_t>(c1 * static_cast<U>(pz) + s1 * static_cast<U>(px)) >> 12;
}

// A panel's x and y, the words +4 / +6, read afresh at each use as the
// original reads them.
int PanelX(const unsigned char* panel) { return static_cast<int>(Word(panel + 4)); }

int PanelY(const unsigned char* panel) { return static_cast<int>(Word(panel + 6)); }

// The member id of the menu's shown party slot: Character_IndexById by the
// party list's byte at the slot (the slot a signed byte).
unsigned char ShownMember() {
    const unsigned char id = AtFieldO(::bof3::Img(Cond_Flags_addr) + 0xd2 + BattleActorCopies_ItemActor)[0];
    return Character_IndexById[id];
}

}  // namespace

// A title of a table of pointers, centred on 13 six-unit columns - or, under
// DIVERGENCE DIV-0059, on the width its pen covers.
// g_title_live is set by FieldO_Inject after the fuzz, which runs later than
// BattleDraw_Inject's patch and compares with the original's arithmetic.
namespace field_o { bool g_title_live = false; }

namespace {

int Centred(int x, unsigned n, const unsigned char* title) {
    const int count = static_cast<int>(n);
    return x + (field_o::g_title_live ? ListTitle_X(title, count) : 6 * (0xD - count));
}

}  // namespace

namespace map_field_objects {

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace map_field_objects

namespace {

unsigned char* AtMapFieldObjects(map_field_objects::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

map_field_objects::U Byte(map_field_objects::U address) { return AtMapFieldObjects(address)[0]; }

map_field_objects::U DwordMapFieldObjects(const unsigned char* p) {
    map_field_objects::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void SetFloatMapFieldObjects(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

unsigned char* PacketNext() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(DwordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::PacketNextAt())))); }

}  // namespace

namespace menu_windows {

using move_script::At;

}  // namespace menu_windows

namespace {

using move_script::SetWord;

std::uint32_t Addr(const void* p) { return Low32(p); }

unsigned char* Ptr(std::uint32_t at) { return menu_windows::At(static_cast<std::uint32_t>(move_script::Long(menu_windows::At(at)))); }

unsigned char* Next() { return Gfx_PacketNext; }

char* PrintBufMenuWindows() { return reinterpret_cast<char*>(menu_windows::At(::bof3::Img(menu_windows::at::kPrintBuf.v))); }

const char* FormatMenuWindows(std::uint32_t at) { return reinterpret_cast<const char*>(menu_windows::At(at)); }

std::int16_t S16MenuWindows(std::uint32_t v) { return static_cast<std::int16_t>(v); }

std::int32_t U16(std::uint32_t v) { return static_cast<std::int32_t>(v & 0xFFFF); }

// `fild dword` then `fstp dword`: an int rounded once to a float.
float F(std::int32_t v) { return static_cast<float>(v); }

// `fld dword`, `fadd` / `fsub` of a float constant, `fstp dword`: exact in
// the 53-bit precision the game runs x87 at, then rounded once.
float Add(float a, double b) { return static_cast<float>(static_cast<double>(a) + b); }

void Put(unsigned char* at, float f) { std::memcpy(at, &f, sizeof f); }

// The 8-byte RECT some draws put in the packet pool for a draw mode's texture
// window: Gfx_PacketNext moves past it, no commit.
unsigned char* PoolRect(unsigned x, unsigned y, unsigned w, unsigned h) {
    unsigned char* const r = Next();
    Gfx_PacketNext = r + 8;
    SetWord(r, x);
    SetWord(r + 2, y);
    SetWord(r + 4, w);
    SetWord(r + 6, h);
    return r;
}

// The draw mode with the page GetTPage(0, 0, page_x, 0x100) that the piece
// draws open with.
void PiecePage(int page_x) {
    const unsigned tpage = menu_windows::g.get_tpage(0, 0, page_x, 0x100);
    menu_windows::g.draw_mode(Next(), 0, 0, tpage & 0xFFFF, 0);
    menu_windows::g.commit(1, 0xC);
}

}  // namespace

// original 0x573560 (PSX 0x801D8270, call-anchored): a party
// member's status entry in the menus' lists (menu_frame.cpp: the PSX
// 0x801EA99C's entry body). Its box Menu_DrawBox(x + 3, y + 3, 0x7D, 0x30,
// highlight, the window colour); the record CharacterRecords + (member & 0xFF) *
// 0xA4; its portrait Menu_DrawItemIcon(x + 0x56, y + 2, byte +9, shade 1 when
// highlighted, else 2 when +0x10 bit 7, else 0); the name Text_DrawAt(x + 0x14,
// y + 3, colour, 5, record); the level "%3d" of byte +0xA at (x + 0x16, y + 0x17)
// in the 8 px font; a status word through 0x516E70 at (x + 0x2E, y + 0x17) -
// 0x66A0E8 with +0x10 bit 7, 0x66A0F0 with bit 5, alternating on Frame_Counter
// bit 5 with both; HP "%3d/   " and "   /%3d" (+0x18, +0x20) at y + 0x1F and AP
// (+0x1A, +0x22) at y + 0x27; the frame pieces 0x6632C8; and the EXP bar
// Menu_DrawExpBar(x + 0x14, y + 0x12, member, level, dword +0xC). +0x10 bit 13
// is set when HP is below a quarter of the maximum, cleared otherwise.
// Colours: 7 for everything when highlighted; otherwise 0, the HP 4 below a
// quarter (by the bit just stored) and 2 at 1 or less, the maximum HP 4 when
// byte +0x1E is set, the AP 4 below a quarter and 2 at 0, the maximum AP 0,
// and the status word 1.
// As the original has it: the colours go out in a dword whose upper bytes are
// stale stack (Text_DrawString and Text_DrawFont8 read the low byte, 6 bits);
// the window colour with the entry's ecx above it (Menu_DrawBox reads the
// byte); the level to Menu_DrawExpBar with Menu_DrawPieces' eax above it, which
// ours hands on as it is. Every field is read after the call before it.
extern "C" unsigned long __cdecl Menu_DrawMemberStatus(int x, int y, unsigned member, unsigned highlight) {
    const bool lit = (highlight & 0xFFu) != 0;
    const U bl = lit ? 7u : 0u;
    U colour = bl;
    map_field_objects::g.draw_box(x + 3, y + 3, 0x7D, 0x30, static_cast<int>(highlight), static_cast<unsigned char>(Config_WindowColour));
    unsigned char* const record = AtMapFieldObjects(::bof3::Img(CharacterRecords_addr) + (member & 0xFFu) * kCharStride);
    const U shade = lit ? 1u : ((record[0x10] >> 6) & 2u);
    map_field_objects::g.item_icon(x + 0x56, y + 2, static_cast<int>(record[9]), static_cast<int>(shade));
    map_field_objects::g.text_at(x + 0x14, y + 3, static_cast<int>(colour), 5, record);
    char* const buffer = reinterpret_cast<char*>(AtMapFieldObjects(::bof3::Img(kPrintBuf)));
    const auto* const text = reinterpret_cast<const unsigned char*>(buffer);
    map_field_objects::g.sprintf(buffer, reinterpret_cast<const char*>(AtMapFieldObjects(::bof3::Img(kFormat3d))), record[0xA]);
    map_field_objects::g.font8(x + 0x16, y + 0x17, static_cast<int>(colour), text);
    const U flags = static_cast<U>(rv::As<records::CharRecord>(record)->status);
    U status = 0;
    if (flags & 0x80u) {
        if (flags & 0x20u) status = (Byte(map_field_objects::at::FrameCounterAt()) & 0x20u) ? ::bof3::Img(kStatusText0) : ::bof3::Img(kStatusText1);
        else status = ::bof3::Img(kStatusText0);
    } else if (flags & 0x20u) {
        status = ::bof3::Img(kStatusText1);
    }
    if (status != 0) map_field_objects::g.tiny_font(x + 0x2E, y + 0x17, colour != 0 ? colour : 1u, 0x10, AtMapFieldObjects(status));
    const U hp = static_cast<U>(rv::As<records::CharRecord>(record)->hp);
    if (hp < (static_cast<U>(rv::As<records::CharRecord>(record)->stats.max_hp) >> 2)) rv::As<records::CharRecord>(record)->status = static_cast<U>(rv::As<records::CharRecord>(record)->status) | 0x2000u;
    else rv::As<records::CharRecord>(record)->status = static_cast<U>(rv::As<records::CharRecord>(record)->status) & 0xDFFFu;
    if (!lit) {
        if (static_cast<U>(rv::As<records::CharRecord>(record)->status) & 0x2000u) colour = 4;
        if (hp <= 1) colour = 2;
    } else {
        colour = 7;
    }
    map_field_objects::g.sprintf(buffer, reinterpret_cast<const char*>(AtMapFieldObjects(::bof3::Img(kFormatLeft))), hp);
    map_field_objects::g.font8(x + 0x16, y + 0x1F, static_cast<int>(colour), text);
    colour = lit ? 7u : (record[0x1E] != 0 ? 4u : 0u);
    map_field_objects::g.sprintf(buffer, reinterpret_cast<const char*>(AtMapFieldObjects(::bof3::Img(kFormatRight))), static_cast<U>(rv::As<records::CharRecord>(record)->stats.max_hp));
    map_field_objects::g.font8(x + 0x16, y + 0x1F, static_cast<int>(colour), text);
    colour = bl;
    if (!lit) {
        const U ap = static_cast<U>(rv::As<records::CharRecord>(record)->ap);
        if (ap < (static_cast<U>(rv::As<records::CharRecord>(record)->stats.max_ap) >> 2)) colour = 4;
        if (ap == 0) colour = 2;
    } else {
        colour = 7;
    }
    map_field_objects::g.sprintf(buffer, reinterpret_cast<const char*>(AtMapFieldObjects(::bof3::Img(kFormatLeft))), static_cast<U>(rv::As<records::CharRecord>(record)->ap));
    map_field_objects::g.font8(x + 0x16, y + 0x27, static_cast<int>(colour), text);
    map_field_objects::g.sprintf(buffer, reinterpret_cast<const char*>(AtMapFieldObjects(::bof3::Img(kFormatRight))), static_cast<U>(rv::As<records::CharRecord>(record)->stats.max_ap));
    map_field_objects::g.font8(x + 0x16, y + 0x27, static_cast<int>(bl), text);
    const U eax = map_field_objects::g.pieces(x, y, AtMapFieldObjects(::bof3::Img(Menu_MemberPieces_addr)), 0);
    const U exp = static_cast<U>(rv::As<records::CharRecord>(record)->exp);
    const U level = (eax & 0xFFFFFF00u) | record[0xA];
    return map_field_objects::g.exp_bar(x + 0x14, y + 0x12, member, level, exp);
}

// original 0x5738A0 (PSX 0x801D86D8): a character record's four stats and its trait line (the
// field menu's panel list, 0x59A065; record the party slot's EffectState id).
// Menu_DrawBox at (x + 5, y + 3), 0x94 x 0x2D; the four labels of 0x66A0F8
// (8 bytes apart) at (x + 0xB | x + 0x53, y + 8 | y + 0x15) through
// Text_DrawAt, each value - the words +0x24, +0x26, +0x2A, +0x28 - through
// Crt_sprintf and Text_DrawFont8 at x + 0x30 / x + 0x78; then, when the trait
// byte +0x1F is not 0xFF, system message 0x111 + trait into text record 0
// (8 long) and message 0x34 at (x + 0xB, y + 0x22); the frame pieces 0x663358,
// sixteen of 0x663384 eight apart, 0x663390.
// As the original has it: the record is the argument's byte, unchecked.
extern "C" void __cdecl Menu_DrawStatsPanel(int x, int y, unsigned record) {
    SH_CALL(Menu_DrawBox)(x + 5, y + 3, 0x94, 0x2D, 0, Style());
    const unsigned char* const rec = Record(record);
    const int left = x + 0xB;
    SH_CALL(Text_DrawAt)(left, y + 8, 0, 0xFF, Text(::bof3::Img(field_o::at::kStatLabels)));
    Sprintf(::bof3::Img(field_o::at::kFmtNumber), static_cast<unsigned>(rv::As<records::CharRecord>(rec)->stats.atk));
    SH_CALL(Text_DrawFont8)(x + 0x30, y + 0xA, 0, Buf());
    SH_CALL(Text_DrawAt)(left, y + 0x15, 0, 0xFF, Text(::bof3::Img(field_o::at::kStatLabels) + 8));
    Sprintf(::bof3::Img(field_o::at::kFmtNumber), static_cast<unsigned>(rv::As<records::CharRecord>(rec)->stats.def));
    SH_CALL(Text_DrawFont8)(x + 0x30, y + 0x17, 0, Buf());
    SH_CALL(Text_DrawAt)(x + 0x53, y + 8, 0, 0xFF, Text(::bof3::Img(field_o::at::kStatLabels) + 0x10));
    Sprintf(::bof3::Img(field_o::at::kFmtNumber), static_cast<unsigned>(rv::As<records::CharRecord>(rec)->stats.intel));
    SH_CALL(Text_DrawFont8)(x + 0x78, y + 0xA, 0, Buf());
    SH_CALL(Text_DrawAt)(x + 0x53, y + 0x15, 0, 0xFF, Text(::bof3::Img(field_o::at::kStatLabels) + 0x18));
    Sprintf(::bof3::Img(field_o::at::kFmtNumber), static_cast<unsigned>(rv::As<records::CharRecord>(rec)->stats.agi));
    SH_CALL(Text_DrawFont8)(x + 0x78, y + 0x17, 0, Buf());
    const unsigned char trait = rec[0x1F];
    if (trait != 0xFF) {
        SH_CALL(TextRecord_Set)(0, 8, SH_CALL(Msg_SystemPtr)(0x111u + trait));
        SH_CALL(Text_DrawAt)(left, y + 0x22, 0, 0xFF, SH_CALL(Msg_SystemPtr)(0x34));
    }
    SH_CALL(Menu_DrawPieces)(x, y, Text(::bof3::Img(field_o::at::kStatsPiecesA)), 0);
    for (unsigned b = 0; b < 0x10; ++b) SH_CALL(Menu_DrawPieces)(x + 8 * static_cast<int>(b), y, Text(::bof3::Img(field_o::at::kStatsPiecesB)), 0);
    SH_CALL(Menu_DrawPieces)(x, y, Text(::bof3::Img(field_o::at::kStatsPiecesC)), 0);
}

// original 0x573A80 (PSX 0x801D8A10 and three more overlays' copies): a
// member's equipment - the panel (x, y, 9, 0xA), "EQUIP" at (x + 0x26, y + 7),
// the stat icon 0x5903F0(0, x + 7, y + 5, 0x10, 0x10, 0x80), then the six
// slots, 13 apart: the slot icon of 0x663660 and the name of record bytes
// +0x12 (a weapon), +0x13..+0x15 (armour), +0x16, +0x17 (accessories). The
// six names are looked up once, after the stat icon.
extern "C" void __cdecl Menu_DrawEquipPanel(int x, int y, unsigned member) {
    menu_windows::g.panel(x, y, 9, 0xA);
    const unsigned char* const record = At(::bof3::Img(CharacterRecords_addr) + (member & 0xFF) * menu_windows::at::kCharStride);
    menu_windows::g.text_draw_at(x + 0x26, y + 7, 0, 0xFF, At(::bof3::Img(menu_windows::at::kEquipLabel.v)));
    menu_windows::g.stat_icon(0, x + 7, y + 5, 0x10, 0x10, 0x80);
    const unsigned char* const names[6] = {
        At(::bof3::Img(0x657450) + record[0x12] * 28u), At(::bof3::Img(0x657D68) + record[0x13] * 26u), At(::bof3::Img(0x657D68) + record[0x14] * 26u),
        At(::bof3::Img(0x657D68) + record[0x15] * 26u), At(::bof3::Img(0x658450) + record[0x16] * 24u), At(::bof3::Img(0x658450) + record[0x17] * 24u),
    };
    for (unsigned i = 0; i < 6; ++i) {
        menu_windows::g.icon8(x + 5, y + static_cast<int>(13 * i) + 0x1C, At(::bof3::Img(0x663660))[i], 0);
        const unsigned n = menu_windows::g.char_count(names[i]);
        menu_windows::g.text_draw_at(x + 0x11, y + static_cast<int>(13 * i) + 0x1A, 0, static_cast<int>(n), names[i]);
    }
}

// original 0x573BF0: a record's EXP (next 0) or the EXP its next level wants
// (next not 0: Char_ExpForLevel(record, level + 1), -1 past 99 drawn as the
// text at 0x6639B8). Box (x + 3, y + 3) 0x65 x 0x25; the label 0x663670 at
// x + 0x24, or 0x663674 at x + 0x1E; the value through Text_DrawFont12 at
// (x + 5, y + 0x1A); the pieces 0x6634CC. Callers 0x59A237 (next 0) and
// 0x59A2A7 (next 1), the field menu's panel list.
extern "C" void __cdecl Menu_DrawExpPanel(int x, int y, unsigned record, unsigned next) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x65, 0x25, 0, Style());
    const unsigned char* const rec = Record(record);
    const bool want = static_cast<unsigned char>(next) != 0;
    SH_CALL(Text_DrawAt)(x + 0x1E + (want ? 0 : 6), y + 7, 0, 0xFF, Text(want ? ::bof3::Img(field_o::at::kExpLabel) + 4 : ::bof3::Img(field_o::at::kExpLabel)));
    const std::int32_t v = want ? SH_CALL(Char_ExpForLevel)(record, static_cast<unsigned char>(rec[0xA] + 1))
                                : rv::As<records::CharRecord>(rec)->exp;
    if (v == -1) Sprintf(::bof3::Img(field_o::at::kFmtExpNone));
    else Sprintf(::bof3::Img(field_o::at::kFmtExp), v);
    SH_CALL(Text_DrawFont12)(x + 5, y + 0x1A, 0, Buf());
    SH_CALL(Menu_DrawPieces)(x, y, Text(::bof3::Img(field_o::at::kExpPieces)), 1);
}

// original 0x573CE0 (PSX 0x801D8E08): a cursor box - a LINE_F3 (0x5A7670) from
// (x + 4, y) down the left, (x, y + 4) to (x, y + h - 1), then a LINE_F4 round
// the rest, (x + 4, y) (x + w - 1, y) (x + w - 1, y + h - 1) (x, y + h - 1), x y
// w h as 16-bit. Its colour: shade in the channels flags bits 2, 1, 0 name
// (red, green, blue), the rest 0; shade 0xFF, or blinking with Frame_Counter
// bits 1..3 when blink's low byte is set.
extern "C" void __cdecl Menu_DrawCursorBox(int x, int y, int w, int h, int blink, int flags) {
    unsigned char shade;
    if (blink & 0xFF) {
        const unsigned char fc = At(::bof3::Img(Frame_Counter_addr))[0];
        shade = (fc & 8) ? static_cast<unsigned char>(((fc & 6) << 5) + 0x3F)
                         : static_cast<unsigned char>(((~fc & 6) << 5) + 0x3F);
    } else {
        shade = 0xFF;
    }
    const auto f = static_cast<unsigned char>(flags);
    const unsigned char c0 = (f & 1) ? shade : 0, c1 = (f & 2) ? shade : 0, c2 = (f & 4) ? shade : 0;
    unsigned char* p = Next();
    menu_windows::g.set_line_f3(p);
    const std::int32_t xs = U16(static_cast<std::uint32_t>(x)), ys = U16(static_cast<std::uint32_t>(y));
    const std::int32_t hs = U16(static_cast<std::uint32_t>(h));
    const float fx4 = F(xs + 4), fy = F(ys), fx = F(xs), fyh = F(hs + ys - 1);
    p[4] = c2;
    Put(p + 8, fx4);
    Put(p + 0xC, fy);
    p[5] = c1;
    p[6] = c0;
    Put(p + 0x14, fx);
    Put(p + 0x18, F(ys + 4));
    Put(p + 0x20, fx);
    Put(p + 0x24, fyh);
    menu_windows::g.commit(1, 0x2C);
    p = Next();
    menu_windows::g.set_line_f4(p);
    const float fxw = F(U16(static_cast<std::uint32_t>(w)) + xs - 1);
    Put(p + 8, fx4);
    Put(p + 0x2C, fx);
    Put(p + 0x30, fyh);
    Put(p + 0xC, fy);
    Put(p + 0x18, fy);
    Put(p + 0x24, fyh);
    p[4] = c2;
    p[5] = c1;
    p[6] = c0;
    Put(p + 0x20, fxw);
    Put(p + 0x14, fxw);
    menu_windows::g.commit(1, 0x38);
}

// original 0x573E50 (PSX 0x801D8F9C): an icon of 0x28 x 0x30 - rectangle icon
// of 0x66367C: (u, v, clut-x byte, clut-y byte), the CLUT word
// ((b3 + 0x1E0) << 6) | (b2 >> 4) - after a draw mode of page 0x1E with dtd;
// shade 0x80 for 0, 0x30 for 1, else (0x40, 0x40, 0x80).
extern "C" void __cdecl Menu_DrawBigIcon(int x, int y, int icon, int shade) {
    const unsigned char* const r = At(::bof3::Img(menu_windows::at::kIconRects) + (static_cast<unsigned>(icon) & 0xFF) * 4);
    menu_windows::g.draw_mode(Next(), 0, 1, 0x1E, 0);
    menu_windows::g.commit(1, 0xC);
    unsigned char* const p = Next();
    menu_windows::g.set_sprt(p);
    const auto s = static_cast<unsigned char>(shade);
    if (s == 0) {
        p[6] = p[5] = p[4] = 0x80;
    } else if (s == 1) {
        p[6] = p[5] = p[4] = 0x30;
    } else {
        p[6] = 0x80;
        p[5] = p[4] = 0x40;
    }
    Put(p + 8, F(U16(static_cast<std::uint32_t>(x))));
    Put(p + 0xC, F(U16(static_cast<std::uint32_t>(y))));
    p[0x14] = r[0];
    p[0x15] = r[1];
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, 0x28);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, 0x30);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, ((r[3] + 0x1E0u) << 6) | static_cast<unsigned>(r[2] >> 4));
    menu_windows::g.commit(1, 0x1C);
}

// original 0x573F30 (PSX 0x801D90E8): Menu_DrawBigIcon, with icon 4 drawn as
// icon 0xB once the s8 at 0x8034E0 (Cond_ByteFA) is 8 or more.
extern "C" void __cdecl Menu_DrawItemIcon(int x, int y, int icon, int shade) {
    if ((icon & 0xFF) == 4 && Cond_ByteFA >= 8)
        icon = static_cast<int>((static_cast<unsigned>(icon) & ~0xFFu) | 0xB);
    menu_windows::g.icon(x, y, icon, shade);
}

// DIVERGENCE DIV-0084: the icon wheel's formation name centred on x + 0x27,
// as the US release draws it (its small-text draw takes the middle), where
// the port starts its four-glyph names at x + 0x16. 0 until FieldO_Inject
// patches it to 1 under a Latin overlay, after the fuzz, under the name
// FormationNameCentre.
namespace field_o { unsigned char g_wheel_name_centre = 0; }

// original 0x573F70: a kind's name, a turned triangle and up to three icons
// (the field menu's panel list, 0x59A46E: the panel's +0xA kind, +0xC the lit
// bits, +0x10 the angle). Box (x + 3, y + 3) 0x45 x 0x30; the name, 28-byte
// entries at 0x6636B0, through Text_DrawSmall at (x + 0x16, y + 0x28); the
// three corners of 0x6637C8 turned by the angle into 0x6BC860.. and drawn as a
// POLY_G3 (0x34 bytes) at (x & 0xFFFF) + x' + 0x26, (y & 0xFFFF) + z' + 0x17,
// each corner shaded by its z; then 1 (kind 0), 2 (kinds 1..3) or 3 icons
// whose spots (the kind's s16 pairs at 0x6636C0) are turned likewise into
// 0x6BC748.., lit (bit j of the lit byte: a Frame_Counter pulse) or 0x70,
// sorted by z (a bubble sort), and drawn with Menu_DrawCell8 (u = the icon's
// index + 3, v 0x1E, CLUT (0x10, 0x1E0), the shade); the frame pieces 0x6633E0,
// six of 0x663404 eight apart, 0x663410.
// As the original has it: the sort's swap moves each coordinate through a
// byte register - the one moved down keeps only its low byte, sign-extended
// (harmless while a turned coordinate fits a signed byte); the kind indexes
// its two 28-byte tables unchecked.
extern "C" void __cdecl Menu_DrawIconWheel(int x, int y, unsigned kind, unsigned lit, int angle) {
    SH_CALL(Menu_DrawBox)(x + 3, y + 3, 0x45, 0x30, 0, Style());
    unsigned char order[3] = {0, 1, 2};
    const unsigned k = kind & 0xFF;
    // DIVERGENCE DIV-0084: a one-byte-a-letter name (DIV-0064's group 10)
    // centred on x + 0x27, 8 a letter; the port's x + 0x16 otherwise.
    const unsigned char* const name = Text(::bof3::Img(field_o::at::kWheelNames) + k * 28);
    int name_x = x + 0x16;
    if (field_o::g_wheel_name_centre && Labels_SmallWritten(10))
        name_x = x + 0x27 - 4 * static_cast<int>(std::strlen(reinterpret_cast<const char*>(name)));
    SH_CALL(Text_DrawSmall)(name_x, y + 0x28, 0, 0xFF, name);
    const unsigned char kb = static_cast<unsigned char>(kind);
    const unsigned char count = kb == 0 ? 1 : kb <= 3 ? 2 : 3;
    for (unsigned i = 0; i < 3; ++i) {
        std::int32_t ox, oz;
        Turn(angle, Long(AtFieldO(::bof3::Img(field_o::at::kWheelTriangle) + 8 * i)), Long(AtFieldO(::bof3::Img(field_o::at::kWheelTriangle) + 8 * i + 4)), ox, oz);
        SetLong(AtFieldO(::bof3::Img(field_o::at::kWheelFrame) + 8 * i), ox);
        SetLong(AtFieldO(::bof3::Img(field_o::at::kWheelFrame) + 8 * i + 4), oz);
    }
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG3)(prim);
    const std::int32_t bx = static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFF);
    const std::int32_t by = static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFF);
    const unsigned char* const f = AtFieldO(::bof3::Img(field_o::at::kWheelFrame));
    PutFloat(prim + 8, bx + Long(f) + 0x26);
    PutFloat(prim + 0xC, by + Long(f + 4) + 0x17);
    PutFloat(prim + 0x18, bx + Long(f + 8) + 0x26);
    PutFloat(prim + 0x1C, by + Long(f + 0xC) + 0x17);
    PutFloat(prim + 0x28, bx + Long(f + 0x10) + 0x26);
    PutFloat(prim + 0x2C, by + Long(f + 0x14) + 0x17);
    prim[6] = 0;
    prim[5] = prim[4] = WheelShade(Long(f + 4));
    prim[0x16] = 0;
    prim[0x15] = prim[0x14] = WheelShade(Long(f + 0xC));
    prim[0x26] = 0;
    prim[0x25] = prim[0x24] = WheelShade(Long(f + 0x14));
    SH_CALL(Gfx_CommitPrim)(1, 0x34);
    unsigned char colour[3] = {};
    unsigned char mask = 1;
    for (unsigned j = 0; j < count; ++j, mask = static_cast<unsigned char>(mask << 1)) {
        if (static_cast<unsigned char>(lit) & mask) {
            const auto fc = static_cast<unsigned char>(Frame_Counter);
            colour[j] = static_cast<unsigned char>(((fc & 8 ? fc : ~fc) & 6) << 5) + 0x3F;
        } else {
            colour[j] = 0x70;
        }
        const unsigned char* const spot = AtFieldO(::bof3::Img(field_o::at::kWheelIcons) + k * 28 + 4 * j);
        std::int32_t ox, oz;
        Turn(angle, S16(Word(spot)), S16(Word(spot + 2)), ox, oz);
        SetLong(AtFieldO(::bof3::Img(field_o::at::kWheelSpots) + 8 * j), ox);
        SetLong(AtFieldO(::bof3::Img(field_o::at::kWheelSpots) + 8 * j + 4), oz);
    }
    for (unsigned i = 0; static_cast<int>(i) < count - 1; ++i) {
        for (unsigned j = 0; static_cast<int>(j) < count - static_cast<int>(i) - 1; ++j) {
            unsigned char* const a = AtFieldO(::bof3::Img(field_o::at::kWheelSpots) + 8 * j);
            if (Long(a + 4) <= Long(a + 0xC)) continue;
            const std::int32_t x0 = Long(a), z0 = Long(a + 4);
            SetLong(a, Long(a + 8));
            SetLong(a + 8, Sb(static_cast<U>(x0)));
            SetLong(a + 4, Long(a + 0xC));
            SetLong(a + 0xC, Sb(static_cast<U>(z0)));
            const unsigned char o = order[j];
            order[j] = order[j + 1];
            order[j + 1] = o;
            const unsigned char c = colour[j];
            colour[j] = colour[j + 1];
            colour[j + 1] = c;
        }
    }
    for (unsigned j = 0; j < count; ++j) {
        const unsigned clut = SH_CALL(Gpu_GetClut)(0x10, 0x1E0);
        const unsigned char* const spot = AtFieldO(::bof3::Img(field_o::at::kWheelSpots) + 8 * j);
        const auto sx = static_cast<unsigned>(static_cast<std::uint16_t>(Word(spot) + static_cast<unsigned>(x)) + 0x23u);
        const auto sy = static_cast<unsigned>(static_cast<std::uint16_t>(Word(spot + 4) + static_cast<unsigned>(y)) + 0xFu);
        SH_CALL(Menu_DrawCell8)(sx, sy, static_cast<unsigned char>(order[j] + 3), 0x1E, clut, colour[j]);
    }
    SH_CALL(Menu_DrawPieces)(x, y, Text(::bof3::Img(Menu_PlayTimePieces_addr) + 0x2c), 0);
    for (unsigned b = 0; b < 6; ++b) SH_CALL(Menu_DrawPieces)(x + 8 * static_cast<int>(b), y, Text(::bof3::Img(field_o::at::kWheelPiecesB)), 0);
    SH_CALL(Menu_DrawPieces)(x, y, Text(::bof3::Img(field_o::at::kWheelPiecesC)), 0);
}

// original 0x574400 (PSX 0x801D9FC0): one 16 x 8 SPRT of the menu page (CLUT 0x7800, v 0xD8, u
// the argument << 4), grey 0x80 or dimmed to 0x10, semi-transparent, after a
// draw mode (texture page 0x2F). Callers: the shop's 0x581300 (FS, three
// sites) and the field menu's 0x59AA80 (three).
extern "C" void __cdecl Menu_DrawTile16(int x, int y, unsigned u, unsigned dim) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x2F, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    rv::As<records::GpuSprt>(p)->w = 0x10;
    rv::As<records::GpuSprt>(p)->h = 8;
    rv::As<records::GpuSprt>(p)->clut = 0x7800;
    const unsigned char shade = static_cast<unsigned char>(dim) ? 0x10 : 0x80;
    p[6] = p[5] = p[4] = shade;
    PutFloat(p + 8, static_cast<std::int32_t>(static_cast<U>(x) & 0xFFFF));
    p[0x15] = 0xD8;
    PutFloat(p + 0xC, static_cast<std::int32_t>(static_cast<U>(y) & 0xFFFF));
    p[0x14] = static_cast<unsigned char>(u << 4);
    SH_CALL(Gpu_SetSprt)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x5744B0 (PSX 0x801DA0D4, gap pair): one 8 x 8 cell - a SPRT_8 at
// Gfx_PacketNext through Gpu_SetSprt8, at (x, y) (their low words, unsigned,
// as floats), u = u * 8 and v = v * 8 (bytes), CLUT word `clut`, colour the
// byte `shade` in all three channels; Gpu_SetSemiTrans(0) and
// Gfx_CommitPrim(1, 0x18), whose eax is returned. 8 E8 callers (0x461DB1,
// 0x461DD7, 0x461DFD, 0x57437E, 0x5750B7, 0x599C17 among them).
extern "C" unsigned long __cdecl Menu_DrawCell8(unsigned x, unsigned y, unsigned u, unsigned v, unsigned clut,
                                                unsigned shade) {
    unsigned char* const prim = PacketNext();
    map_field_objects::g.set_sprt8(prim);
    SetFloatMapFieldObjects(prim + 8, static_cast<float>(static_cast<std::int32_t>(x & 0xFFFFu)));
    prim[0x15] = static_cast<unsigned char>(v << 3);
    SetFloatMapFieldObjects(prim + 0xC, static_cast<float>(static_cast<std::int32_t>(y & 0xFFFFu)));
    prim[0x14] = static_cast<unsigned char>(u << 3);
    rv::As<records::GpuSprt>(prim)->clut = clut;
    prim[6] = prim[5] = prim[4] = static_cast<unsigned char>(shade);
    map_field_objects::g.set_semi(prim, 0);
    return map_field_objects::g.commit(1, 0x18);
}

// original 0x574530 (PSX 0x801DA18C): the experience bar - an opaque LINE_F2
// in red (0x80, 0, 0) from (x, y) to (x + 57 (exp - need) / (next - need), y),
// need and next Char_ExpForLevel of level and level + 1 (a byte). All 32-bit
// unsigned, as the original has it: the product wraps and the division is
// unsigned. With next equal to need the line has no length. x and y 16-bit.
extern "C" void __cdecl Menu_DrawExpBar(int x, int y, unsigned member, unsigned level, unsigned exp) {
    unsigned char* const p = Next();
    menu_windows::g.set_line_f2(p);
    menu_windows::g.set_semi(p, 0);
    const std::int32_t xs = U16(static_cast<std::uint32_t>(x));
    const float fx = F(xs), fy = F(U16(static_cast<std::uint32_t>(y)));
    Put(p + 8, fx);
    Put(p + 0xC, fy);
    const auto need = static_cast<std::uint32_t>(menu_windows::g.exp_for_level(member, level));
    const auto next = static_cast<std::uint32_t>(
        menu_windows::g.exp_for_level(member, (level & ~0xFFu) | ((level + 1) & 0xFFu)));
    const std::uint32_t span = next - need;
    if (span != 0) {
        const std::uint32_t end = (57u * exp - 57u * need) / span + static_cast<std::uint32_t>(xs);
        Put(p + 0x14, static_cast<float>(static_cast<double>(end)));   // fild qword of (0, end)
    } else {
        Put(p + 0x14, fx);
    }
    Put(p + 0x18, fy);
    p[4] = 0x80;
    p[5] = 0;
    p[6] = 0;
    menu_windows::g.commit(1, 0x20);
}

// original 0x574610 (PSX 0x801DA2A8): the money box - the box (x + 3, y + 3,
// 0x65, 0x10), value in "%7d" in the 12 x 12 font at (x + 4, y + 4), the
// label 0x66A31C at (x + 0x58, y + 4), then three piece lists across it. The
// third argument is not read (every caller passes 0).
extern "C" void __cdecl Menu_DrawMoneyBox(int x, int y, int, unsigned value) {
    menu_windows::g.box(x + 3, y + 3, 0x65, 0x10, 0, static_cast<unsigned char>(Config_WindowColour));
    menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmt7d.v)), value);
    menu_windows::g.font12(x + 4, y + 4, 0, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    menu_windows::g.text_draw_at(x + 0x58, y + 4, 0, 0xFF, At(::bof3::Img(menu_windows::at::kMoneyLabel.v)));
    menu_windows::g.pieces(x, y, At(::bof3::Img(0x6633A8)), 0);
    for (unsigned i = 0; i < 11; ++i) menu_windows::g.pieces(x + static_cast<int>(i * 8), y, At(::bof3::Img(Menu_PlayTimePieces_addr)), 0);
    menu_windows::g.pieces(x, y, At(::bof3::Img(Menu_PlayTimePieces_addr) + 0xc), 0);
}

// original 0x5746C0: the play-time box - Menu_DrawBox(x + 3, y + 3, 0x55, 0x10,
// 0, the window colour); the hours and minutes (0x9040C8, 0x9040C9) "%02d" in
// the 12 px font at (x + 0x1C, y + 4) and (x + 0x3C, y + 4); the colon, two
// dots through 0x516E70 at (x + 0x34, y + 5) and (x + 0x34, y + 9), for the
// first 15 of the clock's 30 frames; then Menu_DrawPieces of 0x6633CC at (x, y),
// 0x6633B4 six times, and 0x6633C0 at x - 0x10, whose eax is returned.
// As the original has it: the six middle pieces' x is x + 0x18 + 8 * i with i
// in a register whose upper half is the last Menu_DrawPieces' eax (xor ax, ax;
// mov al, i) - ours takes the callee's eax as the original does, so i * 8 is
// off by eax's high word << 19, which Menu_DrawPieces' 16-bit piece draws do
// not see.
extern "C" unsigned long __cdecl Menu_DrawPlayTime(int x, int y) {
    map_field_objects::g.draw_box(x + 3, y + 3, 0x55, 0x10, 0, static_cast<unsigned char>(Config_WindowColour));
    char* const buffer = reinterpret_cast<char*>(AtMapFieldObjects(::bof3::Img(kPrintBuf)));
    const auto* const text = reinterpret_cast<const unsigned char*>(buffer);
    map_field_objects::g.sprintf(buffer, reinterpret_cast<const char*>(AtMapFieldObjects(::bof3::Img(kFormat02d))), Byte(::bof3::Img(kClockHours)));
    map_field_objects::g.font12(x + 0x1C, y + 4, 0, text);
    map_field_objects::g.sprintf(buffer, reinterpret_cast<const char*>(AtMapFieldObjects(::bof3::Img(kFormat02d))), static_cast<std::uint32_t>(MapFieldObjects_ClockMinutes));
    map_field_objects::g.font12(x + 0x3C, y + 4, 0, text);
    if (static_cast<std::uint32_t>(MapFieldObjects_ClockFrames) < 0xF) {
        map_field_objects::g.tiny_font(x + 0x34, y + 5, 0, 0xFF, AtMapFieldObjects(::bof3::Img(kColonText)));
        map_field_objects::g.tiny_font(x + 0x34, y + 9, 0, 0xFF, AtMapFieldObjects(::bof3::Img(kColonText)));
    }
    U eax = map_field_objects::g.pieces(x, y, AtMapFieldObjects(::bof3::Img(Menu_PlayTimePieces_addr) + 0x18), 0);
    for (U i = 0; i < 6; ++i) {
        const U index = (eax & 0xFFFF0000u) | i;
        eax = map_field_objects::g.pieces(static_cast<int>(static_cast<U>(x) + index * 8u + 0x18u), y, AtMapFieldObjects(::bof3::Img(Menu_PlayTimePieces_addr)), 0);
    }
    return map_field_objects::g.pieces(x - 0x10, y, AtMapFieldObjects(::bof3::Img(Menu_PlayTimePieces_addr) + 0xc), 0);
}

namespace menu_windows {
bool g_yes_no_div = false;
}  // namespace menu_windows

// original 0x5747D0 (PSX 0x801DA628): the Yes / No chooser (DIV-0027,
// docs/glyph-draw.md section 7). System message 0xF at (0x1C, 0x16), the hand
// at x 0xFE - 36 * s8 selection (0x929F0B), y 0x18. Then on this frame's
// Input_Pressed: a cancel button - sound 0x106, selection 0, answer 1; a
// confirm button - sound 0x104 on Yes, 0x106 on No, answer 1; bit 13 or 15
// (left, right) - sound 0x101, the selection's bit 0 flipped; else 0.
//
// DIV-0027 (under a language overlay, BOF3X_ORIGINAL=YesNoLayout): the line
// through YesNo_Line and the hand at 0x112 - 56 * selection, as the patched
// original has them.
extern "C" unsigned char __cdecl Menu_YesNo() {
    const unsigned char* const line = menu_windows::g.yes_no_line(0xF);
    menu_windows::g.text_draw_at(0x1C, 0x16, 0, 0xFF, line);
    const auto sel = static_cast<std::uint32_t>(FieldS_Answer);
    const std::uint32_t hand = g_yes_no_div ? 0x112u - 56u * sel : 0xFEu - 36u * sel;
    menu_windows::g.draw_hand(static_cast<int>(hand), 0x18, 0);
    const auto pressed = static_cast<std::uint32_t>(Long(At(::bof3::Img(Input_Pressed_addr))));
    if (Field_CancelButtons & pressed) {
        menu_windows::g.sound(0x106);
        FieldS_Answer = 0;
        return 1;
    }
    if (Field_ConfirmButtons & pressed) {
        if (static_cast<unsigned char>(FieldS_Answer)) {
            menu_windows::g.sound(0x104);
            return 1;
        }
        menu_windows::g.sound(0x106);
        return 1;
    }
    if ((pressed >> 8) & 0xA0) {
        menu_windows::g.sound(0x101);
        FieldS_Answer = static_cast<unsigned char>(static_cast<unsigned char>(FieldS_Answer) ^ 1);
    }
    return 0;
}

// original 0x574890 (PSX 0x801DA738 and three more overlays' copies): the
// buttons above a menu panel (DIV-0018, docs/config-screen.md section 8). For
// each of Menu_ButtonSets[set].count: the box (x + 48 i, y, 0x2D, 0x14) in the
// window colour; the verb through Text_DrawAt, centred as
// x + 0x16 + 48 i - 6 n (n from Text_CharCount), count 0x10, colour 7 -
// greyed - for every button but the selected one when the selection is below
// the count, for verb 0x12 unless bit 0 of 0x905BA2, and for verb 8 when key
// item 0xF is not held and no item 0x58 is; then the pieces 0x663440 on the
// selected button, 0x663428 on the rest. The count is read again after each
// button; the fifth argument is not read.
//
// DIV-0018 (under a language overlay, BOF3X_ORIGINAL=MenuVerbs): the label
// through MenuVerbs_DrawLabel, as the patched original has it.
extern "C" void __cdecl Menu_DrawButtonRow(int x, int y, int set, int sel, int) {
    const unsigned s5 = (static_cast<unsigned>(set) & 0xFF) * 5;
    const unsigned char* const sets = At(::bof3::Img(Menu_ButtonSets_addr));
    if (sets[s5] == 0) return;
    const auto selected = static_cast<unsigned char>(sel);
    unsigned char i = 0;
    do {
        const int bx = x + 48 * i;
        menu_windows::g.box(bx, y, 0x2D, 0x14, 0, static_cast<unsigned char>(Config_WindowColour));
        unsigned char colour;
        if (selected >= sets[s5]) colour = 0;
        else colour = (i == selected) ? 0 : 7;
        const unsigned char verb = sets[s5 + 1 + i];
        // DIV-0217 AllyAnywhere. The Definitive Edition romhack (evidence: the
        // patched US disc, START.EMI 8 = STATUS.EMI 0's copy of this function,
        // 0x801DDC74, its grey store at 0x801DDD80 made a nop) no longer greys
        // verb 0x12, the member change, in the field menu's copy only; the
        // other overlays' copies keep it. The PC has one function for all of
        // them: the field menu's draws are told apart by Game_Mode 3, the
        // menu (GameMode_Field's Field_Request 1), the mode the START overlay
        // runs in. Verb 0x12 is in one set only, 7, Tactics' row.
        if (verb == 0x12 && !(Field_InputFlags & 1) &&
            !(bof3::BehaviourOn(bof3::behaviours::Id::AllyAnywhere) && Game_Mode == 3))
            colour = 7;
        if (verb == 8) {
            if ((menu_windows::g.has_key_item(0xF) & 0xFF) == 0) {
                if ((menu_windows::g.count_owned(0, 0x58, 0) & 0xFFFF) == 0) colour = 7;
            }
        }
        const unsigned char* const text = Ptr(::bof3::Img(menu_windows::at::kVerbPointers.v) + verb * 4u);
        const unsigned n = menu_windows::g.char_count(text);
        menu_windows::g.verb_label(x + 0x16 + 6 * (8 * i - static_cast<int>(n)), y + 3, colour, 0x10, text);
        menu_windows::g.pieces(bx, y, At(::bof3::Img(i == selected ? 0x663440 : 0x663428)), 0);
        ++i;
    } while (i < sets[s5]);
}

// original 0x5749F0 (PSX 0x801DA988): an item's price, the u16 of its record -
// weapons (category 1) 0x65746A + 28 id, armour (2) 0x657D80 + 26 id,
// accessories (3) 0x658466 + 24 id, any other category a consumable, 0x656B3C
// + 22 id - both arguments read as bytes, the answer zero-extended.
extern "C" unsigned __cdecl Item_BasePrice(unsigned category, unsigned id) {
    const unsigned i = id & 0xFF;
    switch (category & 0xFF) {
    case 1: return Word(At(::bof3::Img(0x65746A) + i * 28));
    case 2: return Word(At(::bof3::Img(0x657D80) + i * 26));
    case 3: return Word(At(::bof3::Img(0x658466) + i * 24));
    default: return Word(At(::bof3::Img(0x656B3C) + i * 22));
    }
}

// original 0x574A60 (PSX 0x801DAA30): the experience member needs for a level:
// the sum of the first `level` u16s of its table (0x658F48 + member * 0x318,
// stride 8); -1 for a level above 99. Both read as bytes.
extern "C" int __cdecl Char_ExpForLevel(unsigned member, unsigned level) {
    const auto n = static_cast<unsigned char>(level);
    if (n > 0x63) return -1;
    std::uint32_t sum = 0;
    const unsigned char* const table = At(::bof3::Img(menu_windows::at::kExpTable.v) + (member & 0xFF) * 0x318);
    for (unsigned k = 0; k < n; ++k) sum += Word(table + 8 * k);
    return static_cast<int>(sum);
}

// original 0x574AB0 (PSX 0x801DAB48): the screen title's box, the owner's
// "Ability has no box" lead (docs/menu-screens.md section 3): four
// semi-transparent POLY_FT4s of the 16 x 16 tile at (0, 0xF0) under its
// texture window - a left end 2 wide with slanted corners, two middles of
// (w + 1 - 4) / 2 each (the second a unit wider when w + 1 is odd), a right
// end 2 wide - shade 0xAC, CLUT GetClut(colour * 32 + 16, 0x1E1); then the
// window back to the whole page and Menu_DrawOutline(x + 2, y + 2, w - 4,
// h - 4, 0) inside it. The box spans w + 1 by h + 1.
extern "C" void __cdecl Menu_DrawTitleBox(int x, int y, int w, int h, int colour) {
    const unsigned char* const window = PoolRect(0, 0xF0, 0x10, 0x10);
    menu_windows::g.draw_mode(Next(), 0, 1, 0xF, Addr(window));
    menu_windows::g.commit(1, 0xC);
    const auto w1 = static_cast<std::uint32_t>(w) + 1, h1 = static_cast<std::uint32_t>(h) + 1;
    unsigned char* p = Next();
    menu_windows::g.set_poly_ft4(p);
    menu_windows::g.set_semi(p, 1);
    const std::int32_t xs = U16(static_cast<std::uint32_t>(x)), ys = U16(static_cast<std::uint32_t>(y));
    const std::int32_t hs = U16(h1);
    const auto hb = static_cast<unsigned char>(h1);
    const float fx = F(xs), fy = F(ys), fx2 = F(xs + 2), fyh = F(hs + ys);
    const float fy2 = Add(fy, 2.0), fyh3 = Add(fyh, -3.0);
    p[0x14] = 0;
    p[0x24] = 2;
    p[0x25] = 0;
    Put(p + 8, fx);
    p[0x34] = 0;
    Put(p + 0x1C, fy);
    Put(p + 0x18, fx2);
    p[0x44] = 2;
    Put(p + 0x28, fx);
    Put(p + 0x38, fx2);
    p[0x45] = hb;
    p[4] = p[5] = 0xAC;
    p[6] = 0xAC;
    Put(p + 0x3C, fyh);
    p[0x15] = 2;
    Put(p + 0xC, fy2);
    Put(p + 0x2C, fyh3);
    const auto hb3 = static_cast<unsigned char>(hb - 3);
    p[0x35] = hb3;
    const int clut_x = static_cast<int>((static_cast<unsigned>(colour) & 0xFF) << 5) + 0x10;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, menu_windows::g.get_clut(clut_x, 0x1E1));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, 0xF);
    menu_windows::g.commit(1, 0x48);

    const std::int32_t ws = U16(w1);
    const std::int32_t half = (ws - 4) >> 1;
    const std::int32_t parity = static_cast<std::int32_t>(w1 & 1);
    p = Next();
    menu_windows::g.set_poly_ft4(p);
    menu_windows::g.set_semi(p, 1);
    const std::int32_t half16 = S16MenuWindows(static_cast<std::uint32_t>(half));
    const float fmid = F(half16 + xs + 2);
    Put(p + 0xC, fy);
    Put(p + 8, fx2);
    Put(p + 0x1C, fy);
    Put(p + 0x28, fx2);
    Put(p + 0x18, fmid);
    Put(p + 0x38, fmid);
    Put(p + 0x2C, fyh);
    Put(p + 0x3C, fyh);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x24] = static_cast<unsigned char>(half);
    p[0x25] = 0;
    p[0x34] = 0;
    p[0x35] = hb;
    p[0x44] = static_cast<unsigned char>(half);
    p[0x45] = hb;
    p[4] = p[5] = p[6] = 0xAC;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, menu_windows::g.get_clut(clut_x, 0x1E1));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, 0xF);
    menu_windows::g.commit(1, 0x48);

    p = Next();
    menu_windows::g.set_poly_ft4(p);
    menu_windows::g.set_semi(p, 1);
    const float fright = F(xs + parity + half16 * 2 + 2);
    const auto u3 = static_cast<unsigned char>(parity + half);
    Put(p + 8, fmid);
    Put(p + 0xC, fy);
    p[0x14] = 0;
    Put(p + 0x1C, fy);
    Put(p + 0x28, fmid);
    Put(p + 0x2C, fyh);
    Put(p + 0x18, fright);
    Put(p + 0x38, fright);
    Put(p + 0x3C, fyh);
    p[0x15] = 0;
    p[0x24] = u3;
    p[0x25] = 0;
    p[0x34] = 0;
    p[0x35] = hb;
    p[0x44] = u3;
    p[0x45] = hb;
    p[4] = p[5] = p[6] = 0xAC;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, menu_windows::g.get_clut(clut_x, 0x1E1));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, 0xF);
    menu_windows::g.commit(1, 0x48);

    p = Next();
    menu_windows::g.set_poly_ft4(p);
    menu_windows::g.set_semi(p, 1);
    const std::int32_t xe = xs + ws;
    const float fe2 = F(xe - 2), fe = F(xe);
    Put(p + 0xC, fy);
    Put(p + 0x1C, fy2);
    p[0x14] = 0;
    Put(p + 8, fe2);
    p[0x15] = 0;
    p[0x24] = 2;
    p[0x34] = 0;
    p[0x44] = 2;
    Put(p + 0x18, fe);
    p[4] = 0xAC;
    Put(p + 0x28, fe2);
    Put(p + 0x38, fe);
    p[5] = p[6] = 0xAC;
    Put(p + 0x3C, fyh3);
    Put(p + 0x2C, Add(fyh, -1.0));
    p[0x25] = 2;
    p[0x35] = static_cast<unsigned char>(hb - 1);
    p[0x45] = hb3;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, menu_windows::g.get_clut(clut_x, 0x1E1));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, 0xF);
    menu_windows::g.commit(1, 0x48);

    const unsigned char* const whole = PoolRect(0, 0, 0x100, 0x100);
    menu_windows::g.draw_mode(Next(), 0, 1, 0xF, Addr(whole));
    menu_windows::g.commit(1, 0xC);
    menu_windows::g.outline(x + 2, y + 2, static_cast<int>(w1 - 5), static_cast<int>(h1 - 5), 0);
}

// original 0x574EC0: the equipment screen's record panel (the field menu's
// 0x59A91E and 0x5969CC; the shop's): box (x + 4, y + 4) 0x78 x 0xA3, the name
// (record +0, five characters) at (x + 0x20, y + 7), the four stat labels at
// x + 5 and values at x + 0x31, 13 apart from y + 0x1A; when no_preview's byte
// is 0, Equip_PreviewSet(record, set, marks, values) and for each stat an
// arrow cell (Menu_DrawCell8, u 0x11 for mark 4, 0x12 for mark 1, else 0x13)
// at x + 0x51 and the previewed value in the mark's colour at x + 0x59; then
// the six equipment names (weapon, three armour, two accessories: the name
// tables by record +0x12..+0x17) with their icons (0x66386C), the panel's +0xB
// slot raised in colours 7 / 2, its +0xA slot in 7 / 0, the rest in 0; the
// frame pieces.
// As the original has it: the six names are the tables' entries by bytes
// unchecked; the panel's +0xB and +0xA are read again for every slot.
extern "C" void __cdecl Menu_DrawEquipCompare(unsigned record, int x, int y, const unsigned char* set, unsigned no_preview,
                                              const unsigned char* panel) {
    SH_CALL(Menu_DrawBox)(x + 4, y + 4, 0x78, 0xA3, 0, Style());
    const unsigned char* const rec = Record(record);
    SH_CALL(Text_DrawAt)(x + 0x20, y + 7, 0, 5, rec);
    for (unsigned i = 0; i < 4; ++i)
        SH_CALL(Text_DrawAt)(x + 5, y + 0x1A + 13 * static_cast<int>(i), 0, 4, Text(::bof3::Img(field_o::at::kStatLabels) + 8 * i));
    static const unsigned char kStats[4] = {0x24, 0x26, 0x2A, 0x28};
    for (unsigned i = 0; i < 4; ++i) {
        Sprintf(::bof3::Img(field_o::at::kFmtNumber), static_cast<unsigned>(Word(rec + kStats[i])));
        SH_CALL(Text_DrawFont8)(x + 0x31, y + 0x1C + 13 * static_cast<int>(i), 0, Buf());
    }
    if (static_cast<unsigned char>(no_preview) == 0) {
        unsigned char marks[4];
        unsigned short values[4];
        SH_CALL(Equip_PreviewSet)(record, set, marks, values);
        for (unsigned i = 0; i < 4; ++i) {
            const unsigned char m = marks[i];
            const unsigned char u = m == 4 ? 0x11 : m == 1 ? 0x12 : 0x13;
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xF, 0);
            SH_CALL(Gfx_CommitPrim)(1, 0xC);
            const int row = y + 13 * static_cast<int>(i) + 0x1C;
            const unsigned clut = SH_CALL(Gpu_GetClut)(0, 0x1E0);
            SH_CALL(Menu_DrawCell8)(static_cast<unsigned>(x + 0x51), static_cast<unsigned>(row), u, 0x1C, clut, 0x80);
            Sprintf(::bof3::Img(field_o::at::kFmtNumber), static_cast<unsigned>(values[i]));
            SH_CALL(Text_DrawFont8)(x + 0x59, row, marks[i], Buf());
        }
    }
    const unsigned char* names[6] = {
        AtFieldO(::bof3::Img(bof3::addr::NameTable_Weapons) + rec[0x12] * 28u), AtFieldO(::bof3::Img(bof3::addr::NameTable_Armour) + rec[0x13] * 26u),
        AtFieldO(::bof3::Img(bof3::addr::NameTable_Armour) + rec[0x14] * 26u),  AtFieldO(::bof3::Img(bof3::addr::NameTable_Armour) + rec[0x15] * 26u),
        AtFieldO(::bof3::Img(bof3::addr::NameTable_Accessories) + rec[0x16] * 24u), AtFieldO(::bof3::Img(bof3::addr::NameTable_Accessories) + rec[0x17] * 24u),
    };
    for (unsigned b = 0; b < 6; ++b) {
        const int row = y + 13 * static_cast<int>(b);
        const unsigned char icon = AtFieldO(::bof3::Img(field_o::at::kEquipIcons))[b];
        const unsigned char* const name = names[b];
        const bool raised = b == panel[0xB] || b == panel[0xA];
        if (raised) {
            const unsigned char under = b == panel[0xB] ? 2 : 0;
            SH_CALL(Menu_DrawIcon8)(x + 6, row + 0x57, icon, 1);
            SH_CALL(Text_DrawAt)(x + 0x11, row + 0x55, 7, SH_CALL(Text_CharCount)(name), name);
            SH_CALL(Menu_DrawIcon8)(x + 6, row + 0x55, AtFieldO(::bof3::Img(field_o::at::kEquipIcons))[b], 0);
            SH_CALL(Text_DrawAt)(x + 0x11, row + 0x53, under, SH_CALL(Text_CharCount)(name), name);
        } else {
            SH_CALL(Menu_DrawIcon8)(x + 6, row + 0x57, icon, 0);
            SH_CALL(Text_DrawAt)(x + 0x11, row + 0x55, 0, SH_CALL(Text_CharCount)(name), name);
        }
    }
    SH_CALL(Menu_DrawPieces)(x, y, Text(::bof3::Img(field_o::at::kEquipPieces)), 1);
    for (unsigned b = 0; b < 7; ++b) {
        const int row = y + 8 * static_cast<int>(b) + 0x18;
        SH_CALL(Menu_DrawPiece)(x, row, 4, 1);
        SH_CALL(Menu_DrawPiece)(x + 0x78, row, 8, 1);
    }
    for (unsigned b = 0; b < 9; ++b) {
        const int row = y + 8 * static_cast<int>(b) + 0x58;
        SH_CALL(Menu_DrawPiece)(x, row, 4, 1);
        SH_CALL(Menu_DrawPiece)(x + 0x78, row, 8, 1);
    }
    const int mid = y + 0x50, bottom = y + 0xA0;
    for (unsigned b = 0; b < 0xE; ++b) {
        const int column = x + 8 * static_cast<int>(b) + 8;
        SH_CALL(Menu_DrawPiece)(column, mid, 0xE, 1);
        SH_CALL(Menu_DrawPiece)(column, bottom, 0x11, 1);
    }
    SH_CALL(Menu_DrawPiece)(x, mid, 0xD, 1);
    SH_CALL(Menu_DrawPiece)(x + 0x78, mid, 0xF, 1);
    SH_CALL(Menu_DrawPiece)(x, bottom, 0x10, 1);
    SH_CALL(Menu_DrawPiece)(x + 0x78, bottom, 0x12, 1);
}

// original 0x575430 (PSX 0x801DB988 and three more overlays' copies): a
// member's panel in the weapon shop - the box (x + 3, y + 3, 0x6C, 0x30), kind
// 1 (dark) when the member cannot equip the item (its bit in the equip mask of
// the slot's category: slot 1 weapons, 2..4 armour, 5, 10, 11 accessories,
// else consumables); three numbers in the 8 x 8 font 15 apart - when it can,
// what 0x590960 answers the stats would become, each in its colour (0, 2 or
// 3), from its first, second and FOURTH outputs; when it cannot, the record's
// u16s +0x24, +0x26, +0x28 in colour 7; the member's icon (record +9), dark
// when it cannot; the three stat labels, greyed (7) when it cannot; and the
// piece list 0x663528.
extern "C" void __cdecl Shop_DrawMemberStats(int x, int y, unsigned member, unsigned slot, unsigned item) {
    static const unsigned char kCategory[11] = {1, 2, 2, 2, 3, 0, 0, 0, 0, 3, 3};   // 0x575684 by way of 0x575674
    const unsigned m = member & 0xFF;
    const unsigned char* const record = At(::bof3::Img(CharacterRecords_addr) + m * menu_windows::at::kCharStride);
    unsigned char colour = 0;
    const unsigned s1 = (slot & 0xFF) - 1;
    const unsigned char category = s1 > 10 ? 0 : kCategory[s1];
    const unsigned mask = menu_windows::g.equip_mask(category, item);
    const unsigned char cannot = ((1u << (m & 31)) & mask & 0xFF) == 0 ? 1 : 0;
    menu_windows::g.box(x + 3, y + 3, 0x6C, 0x30, cannot, static_cast<unsigned char>(Config_WindowColour));
    if (!cannot) {
        unsigned char colours[4] = {0, 0, 0, 0};
        unsigned short stats[4] = {0, 0, 0, 0};
        menu_windows::g.equip_compare(member, (slot & ~0xFFu) | ((slot - 1) & 0xFF), item, colours, stats);
        stats[2] = stats[3];
        colours[2] = colours[3];
        for (unsigned i = 0; i < 3; ++i) {
            menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmt3d.v)), static_cast<unsigned>(stats[i]));
            menu_windows::g.font8(x + 0x2C, y + static_cast<int>(15 * i) + 8, colours[i],
                    reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
        }
    } else {
        colour = 7;
        static const unsigned kStat[3] = {0x24, 0x26, 0x28};
        static const int kY[3] = {8, 0x17, 0x26};
        for (unsigned i = 0; i < 3; ++i) {
            menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmt3d.v)), static_cast<unsigned>(Word(record + kStat[i])));
            menu_windows::g.font8(x + 0x2C, y + kY[i], 7, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
        }
    }
    menu_windows::g.item_icon(x + 0x46, y + 2, record[9], cannot);
    menu_windows::g.text_draw_at(x + 5, y + 6, colour, 0xFF, At(::bof3::Img(menu_windows::at::kStatLabels[0])));
    menu_windows::g.text_draw_at(x + 5, y + 0x15, colour, 0xFF, At(::bof3::Img(menu_windows::at::kStatLabels[1])));
    menu_windows::g.text_draw_at(x + 5, y + 0x24, colour, 0xFF, At(::bof3::Img(menu_windows::at::kStatLabels[2])));
    menu_windows::g.pieces(x, y, At(::bof3::Img(0x663528)), 0);
}

// original 0x575690 (PSX 0x801DBCBC): the menu backdrop (docs/menu-screens.md
// section 2) - ten columns 0x20 apart, each tiled down with SPRTs of the
// rectangles 0x663874 in the order of the pattern 0x663920 from kind's start
// (0x66396C) until 0xF0 deep, the pattern carrying on across each pair of
// columns; CLUT (0x30 / 0x50 / 0x40 / 0x60, 0x1E2) by kind; page (0x340,
// 0x100). kind is Config's "Background" byte 0x903A5B, which Config keeps in
// 0..3 (0x461239 / 0x46126D).
//
// Not as the original (DIV-0030): a kind of 4 or more reads the four CLUTs'
// array past its end - the return address, the argument, then the caller's
// frame - and the start table 0x66396C past its four entries. C++ cannot
// reproduce the stack read; drawn by Capcom's code, every such kind tried
// (4..8, 16, 64, 255) showed no backdrop at all, the menu on black
// (tools/recipes/backdrop_kinds.txt). Ours draws none: the same draw mode and
// CLUT lookups, then no tiles. Config keeps the byte in 0..3; only a corrupted
// save reaches this.
extern "C" void __cdecl Menu_DrawBackdrop(unsigned kind) {
    PiecePage(0x340);
    std::uint16_t cluts[4];
    cluts[0] = static_cast<std::uint16_t>(menu_windows::g.get_clut(0x30, 0x1E2));
    cluts[1] = static_cast<std::uint16_t>(menu_windows::g.get_clut(0x50, 0x1E2));
    cluts[2] = static_cast<std::uint16_t>(menu_windows::g.get_clut(0x40, 0x1E2));
    cluts[3] = static_cast<std::uint16_t>(menu_windows::g.get_clut(0x60, 0x1E2));
    const unsigned k = kind & 0xFF;
    if (k >= 4) return;   // DIV-0030
    const std::uint16_t clut = cluts[k];
    // DIV-0041: a wide picture gets ceil(columns / 64) pairs of columns more
    // each side (Widescreen_BackdropPairs) - at 16:9 one, from -0x40 to 0x160,
    // so the pattern covers -53..373 and the original's five pairs from 0 are
    // the middle of the seven; at 21:9 two, -0x80..0x1C0 over -120..440.
    const int extra = static_cast<int>(Widescreen_BackdropPairs());
    std::int32_t column = -0x40 * extra;
    for (int outer = 0; outer < 5 + 2 * extra; ++outer) {
        const unsigned char* pattern = At(::bof3::Img(menu_windows::at::kBackdropPattern.v) + At(::bof3::Img(menu_windows::at::kBackdropStart.v))[k]);
        for (int inner = 0; inner < 2; ++inner) {
            const float fcolumn = F(S16MenuWindows(static_cast<std::uint32_t>(column)));
            std::uint32_t row = 0;
            do {
                unsigned char* const p = Next();
                menu_windows::g.set_sprt(p);
                menu_windows::g.set_semi(p, 0);
                Put(p + 8, fcolumn);
                rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, clut);
                Put(p + 0xC, F(S16MenuWindows(row)));
                const unsigned char* const r = At(::bof3::Img(menu_windows::at::kBackdropRects.v) + *pattern++ * 4u);
                p[0x14] = r[0];
                p[0x15] = r[1];
                rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, r[2]);
                rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, r[3]);
                p[4] = p[5] = p[6] = 0x80;
                menu_windows::g.commit(1, 0x1C);
                row += r[3];
            } while (S16MenuWindows(row) < 0xF0);
            column += 0x20;
        }
    }
}

// original 0x575830 (PSX 0x801DBEB0 and three more overlays' copies): a menu
// panel - the box (x + 3, y + 3, w * 8 + 0x2D, h * 8 + 0x18) in the window
// colour, then its frame of pieces 0..8 on page (0x340, 0x100) with flag 1: a
// top of w pieces between two corner pieces, a bottom of w + 5, the sides h
// deep. w and h are read as bytes.
//
// As the original has it: the bottom's counter is a byte compared with
// w + 5, so a w of 251 or more never ends (no caller passes one).
extern "C" void __cdecl Menu_DrawPanel(int x, int y, int w, int h) {
    const unsigned wb = static_cast<unsigned>(w) & 0xFF, hb = static_cast<unsigned>(h) & 0xFF;
    const int height = static_cast<int>(hb) * 8 + 0x18;
    menu_windows::g.box(x + 3, y + 3, static_cast<int>(wb) * 8 + 0x2D, height, 0, static_cast<unsigned char>(Config_WindowColour));
    PiecePage(0x340);
    menu_windows::g.piece(x, y, 0, 1);
    for (unsigned char c = 0; c < wb; ++c) menu_windows::g.piece(x + c * 8 + 0x20, y, 1, 1);
    menu_windows::g.piece(x + static_cast<int>(wb) * 8 + 0x20, y, 2, 1);
    const int right = x + static_cast<int>(wb) * 8 + 0x30;
    menu_windows::g.piece(right, y, 3, 1);
    const int n = static_cast<int>(wb) + 5;
    unsigned char c = 0;
    do {
        menu_windows::g.piece(x + c * 8 + 8, y + height, 6, 1);
        ++c;
    } while (static_cast<int>(c) < n);
    for (unsigned char r = 0; r < hb; ++r) {
        menu_windows::g.piece(x, y + r * 8 + 0x18, 4, 1);
        menu_windows::g.piece(right, y + r * 8 + 0x18, 8, 1);
    }
    menu_windows::g.piece(x, y + height, 5, 1);
    menu_windows::g.piece(right, y + height, 7, 1);
}

// original 0x5759C0: the item list window (the field menu's Items, a shop's
// sell list). The window record: +4 / +6 x and y, +8 the Item_CanUse mode, +9
// greyed, +0xA the category, +0xB the top index, +0xC a marked index, +0xD the
// cursor, +0x10 a word of two arrow flags and a countdown, +0x12 the scroll
// state (Menu_ListScroll).
//
// The box (x + 3, y + 3, 0x99, 0x9A); the scroll step, whose offset (s8)
// shifts the rows and whose moving flag draws a tenth row; then each row, 13
// apart, whose item is not 0: Menu_DrawItemRow, colour 7 (and dim) when
// greyed or not usable, 2 on the cursor; the cursor's and the marked row drawn
// 2 higher over a dim shadow (none for a greyed row). The count is 1 for
// category 4 (key items), else the counts array's. While the scroll state is
// 0xFn the marks are not shifted with the offset. Then the title and footer
// boxes, the category's label centred, "%3d/%3d" of 0x591A80 and the
// category's room (0x20 key items, else 0x80), the arrows by +0x10's bits 1
// and 0, the countdown in +0x10's high nibble stepped down (the word zeroed
// when it runs out), the frame's pieces and the scroll bar.
extern "C" void __cdecl Menu_DrawItemList(unsigned char* window) {
    unsigned char* const w = window;
    menu_windows::g.box(rv::As<records::WindowRecord>(w)->x + 3, rv::As<records::WindowRecord>(w)->y + 3, 0x99, 0x9A, w[9], static_cast<unsigned char>(Config_WindowColour));
    unsigned char offset = 0, moving = 0;
    const unsigned char top = menu_windows::g.list_scroll(w + 0xB, &offset, &moving, w + 0x12);
    const unsigned category = w[0xA];
    const unsigned char* items = Ptr(::bof3::Img(menu_windows::at::kInventory.v) + category * 4) + top;
    const unsigned char* counts = Ptr(::bof3::Img(menu_windows::at::kCounts.v) + category * 4) + top;
    std::uint32_t row = static_cast<std::uint16_t>(static_cast<signed char>(offset) + rv::As<records::WindowRecord>(w)->y) + 0x1Au;
    const unsigned rows = moving + 9u;
    for (unsigned char i = 0; i < rows; ++i) {
        if (items[0] != 0) {
            unsigned char dim;
            if (w[9] == 0) {
                const unsigned char member = Character_IndexById[Cond_Flags[0xd2 + BattleActorCopies_ItemActor]];
                dim = menu_windows::g.can_use(w[8], member, w[0xA], items[0]) ? 0 : 1;
            } else {
                dim = 1;
            }
            unsigned char colour = dim ? 7 : 0;
            const unsigned char adj = (static_cast<std::uint16_t>(rv::As<records::ItemListWindow>(w)->scroll) & 0xFFF0) != 0xF0 ? moving : 0;
            const unsigned mark = w[0xD] + static_cast<unsigned>(adj);
            const unsigned here = w[0xB] + static_cast<unsigned>(i);
            if (here == mark) colour = 2;
            const unsigned char row_category = w[0xA];
            const unsigned char count = row_category == 4 ? 1 : counts[0];
            if (here == mark || here == w[0xC] + static_cast<unsigned>(adj)) {
                if (colour != 7)
                    menu_windows::g.item_row(rv::As<records::WindowRecord>(w)->x + 7, static_cast<int>(row), 7, row_category, items[0], count, 1);
                menu_windows::g.item_row(rv::As<records::WindowRecord>(w)->x + 7, static_cast<int>(row) - 2, colour, w[0xA], items[0], count, dim);
            } else {
                menu_windows::g.item_row(rv::As<records::WindowRecord>(w)->x + 7, static_cast<int>(row), colour, row_category, items[0], count, dim);
            }
        }
        ++items;
        ++counts;
        row += 0xD;
    }

    menu_windows::g.box(rv::As<records::WindowRecord>(w)->x + 3, rv::As<records::WindowRecord>(w)->y + 3, 0x99, 0x14, w[9], static_cast<unsigned char>(Config_WindowColour));
    menu_windows::g.box(rv::As<records::WindowRecord>(w)->x + 3, rv::As<records::WindowRecord>(w)->y + 0x92, 0x99, 8, w[9], static_cast<unsigned char>(Config_WindowColour));
    {
        const unsigned char grey = w[9] ? 7 : 0;
        const int label_y = rv::As<records::WindowRecord>(w)->y + 7;
        const unsigned char* const label = Ptr(::bof3::Img(menu_windows::at::kCategoryLabels.v) + w[0xA] * 4u);
        const unsigned n = menu_windows::g.char_count(label);
        // DIVERGENCE DIV-0059: the original's 6 * (13 - n) unless a Latin overlay is on.
        menu_windows::g.text_draw_at(static_cast<std::uint16_t>(ListTitle_X(label, static_cast<int>(n)) + rv::As<records::WindowRecord>(w)->x), label_y, grey, 0x10,
                       label);
    }
    {
        const unsigned char cat = w[0xA];
        const unsigned room = cat != 4 ? 0x80 : 0x20;
        const unsigned have = menu_windows::g.count_category(cat) & 0xFF;
        menu_windows::g.sprintf_(PrintBufMenuWindows(), FormatMenuWindows(::bof3::Img(menu_windows::at::kFmt3d3d.v)), have, room);
        const unsigned char grey = w[9] ? 7 : 0;
        menu_windows::g.font8(rv::As<records::WindowRecord>(w)->x + 0x55, rv::As<records::WindowRecord>(w)->y + 0x92, grey, reinterpret_cast<const unsigned char*>(PrintBufMenuWindows()));
    }
    menu_windows::g.pieces(rv::As<records::WindowRecord>(w)->x, rv::As<records::WindowRecord>(w)->y, At(::bof3::Img((w[0x10] & 2) ? 0x66349C : 0x663484)), 1);
    menu_windows::g.pieces(rv::As<records::WindowRecord>(w)->x, rv::As<records::WindowRecord>(w)->y, At(::bof3::Img((w[0x10] & 1) ? 0x6634A8 : 0x663490)), 1);
    {
        const unsigned t = static_cast<std::uint16_t>(rv::As<records::ItemListWindow>(w)->arrows);
        rv::Store<unsigned>(rv::As<records::ItemListWindow>(w)->arrows, (t & 0xF0) ? t - 0x10 : 0);
    }
    for (unsigned b = 0; b < 10; ++b) menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + static_cast<int>(b * 8) + 0x28, rv::As<records::WindowRecord>(w)->y, 1, 1);
    for (unsigned b = 0; b < 15; ++b) menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x, rv::As<records::WindowRecord>(w)->y + static_cast<int>(b * 8) + 0x18, 4, 1);
    menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 0x90, rv::As<records::WindowRecord>(w)->y + 0x18, 0x16, 1);
    for (unsigned b = 0; b < 12; ++b) menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 0x90, rv::As<records::WindowRecord>(w)->y + static_cast<int>(b * 8) + 0x28, 0x17, 1);
    menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 0x90, rv::As<records::WindowRecord>(w)->y + 0x88, 0x18, 1);
    menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x, rv::As<records::WindowRecord>(w)->y + 0x90, 0x19, 1);
    for (unsigned b = 0; b < 9; ++b) menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + static_cast<int>(b * 8) + 8, rv::As<records::WindowRecord>(w)->y + 0x90, 0x1A, 1);
    menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 0x50, rv::As<records::WindowRecord>(w)->y + 0x90, 0x1B, 1);
    for (unsigned b = 0; b < 6; ++b) menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + static_cast<int>(b * 8) + 0x58, rv::As<records::WindowRecord>(w)->y + 0x90, 0x1C, 1);
    menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 0x88, rv::As<records::WindowRecord>(w)->y + 0x90, 0x1D, 1);
    if (w[0xA] == 4) {
        menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 8, rv::As<records::WindowRecord>(w)->y, 0x32, 1);
        menu_windows::g.piece(rv::As<records::WindowRecord>(w)->x + 0x90, rv::As<records::WindowRecord>(w)->y, 0x33, 1);
    }
    const unsigned char cat = w[0xA];
    menu_windows::g.scroll_bar(Ptr(::bof3::Img(menu_windows::at::kInventory.v) + cat * 4u), w[0xB], rv::As<records::WindowRecord>(w)->x + 0x90, rv::As<records::WindowRecord>(w)->y + 0x18, 9,
                 cat == 4 ? 0x20 : 0x80, 0x74);
}

// original 0x575F50: the ability panel (the field menu's 0x59A9D9; panel +4 /
// +6 x and y, +8 the Skill_CanUse mode, +9 the arrows' lit bits and a
// countdown in its high nibble, +0xA the member, +0xB the list's type, +0xC /
// +0xD the rows drawn raised, +0x10 a word: the top arrows). Box (x + 3,
// y + 3) 0x9A x 0xA0; Char_AbilityList(member, type, 0)'s ten ids from
// y + 0x1A, 13 apart, through Menu_DrawSkillRow at x + 7: colour 7 for one
// Skill_CanUse refuses (dimmed), 2 on row +0xD, else 0; the rows +0xC / +0xD
// drawn raised (a copy in colour 7 unless already 7, then at y - 2); the title
// 0x663984[type] centred at y + 7; the arrows (0x66349C lit or 0x663484,
// 0x6634C0 lit or 0x6634B4), +9's countdown stepped (its high nibble less 1, or
// the byte 0); the frame pieces.
// As the original has it: the type indexes the title table unchecked, the
// shown slot 0x929F06 is a signed byte into the party list.
extern "C" void __cdecl Menu_DrawAbilityPanel(unsigned char* panel) {
    SH_CALL(Menu_DrawBox)(PanelX(panel) + 3, PanelY(panel) + 3, 0x9A, 0xA0, 0, Style());
    const unsigned char* list = SH_CALL(Char_AbilityList)(panel[0xA], panel[0xB], 0);
    int row = PanelY(panel) + 0x1A;
    for (unsigned b = 0; b < 0xA; ++b, row += 0xD, ++list) {
        const unsigned char id = list[0];
        if (id == 0) continue;
        const unsigned char can = SH_CALL(Skill_CanUse)(panel[8], static_cast<unsigned char>(BattleActorCopies_ItemActor), id);
        const unsigned char dim = can == 0;
        unsigned char colour = dim ? 7 : 0;
        if (b == panel[0xD]) colour = 2;
        const unsigned char kind = SH_CALL(Skill_FlagIndex)(list[0]);
        const unsigned char* const rec = AtFieldO(::bof3::Img(bof3::addr::Ability_Records) + list[0] * 24u);
        const unsigned char cost = SH_CALL(Skill_ApCost)(ShownMember(), list[0], 0);
        if (b != panel[0xD] && b != panel[0xC]) {
            SH_CALL(Menu_DrawSkillRow)(PanelX(panel) + 7, row, colour, kind, rec, cost, dim);
            continue;
        }
        if (colour != 7) SH_CALL(Menu_DrawSkillRow)(PanelX(panel) + 7, row, 7, kind, rec, cost, 1);
        SH_CALL(Menu_DrawSkillRow)(PanelX(panel) + 7, row - 2, colour, kind, rec, cost, dim);
    }
    const char* const title = Str(static_cast<U>(Long(AtFieldO(::bof3::Img(field_o::at::kAbilityTitles) + panel[0xB] * 4u))));
    const int ty = PanelY(panel) + 7;
    SH_CALL(Text_DrawAt)(Centred(PanelX(panel), static_cast<unsigned>(std::strlen(title)),
                                 reinterpret_cast<const unsigned char*>(title)),
                         ty, 0, 0x10, reinterpret_cast<const unsigned char*>(title));
    SH_CALL(Menu_DrawPieces)(PanelX(panel), PanelY(panel), Text(panel[9] & 2 ? ::bof3::Img(field_o::at::kListPiecesLit) : ::bof3::Img(field_o::at::kListPiecesA)), 1);
    SH_CALL(Menu_DrawPieces)(PanelX(panel), PanelY(panel), Text(panel[9] & 1 ? ::bof3::Img(field_o::at::kListPiecesBLit) : ::bof3::Img(field_o::at::kListPiecesB)), 1);
    const unsigned char arrows = panel[9];
    panel[9] = arrows & 0xF0 ? static_cast<unsigned char>(arrows - 0x10) : 0;
    for (unsigned b = 0; b < 0xA; ++b) SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 0x28, PanelY(panel), 1, 1);
    for (unsigned b = 0; b < 0x11; ++b) SH_CALL(Menu_DrawPiece)(PanelX(panel), PanelY(panel) + 8 * static_cast<int>(b) + 0x18, 4, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 0x18, 0x20, 1);
    for (unsigned b = 0; b < 0xF; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 8 * static_cast<int>(b) + 0x20, 0x21, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel), PanelY(panel) + 0xA0, 5, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 0x98, 0x22, 1);
    if (Word(panel + 0x10) != 0) {
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8, PanelY(panel), 0x32, 1);
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel), 0x34, 1);
    }
    for (unsigned b = 0; b < 0x11; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 8, PanelY(panel) + 0xA0, 6, 1);
}

// original 0x5762D0 (PSX 0x801DD304): the shop windows' border - pieces 0x29
// along the top and 0x2E along the bottom (w of them), 0x2B and 0x2C down the
// sides (h of them), corners 0x28 0x2A 0x2D 0x2F - on page (0x380, 0x100). w
// and h are counts of 8-unit cells, read as bytes.
extern "C" void __cdecl Menu_DrawBorder(int x, int y, int w, int h) {
    PiecePage(0x380);
    const unsigned wb = static_cast<unsigned>(w) & 0xFF, hb = static_cast<unsigned>(h) & 0xFF;
    if (wb) {
        const int bottom = y + static_cast<int>(hb) * 8 + 8;
        for (unsigned char i = 0; i < wb; ++i) {
            menu_windows::g.piece(x + i * 8 + 8, y, 0x29, 0);
            menu_windows::g.piece(x + i * 8 + 8, bottom, 0x2E, 0);
        }
    }
    if (hb) {
        const int right = x + static_cast<int>(wb) * 8 + 8;
        for (unsigned char j = 0; j < hb; ++j) {
            menu_windows::g.piece(x, y + j * 8 + 8, 0x2B, 0);
            menu_windows::g.piece(right, y + j * 8 + 8, 0x2C, 0);
        }
    }
    menu_windows::g.piece(x, y, 0x28, 0);
    const int right = x + static_cast<int>(wb) * 8 + 8;
    menu_windows::g.piece(right, y, 0x2A, 0);
    const int bottom = y + static_cast<int>(hb) * 8 + 8;
    menu_windows::g.piece(x, bottom, 0x2D, 0);
    menu_windows::g.piece(right, bottom, 0x2F, 0);
}

// original 0x5763F0: the item panel (the field menu's 0x59AA29 and 0x596999;
// panel +8 the category, +9 the filter, +0xA the list's top, +0xB the cursor,
// +0xC the shown slot, +0xD the id under the cursor, +0x10 the scroll state).
// The category from +9: 1..3 -> 2, 4..5 -> 3, else 1; of its 0x80 ids and
// counts (Inventory_IdLists / Inventory_CountLists) those Item_CanUse(4,
// member, category, id) passes - and, for category 2, whose Item_IconKind
// less 2 is the filter less 1 - kept in order at 0x6BC760 / 0x6BC7E0 and the
// rest of both lists zeroed; +0xD the kept id under the cursor; box (x + 3,
// y + 3) 0x99 x 0x9A; Menu_ListScroll(top, offset, moving, state); rows
// moving + 9 from the kept list at its answer, y + offset + 0x1A, 13 apart,
// through Menu_DrawItemRow at x + 7 (the cursor's row raised: colour 7
// dimmed, then 0 at y - 2); two boxes, the title 0x663994[category] centred,
// "kept / 128" (0x6639C8) at (x + 0x55, y + 0x92), the frame pieces and
// Menu_DrawScrollBar over the kept ids.
// As the original has it: the rows past the list's 0x80 read on into .data;
// the cursor +0xB indexes the kept ids unchecked.
extern "C" void __cdecl Menu_DrawItemPanel(unsigned char* panel) {
    switch (panel[9]) {
    case 1: case 2: case 3: panel[8] = 2; break;
    case 4: case 5: panel[8] = 3; break;
    default: panel[8] = 1; break;
    }
    const unsigned cat = panel[8];
    const unsigned char* ids = AtFieldO(static_cast<U>(Long(AtFieldO(::bof3::Img(bof3::addr::Inventory_IdLists) + 4 * cat))));
    const unsigned char* counts = AtFieldO(static_cast<U>(Long(AtFieldO(::bof3::Img(bof3::addr::Inventory_CountLists) + 4 * cat))));
    unsigned char kept = 0, total = 0;
    for (unsigned n = 0; n < 0x80; ++n, ++ids, ++counts) {
        const unsigned char member = Character_IndexById[AtFieldO(::bof3::Img(Cond_Flags_addr) + 0xd2 + panel[0xC])[0]];
        if (!SH_CALL(Item_CanUse)(4, member, panel[8], ids[0])) continue;
        if (panel[8] == 2) {
            const U kindv = SH_CALL(Item_IconKind)(2, ids[0]) & 0xFF;
            if (static_cast<U>(panel[9]) - 1u != kindv - 2u) continue;
        }
        AtFieldO(::bof3::Img(field_o::at::kListIds))[kept] = ids[0];
        AtFieldO(::bof3::Img(field_o::at::kListCounts))[kept] = counts[0];
        ++total;
        ++kept;
    }
    if (kept < 0x80) {
        std::memset(AtFieldO(::bof3::Img(field_o::at::kListCounts) + kept), 0, 0x80u - kept);
        std::memset(AtFieldO(::bof3::Img(field_o::at::kListIds) + kept), 0, 0x80u - kept);
    }
    panel[0xD] = AtFieldO(::bof3::Img(field_o::at::kListIds))[panel[0xB]];
    SH_CALL(Menu_DrawBox)(PanelX(panel) + 3, PanelY(panel) + 3, 0x99, 0x9A, 0, Style());
    unsigned char offset = 0, moving = 0;
    const unsigned char start = SH_CALL(Menu_ListScroll)(panel + 0xA, &offset, &moving, panel + 0x10);
    int row = PanelY(panel) + Sb(offset) + 0x1A;
    const unsigned char* item = AtFieldO(::bof3::Img(field_o::at::kListIds) + start);
    const std::int32_t to_counts = static_cast<std::int32_t>(field_o::at::kListCounts - field_o::at::kListIds);
    for (unsigned j = 0; static_cast<int>(j) < moving + 9; ++j, ++item, row += 0xD) {
        const unsigned char id = item[0];
        if (id == 0) continue;
        if (panel[0xA] + j == panel[0xB]) {
            SH_CALL(Menu_DrawItemRow)(PanelX(panel) + 7, row, 7, panel[8], id, item[to_counts], 1);
            SH_CALL(Menu_DrawItemRow)(PanelX(panel) + 7, row - 2, 0, panel[8], item[0], item[to_counts], 0);
        } else {
            SH_CALL(Menu_DrawItemRow)(PanelX(panel) + 7, row, 0, panel[8], id, item[to_counts], 0);
        }
    }
    SH_CALL(Menu_DrawBox)(PanelX(panel) + 3, PanelY(panel) + 3, 0x99, 0x14, 0, Style());
    SH_CALL(Menu_DrawBox)(PanelX(panel) + 3, PanelY(panel) + 0x92, 0x99, 8, 0, Style());
    const unsigned category = panel[8];
    const unsigned char* const title = Text(static_cast<U>(Long(AtFieldO(::bof3::Img(field_o::at::kItemTitles) + category * 4u))));
    const int ty = PanelY(panel) + 7;
    SH_CALL(Text_DrawAt)(Centred(PanelX(panel), SH_CALL(Text_CharCount)(title), title), ty, 0, 0x10, title);
    Sprintf(::bof3::Img(field_o::at::kFmtCount), static_cast<unsigned>(total), 0x80u);
    SH_CALL(Text_DrawFont8)(PanelX(panel) + 0x55, PanelY(panel) + 0x92, 0, Buf());
    SH_CALL(Menu_DrawPieces)(PanelX(panel), PanelY(panel), Text(::bof3::Img(field_o::at::kListPiecesA)), 1);
    SH_CALL(Menu_DrawPieces)(PanelX(panel), PanelY(panel), Text(::bof3::Img(field_o::at::kItemPiecesB)), 1);
    for (unsigned b = 0; b < 0xA; ++b) SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 0x28, PanelY(panel), 1, 1);
    for (unsigned b = 0; b < 0xF; ++b) SH_CALL(Menu_DrawPiece)(PanelX(panel), PanelY(panel) + 8 * static_cast<int>(b) + 0x18, 4, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 0x18, 0x16, 1);
    for (unsigned b = 0; b < 0xC; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 8 * static_cast<int>(b) + 0x28, 0x17, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel) + 0x88, 0x18, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel), PanelY(panel) + 0x90, 0x19, 1);
    for (unsigned b = 0; b < 9; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 8, PanelY(panel) + 0x90, 0x1A, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x50, PanelY(panel) + 0x90, 0x1B, 1);
    for (unsigned b = 0; b < 6; ++b)
        SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8 * static_cast<int>(b) + 0x58, PanelY(panel) + 0x90, 0x1C, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x88, PanelY(panel) + 0x90, 0x1D, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 8, PanelY(panel), 0x32, 1);
    SH_CALL(Menu_DrawPiece)(PanelX(panel) + 0x90, PanelY(panel), 0x33, 1);
    SH_CALL(Menu_DrawScrollBar)(AtFieldO(::bof3::Img(field_o::at::kListIds)), panel[0xA], PanelX(panel) + 0x90, PanelY(panel) + 0x18, 9, 0x80, 0x74);
}

// original 0x576960: a save or load slot (SaveMenu_DrawSlots 0x588D6B; the
// save header's summary, or 0 for an empty slot). Box (x + 8, y) 0xC8 x 0x30,
// the slot's number (0x6639D4) at (x + 9, y + 5); with a summary: a draw mode,
// the party's three icons (+5..+7, 0xFF none) through Menu_DrawBigIcon at
// x + 0x56, 41 apart, the name - +0..+4 and the four bytes +0x16 copied to
// 0x904BA0 - through Text_DrawAt (five characters) at (x + the inset, y + 2),
// the level +8 (0x64D3EC) at (x + 0x1F, y + 0x18), the time +0xC / +0xD
// (0x654830) and two colons (0x6639D0) at x + 0x27, Menu_DrawExpBar(x + 0x14,
// y + 0x12, 0, level, +0x10); without one the text 0x669CCC at (x + 0x13,
// y + 2). Then the frame pieces 0x663584 (flags 2 when +0x14 is set, else 0)
// and, with +0x14 set, piece 0x3D at (x + 0x32, y + 0x16).
// The inset is the disp8 at 0x576A48 - 0x13 as Capcom shipped it, 0x15 under
// DIV-0029 (YesNoLayout_Inject patches those bytes) - as yes_no_layout.cpp
// holds it (YesNoLayout_SaveNameInset; docs/operand-store.md).
// DIVERGENCE (exe-gap round, group AU; the coordinator numbers it): an empty
// slot draws no text, as the US PlayStation release draws it and as a
// disc-built image already did (0x669CCC is a gap there, zero). 0 until
// FieldO_Inject patches it to 1, after the fuzz, under the name SaveSlotBlank
// (BOF3X_ORIGINAL=SaveSlotBlank, or BOF3X_OFF, draws the port's string).

namespace field_o { unsigned char g_empty_slot_blank = 0; }

extern "C" void __cdecl Menu_DrawSaveSlot(unsigned slot, int x, int y, const unsigned char* summary) {
    SH_CALL(Menu_DrawBox)(x + 8, y, 0xC8, 0x30, 0, Style());
    Sprintf(::bof3::Img(field_o::at::kFmtSlot), slot & 0xFF);
    SH_CALL(Text_DrawFont8)(x + 9, y + 5, 0, Buf());
    unsigned flags = 0;
    if (summary != nullptr) {
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xF, 0);
        SH_CALL(Gfx_CommitPrim)(1, 0xC);
        for (unsigned b = 0; b < 3; ++b) {
            const unsigned char icon = summary[5 + b];
            if (icon != 0xFF) SH_CALL(Menu_DrawBigIcon)(x + 41 * static_cast<int>(b) + 0x56, y + 2, icon, 0);
        }
        std::memcpy(Buf(), summary, 4);
        Buf()[4] = summary[4];
        std::memcpy(Buf() + 5, summary + 0x16, 4);
        const unsigned char inset = YesNoLayout_SaveNameInset();   // the disp8 at at::kNameInsetSite: 0x13, DIV-0029's 0x15
        SH_CALL(Text_DrawAt)(x + inset, y + 2, 0, 5, Buf());
        Sprintf(::bof3::Img(Boss26Fx_CountFormat_addr), static_cast<unsigned>(summary[8]));
        SH_CALL(Text_DrawFont8)(x + 0x1F, y + 0x18, 0, Buf());
        Sprintf(::bof3::Img(field_o::at::kFmtTime), static_cast<unsigned>(summary[0xC]), static_cast<unsigned>(summary[0xD]));
        SH_CALL(Text_DrawFont8)(x + 0x27, y + 0x25, 0, Buf());
        SH_CALL(Text_DrawFont8)(x + 0x27, y + 0x24, 0, Text(::bof3::Img(field_o::at::kColon)));
        SH_CALL(Text_DrawFont8)(x + 0x27, y + 0x27, 0, Text(::bof3::Img(field_o::at::kColon)));
        SH_CALL(Menu_DrawExpBar)(x + 0x14, y + 0x12, 0, summary[8], static_cast<U>(Long(summary + 0x10)));
        flags = summary[0x14] ? 2 : 0;
    } else if (!field_o::g_empty_slot_blank) {
        SH_CALL(Text_DrawAt)(x + 0x13, y + 2, 0, 5, Text(::bof3::Img(field_o::at::kEmptySlot)));
    }
    SH_CALL(Menu_DrawPieces)(x, y, Text(::bof3::Img(field_o::at::kSlotPieces)), static_cast<int>(flags));
    if (summary != nullptr && summary[0x14] != 0) SH_CALL(Menu_DrawPiece)(x + 0x32, y + 0x16, 0x3D, 0);
}

void Menu_Inject() {
    BOF3_INJECT(Menu_DrawMemberStatus);
    BOF3_INJECT(Menu_DrawStatsPanel);
    BOF3_INJECT(Menu_DrawEquipPanel);
    BOF3_INJECT(Menu_DrawExpPanel);
    BOF3_INJECT(Menu_DrawCursorBox);
    BOF3_INJECT(Menu_DrawBigIcon);
    BOF3_INJECT(Menu_DrawItemIcon);
    BOF3_INJECT(Menu_DrawIconWheel);
    BOF3_INJECT(Menu_DrawTile16);
    BOF3_INJECT(Menu_DrawCell8);
    BOF3_INJECT(Menu_DrawExpBar);
    BOF3_INJECT(Menu_DrawMoneyBox);
    BOF3_INJECT(Menu_DrawPlayTime);
    BOF3_INJECT(Menu_YesNo);
    BOF3_INJECT(Menu_DrawButtonRow);
    BOF3_INJECT(Item_BasePrice);
    BOF3_INJECT(Char_ExpForLevel);
    BOF3_INJECT(Menu_DrawTitleBox);
    BOF3_INJECT(Menu_DrawEquipCompare);
    BOF3_INJECT(Shop_DrawMemberStats);
    BOF3_INJECT(Menu_DrawBackdrop);
    BOF3_INJECT(Menu_DrawPanel);
    BOF3_INJECT(Menu_DrawItemList);
    BOF3_INJECT(Menu_DrawAbilityPanel);
    BOF3_INJECT(Menu_DrawBorder);
    BOF3_INJECT(Menu_DrawItemPanel);
    BOF3_INJECT(Menu_DrawSaveSlot);
}

void MenuFromFieldO_Inject() {
    field_o::g_title_live = true;   // DIV-0059, from here on
    // DIVERGENCE DIV-0084: a Latin language overlay only, as DIV-0059's switch.
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("FormationNameCentre",
                         Low32(&field_o::g_wheel_name_centre),
                         &was, &is, 1);
        bof3::Log("DIV-0084    formation names centred: %s", field_o::g_wheel_name_centre ? "on" : "off");
    }
    // An empty save slot drawn blank on every install and language (the
    // owner's call of 2026-10-31, docs/exe-gap-round.md 7).
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("SaveSlotBlank", Low32(&field_o::g_empty_slot_blank),
                         &was, &is, 1);
        bof3::Log("SaveSlotBlank  an empty save slot draws %s", field_o::g_empty_slot_blank ? "no text" : "the port's text (0x669CCC)");
    }
}
