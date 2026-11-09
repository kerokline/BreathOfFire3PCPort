// Moved here by tools/move_funcs.py from menu_windows.cpp, mode_states.cpp, rest_2f.cpp, text_immediate.cpp, window_task.cpp (docs/source-layout-round.md).
#include "game/menu/window.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_verbs.h"
#include "game/menu_windows.h"
#include "game/menu_windows_callees.h"
#include "game/mode_states.h"
#include "game/mode_states_callees.h"
#include "game/move_script_bytes.h"
#include "game/msgbox.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2f.h"
#include "game/rest_2f_callees.h"
#include "game/scenario_harness.h"
#include "game/shoulder_glyphs.h"
#include "game/text_advance.h"
#include "game/text_immediate.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/window_task.h"
#include "game/window_task_callees.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

using namespace menu_windows;

using namespace mode_states;

using namespace window_task;

namespace menu_windows {

using move_script::At;

}  // namespace menu_windows

namespace {

using move_script::Long;

using move_script::Word;

unsigned char* Ptr(std::uint32_t at) { return menu_windows::At(static_cast<std::uint32_t>(move_script::Long(menu_windows::At(at)))); }

std::int16_t S16MenuWindows(std::uint32_t v) { return static_cast<std::int16_t>(v); }

}  // namespace

namespace mode_states {

using move_script::At;

using move_script::Word;

}  // namespace mode_states

namespace {

using U = std::uint32_t;

using Handler = void (__cdecl*)();

using rest_2f::kLeftOff170Bound;

constexpr ::bof3::image::Va kEffectState{0x66972C}; // Character_IndexById: the record of a member id

constexpr ::bof3::image::Va kPools{0x803580};       // MessagePools

constexpr ::bof3::image::Va kPrint{0x904BA0};       // the print buffer

// Window kind 1's message cells (the choice list).
// kListText: now MenuWindows_ListText (symbols.toml). the choices' text (a pointer)
// kListLast: now MenuWindows_ListCount (symbols.toml). the last choice's index (s8)
// kListCursor: now AreaW0c_ChoiceAnswer (symbols.toml). the cursor (s8)
constexpr ::bof3::image::Va kListRows{0x7DEE6F};    // a byte a choice: its line, then its y

constexpr ::bof3::image::Va kIdLists{0x656B00};     // Inventory_IdLists

constexpr ::bof3::image::Va kCountLists{0x656B14};  // Inventory_CountLists

constexpr ::bof3::image::Va kListTitles{0x66AF10};  // Win2_DrawItemList's titles by category

constexpr ::bof3::image::Va kShisuTitle{0x66AE90};  // Win1_DrawShisuPanel's title (a string)

constexpr ::bof3::image::Va kShisuIds{0x66AE8C};    // its four rows' item ids

constexpr ::bof3::image::Va kShisuPiecesA{0x66AE74};

constexpr ::bof3::image::Va kShisuPiecesB{0x66AE80};

constexpr ::bof3::image::Va kListPieces0{0x66AE98}; // Win2_DrawItemList's arrows: up off / on, down off / on

constexpr ::bof3::image::Va kListPieces2{0x66AEB0};

constexpr ::bof3::image::Va kListPieces1{0x66AEA4};

constexpr ::bof3::image::Va kListPieces3{0x66AEBC};

constexpr ::bof3::image::Va kUsedFormat{0x6639C8};  // "%3d/%3d"

unsigned char& BRest2f(U address) { return move_script::At(address)[0]; }

U WRest2f(U address) { return move_script::Word(move_script::At(address)); }

U Key(const void* p) { return Low32(p); }

// The record Field_RunTaskRecords runs, read afresh at each use.
unsigned char* Cur() { return move_script::At(BattleWinStates_Current); }

int Wx(const unsigned char* r) { return static_cast<int>(rv::As<records::WindowRecord>(r)->x); }

int Wy(const unsigned char* r) { return static_cast<int>(rv::As<records::WindowRecord>(r)->y); }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void Run(const char* who, const unsigned long* table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_2f.md section 7)",
                    who, byte, index, entries, (unsigned)Key(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

using TitleFn = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);

void Piece(int x, int y, unsigned id) { SH_CALL(Menu_DrawPiece)(x, y, id, 1); }

void Row(int x, U y, unsigned colour, unsigned category, unsigned id, unsigned count, unsigned dim) {
    SH_CALL(Menu_DrawItemRow)(x, static_cast<int>(y), static_cast<int>(colour), category, id, count, static_cast<int>(dim));
}

// The original's advance is 12: add ebp, 0xC at 0x5962E5 (TextAdvance_Of's default).
constexpr int kLine = 13;       // 0x59620B: add eax, 0xD

constexpr std::uint32_t kCharacterRecord = 0xA4;

constexpr std::uint32_t kTextRecord = 0x20;

const std::uint8_t* AtTextImmediate(std::uint32_t address) {
    return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

}  // namespace

namespace window_task {

using move_script::At;

using move_script::SetWord;

using move_script::Word;

namespace {

template <typename T> T As(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

void PutFloat(unsigned char* at, float v) { std::memcpy(at, &v, sizeof v); }

float Flt(int v) { return static_cast<float>(v); }

// The record the layer is running, re-read from 0x905B84 at every point the
// original re-reads it - after every call, and in most of these functions
// after every store too (the port's compiler never kept it in a register
// across one). Volatile so that ours cannot cache it either.
unsigned char* Rec() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(
        *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleWinStates_Current_addr)))));
}

// DIV-0041 (psp-widescreen-placements.md 2.6, the owner's answer of
// 2026-10-12): the side placements move out to the wide frame's edges. The
// placement code (0x7DEE6E, as each reader reads it) indexes three tables -
// kBoxXY and kBoxAltXY for kind 0's wide and narrow message box,
// MsgBox_PlacementTable for kind 2's box - and in all three entries 3 and 5
// stand at the left, 4 and 6 at the right, 0..2 centred. The PSP moved
// exactly those x values by 32 (its BOOT.BIN holds them at -16 / 136 / 216 for
// the boxes and 113 / 258 for kind 2: scratch boxtab2.py, tsprites.py); ours
// moves them by Widescreen_Anchor() where the tables are read, never in the
// tables, so the data stays the 4:3 values on every base. 0 unarmed or narrow.
int SideShift() {
    switch (Msgbox_Placement) {
    case 3:
    case 5:
        return -Widescreen_Anchor();
    case 4:
    case 6:
        return Widescreen_Anchor();
    default:
        return 0;
    }
}

// A placement table's x plus the side shift, as the u16 the readers go on with.
unsigned SideX(unsigned x) { return static_cast<unsigned>(static_cast<int>(x) + SideShift()); }

}  // namespace

}  // namespace window_task

namespace {

// The three kind dispatchers are one function three times over: byte +3 of
// the record through a five-entry table built on the stack, the same `sub
// esp, 0x14` / `call [esp + eax * 4]` in each. No bound on the state either.
void RunState(const window_task::Handler (&states)[5], const char* which) {
    const unsigned state = Rec()[3];
    if (state >= 5) bof3::Fatal("a %s window's state byte is %u; the original's stack table holds five", which, state);
    states[state]();
}

// The half-size each axis settles at while the box opens, << 4. Kind 0 takes
// the placement table (narrow or wide by flag 2), kind 1 the record's own
// +0x18 / +0x1A, kind 2 and anything above MsgBox_PlacementTable.
unsigned SettledX(const unsigned char* r) {
    switch (r[2]) {
    case 0:
        return SideX((r[0xD] & 2) != 0 ? window_task::At(::bof3::Img(window_task::at::kBoxAltXY.v))[Msgbox_Placement * 2u]
                                       : window_task::At(::bof3::Img(window_task::at::kBoxXY.v))[Msgbox_Placement * 2u]) << 4;
    case 1:
        return rv::As<records::WindowRecord>(r)->settled_x << 4;
    default:
        return SideX(window_task::Word(window_task::At(::bof3::Img(window_task::at::kPlaceXY.v) + Msgbox_Placement * 4u))) << 4;
    }
}

unsigned SettledY(const unsigned char* r) {
    switch (r[2]) {
    case 0:
        return ((r[0xD] & 2) != 0 ? window_task::At(::bof3::Img(window_task::at::kBoxAltXY.v))[Msgbox_Placement * 2u + 1u]
                                  : window_task::At(::bof3::Img(window_task::at::kBoxXY.v))[Msgbox_Placement * 2u + 1u]) << 4;
    case 1:
        return rv::As<records::WindowRecord>(r)->settled_y << 4;
    default:
        return window_task::Word(window_task::At(::bof3::Img(window_task::at::kPlaceXY.v) + Msgbox_Placement * 4u + 2u)) << 4;
    }
}

// The left / top edge's drift while an axis grows or shrinks: half the step, truncated
// toward zero, added on the shrink and subtracted on the grow, so that the
// box stays centred on its settled position.
int HalfStep(const unsigned char* r, unsigned offset) {
    return static_cast<int>(static_cast<std::int16_t>(window_task::Word(r + offset))) / 2;
}

// The four arguments every draw in this family is called with: the top-left and
// the current size, each an arithmetic shift of the 12.4 word. Window_DrawFrame
// and Window_DrawOutline keep only the low 16 bits of the first two and the
// low byte of the last two, so what the original leaves in the upper halves
// of the registers it pushes never reaches either.
int RecX(const unsigned char* r) { return static_cast<std::int16_t>(rv::As<records::WindowRecord>(r)->x) >> 4; }

int RecY(const unsigned char* r) { return static_cast<std::int16_t>(rv::As<records::WindowRecord>(r)->y) >> 4; }

int RecW(const unsigned char* r) { return static_cast<std::int16_t>(rv::As<records::WindowRecord>(r)->width) >> 4; }

int RecH(const unsigned char* r) { return static_cast<std::int16_t>(rv::As<records::WindowRecord>(r)->height) >> 4; }

}  // namespace

// original 0x5954B0: handler 0 of Field_RunTaskRecords' table - the window
// records proper. Byte +2 picks one of three kinds, through a three-entry
// table built on the stack. No bound: ours aborts loudly (rule 4), since
// index 3 of the original's table is the slot its own `add esp, 0xC` frees.
extern "C" void __cdecl Window_Run(void) {
    const unsigned kind = Rec()[2];
    if (kind >= 3) bof3::Fatal("a window record's kind byte is %u; the original's stack table holds three", kind);
    window_task::g.kinds[kind]();
}

// original 0x5954E0: kind 0 - the message box. Its states are the open, the
// grow, the frame, the shrink and the close.
extern "C" void __cdecl Window_Kind0States(void) { RunState(window_task::g.kind0, "kind 0"); }

// original 0x595520: kind 0's state 0. Takes the two high bits off the 0x0C
// placement code into the record's flag byte +0xD - bit 7 means "do not draw
// the frame" (flag 1), bit 6 means "the narrow box" (flag 2) - then sets the
// box's half-size, its top-left (+4, +6) from the placement table, a zero current size,
// and a step of a fifth of the half-size in each axis, so the box opens over
// five frames. Then the state moves on.
//
// As the original has it: the placement byte is written back with the bit
// cleared and read again for each table lookup; the top-left is
// (table x + 0x3C) << 4 for the narrow box and (table x + 0x64) << 4 for the
// wide one, and (table y + 0x1B) << 4 for both; the halving is an ordinary
// signed divide by five of the half-size shifted up four. The record pointer
// is re-read before every store.
extern "C" void __cdecl Window_Kind0Open(void) {
    Rec()[0xD] = 0;
    unsigned code = Msgbox_Placement;
    if ((code & 0x80) != 0) {
        Msgbox_Placement = static_cast<unsigned char>(code & 0x7F);
        Rec()[0xD] |= 1;
    }
    code = Msgbox_Placement;
    unsigned y;
    if ((code & 0x40) != 0) {
        Msgbox_Placement = static_cast<unsigned char>(code & 0xBF);
        Rec()[0xD] |= 2;
        Rec()[0xA] = 0x78;
        Rec()[0xB] = 0x36;
        rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->x, (SideX(window_task::At(::bof3::Img(window_task::at::kBoxAltXY.v))[Msgbox_Placement * 2u]) + 0x3Cu) << 4);
        y = window_task::At(::bof3::Img(window_task::at::kBoxAltXY.v))[Msgbox_Placement * 2u + 1u];
    } else {
        Rec()[0xA] = 0xC8;
        Rec()[0xB] = 0x36;
        rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->x, (SideX(window_task::At(::bof3::Img(window_task::at::kBoxXY.v))[Msgbox_Placement * 2u]) + 0x64u) << 4);
        y = window_task::At(::bof3::Img(window_task::at::kBoxXY.v))[Msgbox_Placement * 2u + 1u];
    }
    rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->y, (y + 0x1Bu) << 4);
    rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->width, 0);
    rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->height, 0);
    unsigned char* r = Rec();
    rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->width_step, static_cast<unsigned>((static_cast<int>(r[0xA]) << 4) / 5));
    r = Rec();
    rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->height_step, static_cast<unsigned>((static_cast<int>(r[0xB]) << 4) / 5));
    ++Rec()[3];
}

// original 0x595670: state 1 of all three kinds - the box growing. Each axis
// grows by its step and its left / top edge moves back half a step; when the
// size passes the settled half-size the edge snaps to the settled place and the
// size to the settled size. With flag 1 clear the outline is drawn at the
// size this frame reached; when BOTH axes settled this frame the state moves
// on - and then without a draw, since the outline is drawn before the test.
//
// As the original has it: the size is compared as `(s16)size >> 4 > (u8)
// half-size`, a 16-bit arithmetic shift against a zero-extended byte, so a
// size that has gone negative never settles; the "settled" counter is a byte
// that state 1 raises to 1 on the x axis and increments on the y, so a state
// that settles only y counts one, not two.
extern "C" void __cdecl Window_Grow(void) {
    unsigned settled = 0;
    unsigned char* r = Rec();
    rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->width, rv::As<records::WindowRecord>(r)->width + rv::As<records::WindowRecord>(r)->width_step);
    r = Rec();
    rv::As<records::WindowRecord>(r)->x = rv::As<records::WindowRecord>(r)->x - static_cast<unsigned>(HalfStep(r, 0x14));
    r = Rec();
    if (RecW(r) > r[0xA]) {
        rv::As<records::WindowRecord>(r)->x = SettledX(r);
        r = Rec();
        settled = 1;
        rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->width, r[0xA] << 4);
        r = Rec();
    }
    rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->height, rv::As<records::WindowRecord>(r)->height + rv::As<records::WindowRecord>(r)->height_step);
    r = Rec();
    rv::As<records::WindowRecord>(r)->y = rv::As<records::WindowRecord>(r)->y - static_cast<unsigned>(HalfStep(r, 0x16));
    r = Rec();
    if (RecH(r) > r[0xB]) {
        rv::As<records::WindowRecord>(r)->y = SettledY(r);
        r = Rec();
        rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->height, r[0xB] << 4);
        r = Rec();
        ++settled;
    }
    if ((r[0xD] & 1) == 0) {
        window_task::g.draw_outline(RecX(r), RecY(r), RecW(r), RecH(r));
        r = Rec();
    }
    if (settled == 2) ++r[3];
}

// original 0x595820: kind 0's state 2 - the open message box. The frame,
// unless flag 1 is set, then the message box's frame task as a tail jump.
// The task's return value becomes this function's; nothing reads it (its
// caller is the kind dispatcher's `call [esp + eax * 4]`).
extern "C" void __cdecl Window_Kind0Frame(void) {
    unsigned char* const r = Rec();
    if ((r[0xD] & 1) == 0) window_task::g.draw_frame(RecX(r), RecY(r), RecW(r), RecH(r));
    window_task::g.msgbox_frame();
}

// original 0x595860: state 3 of all three kinds - the box shrinking, the
// mirror of the grow. Each axis loses its step and its edge moves forward
// half a step; when the size goes negative the edge snaps to the settled
// place PLUS half the box's own half-size - the box closes onto its own
// middle, not onto its left edge - and the size to zero. When both axes are
// done the state moves on and nothing more happens this frame; otherwise the
// outline is drawn (flag 1 clear) and, for kind 0 alone, the message box's
// frame task runs as a tail jump.
//
// As the original has it: the "gone negative" test is `(s16)(size & 0xFFF0)
// < 0`, and the mask cannot change the sign bit, so it is exactly `size < 0`;
// the half-size is halved as an unsigned byte shift, not the signed halving
// the edge's drift uses; kind 1 takes its settled place from the 8-byte
// records at 0x66ADFC by the list set 0x7DEE65, where the grow takes it from
// the record's own +0x18 / +0x1A.
extern "C" void __cdecl Window_Shrink(void) {
    unsigned settled = 0;
    unsigned char* r = Rec();
    rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->width, rv::As<records::WindowRecord>(r)->width - rv::As<records::WindowRecord>(r)->width_step);
    r = Rec();
    rv::As<records::WindowRecord>(r)->x = rv::As<records::WindowRecord>(r)->x + static_cast<unsigned>(HalfStep(r, 0x14));
    r = Rec();
    if (static_cast<std::int16_t>(rv::As<records::WindowRecord>(r)->width) < 0) {
        const unsigned half = r[0xA] >> 1;
        unsigned place;
        switch (r[2]) {
        case 0:
            place = SideX((r[0xD] & 2) != 0 ? window_task::At(::bof3::Img(window_task::at::kBoxAltXY.v))[Msgbox_Placement * 2u]
                                            : window_task::At(::bof3::Img(window_task::at::kBoxXY.v))[Msgbox_Placement * 2u]) + half;
            break;
        case 1:
            place = window_task::Word(window_task::At(::bof3::Img(window_task::at::kSetXY.v) + MenuWindows_ListSet * 8u)) + half;
            break;
        default:
            place = SideX(window_task::Word(window_task::At(::bof3::Img(window_task::at::kPlaceXY.v) + Msgbox_Placement * 4u))) + half;
            break;
        }
        rv::As<records::WindowRecord>(r)->x = place << 4;
        r = Rec();
        settled = 1;
        rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->width, 0);
        r = Rec();
    }
    rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->height, rv::As<records::WindowRecord>(r)->height - rv::As<records::WindowRecord>(r)->height_step);
    r = Rec();
    rv::As<records::WindowRecord>(r)->y = rv::As<records::WindowRecord>(r)->y + static_cast<unsigned>(HalfStep(r, 0x16));
    r = Rec();
    if (static_cast<std::int16_t>(rv::As<records::WindowRecord>(r)->height) < 0) {
        const unsigned half = r[0xB] >> 1;
        unsigned place;
        switch (r[2]) {
        case 0:
            place = ((r[0xD] & 2) != 0 ? window_task::At(::bof3::Img(window_task::at::kBoxAltXY.v))[Msgbox_Placement * 2u + 1u]
                                       : window_task::At(::bof3::Img(window_task::at::kBoxXY.v))[Msgbox_Placement * 2u + 1u]) + half;
            break;
        case 1:
            place = window_task::Word(window_task::At(::bof3::Img(window_task::at::kSetXY.v) + MenuWindows_ListSet * 8u + 2u)) + half;
            break;
        default:
            place = window_task::Word(window_task::At(::bof3::Img(window_task::at::kPlaceXY.v) + Msgbox_Placement * 4u + 2u)) + half;
            break;
        }
        rv::As<records::WindowRecord>(r)->y = place << 4;
        r = Rec();
        ++settled;
        rv::Store<unsigned>(rv::As<records::WindowRecord>(r)->height, 0);
        r = Rec();
    }
    if (settled == 2) {
        ++r[3];
        return;
    }
    if ((r[0xD] & 1) == 0) {
        window_task::g.draw_outline(RecX(r), RecY(r), RecW(r), RecH(r));
        r = Rec();
    }
    if (r[2] != 0) return;
    window_task::g.msgbox_frame();
}

// original 0x595A80: kind 0's state 4 - once the message box's frame task
// answers 0, the record pass byte 0x802D20 goes back to 0 and the record is
// freed (a tail jump). While it answers non-zero nothing happens.
//
// As the original has it: the byte stored to 0x802D20 is the task's own
// zero return, so it is always 0 on that path.
extern "C" void __cdecl Window_Kind0Close(void) {
    if (window_task::g.msgbox_frame() != 0) return;
    BossSa_WindowPass = 0;
    window_task::g.free_current();
}

// original 0x595AA0: kind 1 - the choice list drawn at fixed screen
// coordinates. States 1 and 3 are kind 0's grow and shrink.
extern "C" void __cdecl Window_Kind1States(void) { RunState(window_task::g.kind1, "kind 1"); }

// original 0x595AE0: kind 1's state 0 - the unread set-up at 0x596330, then
// the state on.
extern "C" void __cdecl Window_Kind1Open(void) {
    window_task::g.list_set_up();
    ++Rec()[3];
}

// original 0x595AF0: kind 1's state 2 - the frame unconditionally (flag 1 is
// not consulted here), the unread list draw at 0x596090, then the unread
// cursor draw at 0x596120 as a tail jump.
extern "C" void __cdecl Window_Kind1Frame(void) {
    unsigned char* const r = Rec();
    window_task::g.draw_frame(RecX(r), RecY(r), RecW(r), RecH(r));
    window_task::g.list_draw();
    window_task::g.list_cursor();
}

// original 0x595B30: kinds 1 and 2's state 4 - the record free alone, as a
// five-byte tail jump.
extern "C" void __cdecl Window_FreeState(void) { window_task::g.free_current(); }

// original 0x595B40: kind 2 - the list drawn relative to its own window.
// States 1, 3 and 4 are shared as well.
extern "C" void __cdecl Window_Kind2States(void) { RunState(window_task::g.kind2, "kind 2"); }

// original 0x595B80: kind 2's state 0. A fixed 0x47 x 0x20 box at the
// placement table's (x + 0x23, y + 0x10) << 4 - the same arithmetic
// MsgBox_PlacementTable's note records - opening from zero at steps of 0xE3
// and 0x66. It touches neither the flag byte +0xD nor the placement code's
// two high bits.
extern "C" void __cdecl Window_Kind2Open(void) {
    rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->x, (SideX(window_task::Word(window_task::At(::bof3::Img(window_task::at::kPlaceXY.v) + Msgbox_Placement * 4u))) + 0x23u) << 4);
    rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->y, (window_task::Word(window_task::At(::bof3::Img(window_task::at::kPlaceXY.v) + Msgbox_Placement * 4u + 2u)) + 0x10u) << 4);
    rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->width, 0);
    rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->height, 0);
    rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->width_step, 0xE3);
    rv::Store<unsigned>(rv::As<records::WindowRecord>(Rec())->height_step, 0x66);
    Rec()[0xA] = 0x47;
    Rec()[0xB] = 0x20;
    ++Rec()[3];
}

// original 0x595C10: kind 2's state 2, the same shape with the two
// window-relative draws 0x596020 and 0x5960D0.
extern "C" void __cdecl Window_Kind2Frame(void) {
    unsigned char* const r = Rec();
    window_task::g.draw_frame(RecX(r), RecY(r), RecW(r), RecH(r));
    window_task::g.set_draw();
    window_task::g.set_cursor();
}

// original 0x595C50 (PSX Window_DrawFrame 0x8015A58C - the function the
// sibling's docs/FURIGANA.md measures a box's width from). Six primitives:
//
//   1. a black TILE over (x, y) of (w + 1, h + 1), semi-transparent, under
//      texture page 0xF;
//   2. an 8-byte primitive holding the words 0, 0xF0, 0x10, 0x10, whose
//      ADDRESS is then passed as the next draw mode's texture window;
//   3. a POLY_GT4 of the same corners, white at every vertex, textured
//      (u, v) (0, 0)..(w, h) from page (0, 0, 0x3C0, 0) and the CLUT at
//      ((s8) 0x903A5A * 32 + 0x10, 0x1E1) - the window colour;
//   4. a second 8-byte primitive, words 0, 0, 0x100, 0x100, likewise the
//      texture window of a draw mode under page 5;
//   5. a LINE_F4 in 0xC8C8C8 round (x - 1, y - 1)..(x + w + 1, y + h + 1) -
//      three of the rectangle's four sides, the fourth drawn as a line below;
//   6. five LINE_F2s: the missing left side in 0xC8, then a 0x8C border two
//      pixels out on each of the four sides.
//
// As the original has it: x and y are the low 16 bits of their arguments
// sign-extended, w and h the low byte alone, so the stale upper halves its
// callers push never reach anything; the vertex floats are `fild` of those
// 32-bit sums stored as singles; the packet cursor is read afresh for each
// draw mode and each primitive; the two 8-byte primitives advance the cursor
// by hand, not through Gfx_CommitPrim.
extern "C" void __cdecl Window_DrawFrame(int x_arg, int y_arg, int w_arg, int h_arg) {
    const int x = static_cast<std::int16_t>(x_arg), y = static_cast<std::int16_t>(y_arg);
    const int w = static_cast<int>(static_cast<unsigned>(w_arg) & 0xFFu);
    const int h = static_cast<int>(static_cast<unsigned>(h_arg) & 0xFFu);

    // 1. the black tile
    window_task::g.draw_mode(Gfx_PacketNext, 0, 0, 0xF, 0);
    window_task::g.commit(1, 0xC);
    unsigned char* const tile = Gfx_PacketNext;
    window_task::g.set_tile(tile);
    const float fx = Flt(x), fy = Flt(y);
    PutFloat(tile + 8, fx);
    PutFloat(tile + 0xC, fy);
    PutFloat(tile + 0x14, Flt(w + 1));
    PutFloat(tile + 0x18, Flt(h + 1));
    tile[4] = 0;
    tile[5] = 0;
    tile[6] = 0;
    window_task::g.set_semi(tile, 1);
    window_task::g.commit(1, 0x1C);

    // 2. the texture window of the quad's draw mode
    unsigned char* const window1 = Gfx_PacketNext;
    Gfx_PacketNext = window1 + 8;
    window_task::SetWord(window1 + 0, 0);
    window_task::SetWord(window1 + 2, 0xF0);
    window_task::SetWord(window1 + 6, 0x10);
    window_task::SetWord(window1 + 4, 0x10);
    window_task::g.draw_mode(Gfx_PacketNext, 0, 0, 0xF,
                Low32(window1));
    window_task::g.commit(1, 0xC);

    // 3. the window's textured quad
    unsigned char* const quad = Gfx_PacketNext;
    window_task::g.set_poly_gt4(quad);
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(quad)->tpage, window_task::g.get_tpage(0, 0, 0x3C0, 0));
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(quad)->clut, window_task::g.get_clut(Config_WindowColour * 32 + 0x10, 0x1E1));
    const float fright = Flt(x + w + 1), fbottom = Flt(y + h + 1);
    PutFloat(quad + 8, fx);
    PutFloat(quad + 0xC, fy);
    quad[4] = 0xFF;
    PutFloat(quad + 0x20, fy);
    PutFloat(quad + 0x30, fx);
    quad[5] = 0xFF;
    quad[6] = 0xFF;
    PutFloat(quad + 0x1C, fright);
    quad[0x18] = 0xFF;
    quad[0x19] = 0xFF;
    quad[0x1A] = 0xFF;
    quad[0x2C] = 0xFF;
    quad[0x2D] = 0xFF;
    PutFloat(quad + 0x34, fbottom);
    quad[0x2E] = 0xFF;
    quad[0x40] = 0xFF;
    PutFloat(quad + 0x44, fright);
    quad[0x41] = 0xFF;
    quad[0x42] = 0xFF;
    PutFloat(quad + 0x48, fbottom);
    quad[0x14] = 0;
    quad[0x15] = 0;
    quad[0x28] = static_cast<unsigned char>(w);
    quad[0x29] = 0;
    quad[0x3C] = 0;
    quad[0x3D] = static_cast<unsigned char>(h);
    quad[0x50] = static_cast<unsigned char>(w);
    quad[0x51] = static_cast<unsigned char>(h);
    window_task::g.set_semi(quad, 1);
    window_task::g.commit(1, 0x54);

    // 4. the texture window of the outline's draw mode
    unsigned char* const window2 = Gfx_PacketNext;
    Gfx_PacketNext = window2 + 8;
    window_task::SetWord(window2 + 2, 0);
    window_task::SetWord(window2 + 0, 0);
    window_task::SetWord(window2 + 6, 0x100);
    window_task::SetWord(window2 + 4, 0x100);
    window_task::g.draw_mode(Gfx_PacketNext, 0, 0, 5,
                Low32(window2));
    window_task::g.commit(1, 0xC);

    // 5. three sides of the bright outline
    unsigned char* const outline = Gfx_PacketNext;
    window_task::g.set_line_f4(outline);
    outline[4] = 0xC8;
    outline[5] = 0xC8;
    outline[6] = 0xC8;
    PutFloat(outline + 8, Flt(x - 1));
    PutFloat(outline + 0xC, Flt(y - 1));
    PutFloat(outline + 0x14, fright);
    PutFloat(outline + 0x18, Flt(y - 1));
    PutFloat(outline + 0x20, fright);
    PutFloat(outline + 0x24, fbottom);
    PutFloat(outline + 0x2C, Flt(x - 1));
    PutFloat(outline + 0x30, fbottom);
    window_task::g.commit(1, 0x38);

    // 6. the fourth side, and the dim border two pixels out
    const int right = x + w, bottom = y + h;
    window_task::g.draw_line(x - 1, y - 1, x - 1, bottom + 1, 0xC8);
    window_task::g.draw_line(x - 2, y, x - 2, bottom, 0x8C);
    window_task::g.draw_line(right + 2, y, right + 2, bottom, 0x8C);
    window_task::g.draw_line(x, y - 2, right, y - 2, 0x8C);
    window_task::g.draw_line(x, bottom + 2, right, bottom + 2, 0x8C);
}

// original 0x595F60 (PSX 0x8015A990): the growing or shrinking box - the
// outline alone, in 0x787878, at the same corners Window_DrawFrame's bright
// outline uses: a LINE_F4 of three sides and one LINE_F2 for the fourth. No
// tile, no quad, no draw mode of its own.
extern "C" void __cdecl Window_DrawOutline(int x_arg, int y_arg, int w_arg, int h_arg) {
    unsigned char* const outline = Gfx_PacketNext;
    window_task::g.set_line_f4(outline);
    const int x = static_cast<std::int16_t>(x_arg), y = static_cast<std::int16_t>(y_arg);
    const int w = static_cast<int>(static_cast<unsigned>(w_arg) & 0xFFu);
    const int h = static_cast<int>(static_cast<unsigned>(h_arg) & 0xFFu);
    outline[4] = 0x78;
    outline[5] = 0x78;
    outline[6] = 0x78;
    const float fright = Flt(x + w + 1), fbottom = Flt(y + h + 1);
    PutFloat(outline + 8, Flt(x - 1));
    PutFloat(outline + 0xC, Flt(y - 1));
    PutFloat(outline + 0x14, fright);
    PutFloat(outline + 0x18, Flt(y - 1));
    PutFloat(outline + 0x20, fright);
    PutFloat(outline + 0x24, fbottom);
    PutFloat(outline + 0x2C, Flt(x - 1));
    PutFloat(outline + 0x30, fbottom);
    window_task::g.commit(1, 0x38);
    window_task::g.draw_line(x - 1, y - 1, x - 1, y + h + 1, 0x78);
}

// original 0x596020 (PSX 0x8015AA88): window kind 2's list (Window_Kind2Frame,
// docs/window-task.md): items 0 .. the s8 at 0x7DEE66 inclusive, each through
// Text_DrawImmediate at the current window record's (x >> 4, y >> 4) - s16
// words +4 / +6, shifted arithmetically - plus the set's offsets 0x66AE2C and
// a per-item stride, all 16-bit. The first item's text is the dword at
// 0x7DEE50; each next item's is where Text_DrawImmediate stopped. The
// record, the set and the count are read again for every item.
extern "C" void __cdecl Window_Kind2List() {
    const unsigned char* text = Ptr(::bof3::Img(menu_windows::at::kListText.v));
    if (MenuWindows_ListCount < 0) return;
    for (int i = 0;;) {
        const unsigned set = MenuWindows_ListSet;
        const unsigned char* const record = Ptr(::bof3::Img(menu_windows::at::kCurrent.v));
        const unsigned char* const xy = menu_windows::At(::bof3::Img(menu_windows::at::kSetXY.v) + set * 6);
        const auto dy = static_cast<std::uint16_t>(move_script::Word(xy + 4) * static_cast<std::uint16_t>(i));
        const auto y = static_cast<std::uint16_t>(dy + (S16MenuWindows(rv::As<records::WindowRecord>(record)->y) >> 4) + move_script::Word(xy + 2));
        const auto x = static_cast<std::uint16_t>((S16MenuWindows(rv::As<records::WindowRecord>(record)->x) >> 4) + move_script::Word(xy));
        text = menu_windows::g.text_immediate(x, y, text);
        ++i;
        if (i > MenuWindows_ListCount) break;
    }
}

// original 0x596090 (PSX 0x8015AB60, gap47): kind 1's choices drawn - each of
// choices 0..the s8 0x7DEE66 through Text_DrawImmediate at x 0x3A and its y
// byte (0x7DEE6F + n), the text from 0x7DEE50 and on from where each ended.
// The y word's upper half is the text pointer's, as the original's eax has it.
extern "C" void __cdecl Window_Kind1List(void) {
    U text = MenuWindows_ListText;
    if (MenuWindows_ListCount < 0) return;
    int n = 0;
    do {
        const U y = (text & 0xFFFF0000u) | BRest2f(::bof3::Img(kListRows.v) + static_cast<U>(n));
        text = Key(SH_CALL(Text_DrawImmediate)(0x3A, static_cast<int>(y), move_script::At(text)));
        ++n;
    } while (n <= MenuWindows_ListCount);
}

// original 0x5960D0 (no PSX twin paired; PSX 0x8015ABC8): the window kind 2's cursor, a tail
// jump from its state 2 (0x595C44) - Menu_DrawHand at the window 0x905B84's
// x + 4 and at its y plus the list set's first row and the row stride times
// the cursor's row: record 0x7DEE65 of the 6-byte table 0x66AE2C (+2 the
// first row, +4 the stride), the s8 row 0x7DEE67; the window's +4 / +6 are
// 12.4 words.
//
// As the original has it: the window words are shifted as signed 16 bits
// and the product and sums are 16-bit; the upper halves of both coordinates
// are the registers' leftovers in the original, and Menu_DrawHand reads the
// low words only, so ours passes them zero-extended.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Window_DrawCursor(void) {
    const unsigned set = MenuWindows_ListSet;
    const unsigned char* const rows = mode_states::At(::bof3::Img(mode_states::at::kCursorRows) + set * 6);
    const short row = AreaW0c_ChoiceAnswer;
    const unsigned char* const w = reinterpret_cast<const unsigned char*>(BattleWinStates_Current);
    const unsigned y = (mode_states::Word(rows + 4) * static_cast<unsigned>(row) + static_cast<unsigned>(static_cast<short>(mode_states::Word(w + 6)) >> 4) +
                        mode_states::Word(rows + 2)) & 0xFFFF;
    const unsigned x = (static_cast<unsigned>(static_cast<short>(mode_states::Word(w + 4)) >> 4) + 4) & 0xFFFF;
    mode_states::g.draw_hand(static_cast<int>(x), static_cast<int>(y), 0);
}

// original 0x596120 (PSX 0x8015AC54, gap47): Window_Kind1Frame's tail jump -
// the hand at x (the record's x >> 4, signed) + 4, the cursor's choice's y.
extern "C" void __cdecl Window_Kind1Cursor(void) {
    const int c = AreaW0c_ChoiceAnswer;
    const unsigned char* const r = Cur();
    const U y = BRest2f(::bof3::Img(kListRows.v) + static_cast<U>(c));
    const U hi = c < 0 ? 0xFFFF0000u : 0u;
    const U x = (hi | static_cast<std::uint16_t>(static_cast<short>(rv::As<records::WindowRecord>(r)->x) >> 4)) + 4u;
    SH_CALL(Menu_DrawHand)(static_cast<int>(x), static_cast<int>(y), 0);
}

// original 0x596150 (PSX 0x8015ABA0): one LINE_F2 at the packet cursor, grey
// `colour` in all three channels, its two ends the four arguments taken as
// s16 and converted to singles. Committed to ordering-table slot 1 at 0x20
// bytes.
//
// As the original has it: the colour byte is read before the primitive is
// built, and the four conversions use the caller's fifth argument slot as
// their scratch - which is the slot the colour came out of.
extern "C" void __cdecl Window_DrawLine(int x0, int y0, int x1, int y1, unsigned colour) {
    unsigned char* const line = Gfx_PacketNext;
    window_task::g.set_line_f2(line);
    const auto shade = static_cast<unsigned char>(colour);
    line[4] = shade;
    line[5] = shade;
    line[6] = shade;
    PutFloat(line + 8, Flt(static_cast<std::int16_t>(x0)));
    PutFloat(line + 0xC, Flt(static_cast<std::int16_t>(y0)));
    PutFloat(line + 0x14, Flt(static_cast<std::int16_t>(x1)));
    PutFloat(line + 0x18, Flt(static_cast<std::int16_t>(y1)));
    window_task::g.commit(1, 0x20);
}

// original 0x5961C0, the PSX Text_DrawImmediate's twin (sibling
// TEXT_ENGINE.md: "y += 0x0D (immediate)"): draws `text` at (x, y), one
// Text_DrawAt call a character, and returns one past the NUL that ended it.
// Two callers; the choice lists - yes / no, the camp menu - are drawn by it.
//
//   0x00      end - or, inside a substitution, back to the byte after it
//   0x01      newline: x = the x given, y += 13
//   0x03      substitute the current character's name, up to 9 characters
//   0x04 nn   substitute character record nn's name, up to 9
//   0x05 nn   colour nn        0x06   colour 0
//   0x07 nn   substitute 32-byte record nn, up to 0x20 characters
//   0x08 nn   substitute message nn of the table the script block's dword +4
//             points at, up to 0x10 characters
//   0x09      skipped
//   the rest, 0x02 included: a character, two bytes if the first has bit 7
//
// As the original has it:
//   - one resume pointer and one count, not a stack: a substitution inside a
//     substitution resumes after the INNER control, in the inner string;
//   - the count is of characters drawn, controls are free, and a string that
//     ends by its NUL before the count also ends the substitution;
///   - 0x08 starts the inserted message at its second byte: where the other
//     substitutions load their address less one and then step on to it
//     (lea esi, [eax + 0x904CDF] / inc esi), this one adds the message's
//     offset and steps all the same (add esi, ecx / inc esi, 0x5962AA). By
//     the look of it a slip; whether any shipped text reaches it is unknown.
//     Found by the fuzz, not by the read;
//   - the colour handed to Text_DrawAt is a byte; the original pushes the
//     dword around it, three bytes of stale stack that Text_DrawString never
//     reads. Ours passes them as zero.
//
// DIVERGENCE DIV-0006: x advances by the glyph's entry in the overlay font's
// advance table where the original adds 12. With no table loaded,
// TextAdvance_Of is 12 and this is the original.
extern "C" const unsigned char* __cdecl Text_DrawImmediate(int x_arg, int y, const unsigned char* text) {
    int x = x_arg;
    bool substituting = false;
    const unsigned char* resume = nullptr;
    std::uint8_t left = 0, colour = 0;

    auto substitute = [&](const unsigned char* control_end, const std::uint8_t* with, std::uint8_t count) {
        substituting = true;
        resume = control_end;   // the last byte of the control: text goes on one past it
        left = count;
        text = with;
    };

    for (;;) {
        const std::uint8_t c = text[0];
        switch (c) {
        case 0x00:
            if (!substituting) return text + 1;
            text = resume + 1;
            substituting = false;
            continue;
        case 0x01:
            x = x_arg;
            y += kLine;
            ++text;
            continue;
        case 0x03:
            substitute(text, AtTextImmediate(::bof3::Img(bof3::addr::Text_CurrentName)), 9);
            continue;
        case 0x04:
            substitute(text + 1, AtTextImmediate(::bof3::Img(CharacterRecords_addr)) + text[1] * kCharacterRecord, 9);
            continue;
        case 0x05:
            colour = text[1];
            text += 2;
            continue;
        case 0x06:
            colour = 0;
            ++text;
            continue;
        case 0x07:
            substitute(text + 1, AtTextImmediate(::bof3::Img(bof3::addr::Text_Records)) + text[1] * kTextRecord, 0x20);
            continue;
        case 0x08: {
            std::uint32_t table_at;
            std::memcpy(&table_at, AtTextImmediate(::bof3::Img(bof3::addr::MessagePools)) + 4, sizeof table_at);
            const std::uint8_t* table = AtTextImmediate(::bof3::Img(bof3::addr::MessagePools)) + table_at;
            std::uint16_t off;
            std::memcpy(&off, table + 2 * text[1], sizeof off);
            substitute(text + 1, table + off + 1, 0x10);  // from its SECOND byte: see above
            continue;
        }
        case 0x09:
            ++text;
            continue;
        default:
            break;
        }

        text_immediate::g_draw_at(x, y, colour, 1, text);
        const int advance = TextAdvance_Of(text);  // DIV-0006; 12 in the original
        if (text[0] & 0x80) ++text;
        if (substituting && --left == 0) {
            text = resume;
            substituting = false;
        }
        x += advance;
        ++text;
    }
}

// original 0x596330: called by Window_Kind1Open - kind 1's layout. The
// choices' text is walked (0 ends a choice and counts a line, 1 counts a line,
// 4 5 7 8 9 take a byte more, 2 3 6 none, a byte from 0x80 a byte more), each
// choice's first line kept in 0x7DEE6F + n. The record (0x905B84): +0xA 0xDC
// wide, +0xB (last + 4) 2 + 13 lines high (bytes); x (+4) = (0xDC / 2 + 0x30)
// << 4, y (+6) = (+0xB / 2 - 13 lines / 2 + 0x78) << 4; +0x18 0x30, +0x1A 0x78
// - (last + 13 lines) / 2 (toward zero); +0x10, +0x12 0; +0x14 / +0x16 the
// width / height * 16 / 5. Then each choice's y = 13 line + 2 n + 0x7C - (last
// + 13 lines) / 2, bytes.
extern "C" void __cdecl Window_Kind1Layout(void) {
    unsigned char lines = 0;
    if (MenuWindows_ListCount >= 0) {
        const unsigned char* text = move_script::At(MenuWindows_ListText);
        unsigned char n = 0;
        do {
            BRest2f(::bof3::Img(kListRows.v) + n) = lines;
            for (bool more = true; more; ++text) {
                const unsigned char c = *text;
                switch (c) {
                case 0:
                    more = false;
                    ++lines;
                    break;
                case 1: ++lines; break;
                case 4:
                case 5:
                case 7:
                case 8:
                case 9: ++text; break;
                case 2:
                case 3:
                case 6: break;
                default:
                    if (c & 0x80) ++text;
                    break;
                }
            }
            ++n;
        } while (static_cast<int>(n) <= MenuWindows_ListCount);
    }
    unsigned char* r = Cur();
    r[0xA] = 0xDC;
    r = Cur();
    r[0xB] = static_cast<unsigned char>((static_cast<unsigned char>(MenuWindows_ListCount) + 4) * 2 + static_cast<unsigned char>(lines * 13));
    r = Cur();
    rv::As<records::WindowRecord>(r)->x = ((r[0xA] >> 1) + 0x30u) << 4;
    const U n13 = 13u * lines;
    r = Cur();
    rv::As<records::WindowRecord>(r)->y = ((r[0xB] >> 1) - (n13 >> 1) + 0x78u) << 4;
    rv::Store<std::uint32_t>(rv::As<records::WindowRecord>(Cur())->settled_x, 0x30);
    {
        const int sum = MenuWindows_ListCount + static_cast<int>(n13);
        rv::Store<std::uint32_t>(rv::As<records::WindowRecord>(Cur())->settled_y, static_cast<U>(0x78 - sum / 2));
    }
    rv::Store<std::uint32_t>(rv::As<records::WindowRecord>(Cur())->width, 0);
    rv::Store<std::uint32_t>(rv::As<records::WindowRecord>(Cur())->height, 0);
    r = Cur();
    rv::As<records::WindowRecord>(r)->width_step = (static_cast<U>(r[0xA]) << 4) / 5;
    r = Cur();
    rv::As<records::WindowRecord>(r)->height_step = (static_cast<U>(r[0xB]) << 4) / 5;
    const int last = MenuWindows_ListCount;
    const auto top = static_cast<unsigned char>(0x7C - static_cast<unsigned char>((last + static_cast<int>(n13)) / 2));
    if (last < 0) return;
    unsigned char add = 0, n = 0;
    do {
        const unsigned char v = BRest2f(::bof3::Img(kListRows.v) + n);
        BRest2f(::bof3::Img(kListRows.v) + n) = static_cast<unsigned char>(static_cast<unsigned char>(v * 13) + add + top);
        add = static_cast<unsigned char>(add + 2);
        ++n;
    } while (static_cast<int>(n) <= MenuWindows_ListCount);
}

// original 0x596530 (in no list; Field_RunTaskRecords' stack table, the imm32
// at 0x59E245): jmp through Window_Handler1KindTable by the record's +2,
// unchecked (ours aborts past its 4).
extern "C" void __cdecl Window_Handler1Kinds(void) {
    Run("Window_Handler1Kinds (0x596530)", Window_Handler1KindTable, Window_Handler1KindTable_count, Cur()[2],
        "the window record's +2");
}

// original 0x596550: Window_Handler1KindTable[1] - its state (Win1_TitleStripStates
// by +3, a call, unchecked), the title box (x, y, 0x118 x 0x13, the window
// colour), and while the word +0x10 is not 0 the MessagePools string it indexes
// (s16) at (x + 7, y + 3), colour 0, count 0xFF.
extern "C" void __cdecl Win1_TitleStrip(void) {
    Run("Win1_TitleStrip (0x596550)", Win1_TitleStripStates, Win1_TitleStripStates_count, Cur()[3],
        "the window record's +3");
    const unsigned char colour = static_cast<unsigned char>(Config_WindowColour);
    const unsigned char* r = Cur();
    SH_CALL(Menu_DrawTitleBox)(Wx(r), Wy(r), 0x118, 0x13, colour);
    r = Cur();
    const U v = move_script::Word(r + 0x10);
    if (v == 0) return;
    const U at = ::bof3::Img(kPools.v) + 2u * static_cast<U>(static_cast<int>(static_cast<short>(v)));
    const unsigned char* const text = move_script::At(::bof3::Img(kPools.v) + WRest2f(at));
    SH_CALL(Text_DrawAt)(static_cast<int>((rv::As<records::WindowRecord>(r)->x + 7u) & 0xFFFF), static_cast<int>((rv::As<records::WindowRecord>(r)->y + 3u) & 0xFFFF), 0, 0xFF,
                         text);
}

// original 0x5965D0: Window_Handler1KindTable[2] - its state (Win1_ButtonRowStates
// by +3), then Menu_DrawButtonRow(x, y, set +0xA, selected +0xB, 0).
extern "C" void __cdecl Win1_ButtonRow(void) {
    Run("Win1_ButtonRow (0x5965D0)", Win1_ButtonRowStates, Win1_ButtonRowStates_count, Cur()[3], "the window record's +3");
    const unsigned char* const r = Cur();
    SH_CALL(Menu_DrawButtonRow)(Wx(r), Wy(r), r[0xA], r[0xB], 0);
}

// original 0x596610: Window_Handler1KindTable[3] - its state (Win1_ShisuPanelStates
// by +3), then the panel (Win1_DrawShisuPanel) of the record.
extern "C" void __cdecl Win1_ShisuPanel(void) {
    Run("Win1_ShisuPanel (0x596610)", Win1_ShisuPanelStates, Win1_ShisuPanelStates_count, Cur()[3],
        "the window record's +3");
    SH_CALL(Win1_DrawShisuPanel)(Cur());
}

// original 0x596630: called by Win1_ShisuPanel - a 0x95 x 0xA0 box at (x + 3,
// y + 3); four rows, row i (y + 13 (i + 2)) an item row of category 0, id
// 0x66AE8C[i], count the byte i of the record's +0x20 list - colour 7 and dim
// where that byte is 0; the row at +0xA (the cursor) raised 2 over a colour-7
// dim shadow (none for a 0 byte); the title string 0x66AE90 at x + 6 (13 -
// its bytes), y + 7, count 0x10; the two piece lists 0x66AE74 / 0x66AE80; the
// frame's pieces.
extern "C" void __cdecl Win1_DrawShisuPanel(unsigned char* rec) {
    SH_CALL(Menu_DrawBox)(static_cast<int>((rv::As<records::WindowRecord>(rec)->x + 3u) & 0xFFFF), static_cast<int>((rv::As<records::WindowRecord>(rec)->y + 3u) & 0xFFFF), 0x95,
                          0xA0, 0, static_cast<unsigned char>(Config_WindowColour));
    for (unsigned char i = 0; i < 4; ++i) {
        const unsigned char held = move_script::At(static_cast<U>(move_script::Long(rec + 0x20)))[i];
        const unsigned colour = held != 0 ? 0 : 7;
        const unsigned dim = held != 0 ? 0 : 1;
        if (i != rec[0xA]) {
            Row(Wx(rec) + 7, (13u * (i + 2u) + rv::As<records::WindowRecord>(rec)->y) & 0xFFFF, colour, 0, BRest2f(::bof3::Img(kShisuIds.v) + i), held, dim);
            continue;
        }
        if (held != 0) Row(Wx(rec) + 7, (13u * (i + 2u) + rv::As<records::WindowRecord>(rec)->y) & 0xFFFF, 7, 0, BRest2f(::bof3::Img(kShisuIds.v) + i), held, 1);
        const unsigned char again = move_script::At(static_cast<U>(move_script::Long(rec + 0x20)))[i];
        Row(Wx(rec) + 7, ((13u * i + rv::As<records::WindowRecord>(rec)->y) & 0xFFFF) + 0x18u, colour, 0, BRest2f(::bof3::Img(kShisuIds.v) + i), again, dim);
    }
    {
        unsigned n = 0;
        while (BRest2f(::bof3::Img(kShisuTitle.v) + n) != 0) ++n;
        const int x = static_cast<int>((6u * (13u - n) + rv::As<records::WindowRecord>(rec)->x) & 0xFFFF);
        SH_CALL(Text_DrawAt)(x, static_cast<int>((rv::As<records::WindowRecord>(rec)->y + 7u) & 0xFFFF), 0, 0x10, move_script::At(::bof3::Img(kShisuTitle.v)));
    }
    SH_CALL(Menu_DrawPieces)(Wx(rec), Wy(rec), move_script::At(::bof3::Img(kShisuPiecesA.v)), 1);
    SH_CALL(Menu_DrawPieces)(Wx(rec), Wy(rec), move_script::At(::bof3::Img(kShisuPiecesB.v)), 1);
    for (unsigned i = 0; i < 10; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 0x28), Wy(rec), 1);
    for (unsigned i = 0; i < 17; ++i) Piece(Wx(rec), Wy(rec) + static_cast<int>(8 * i + 0x18), 4);
    Piece(Wx(rec) + 0x90, Wy(rec) + 0x18, 0x20);
    for (unsigned i = 0; i < 15; ++i) Piece(Wx(rec) + 0x90, Wy(rec) + static_cast<int>(8 * i + 0x20), 0x21);
    Piece(Wx(rec), Wy(rec) + 0xA0, 5);
    Piece(Wx(rec) + 0x90, Wy(rec) + 0x98, 0x22);
    Piece(Wx(rec) + 8, Wy(rec), 0x32);
    Piece(Wx(rec) + 0x90, Wy(rec), 0x34);
    for (unsigned i = 0; i < 17; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 8), Wy(rec) + 0xA0, 6);
}

// original 0x5968E0 (Field_RunTaskRecords' stack table, the imm32 at 0x59E250):
// jmp through Window_Handler2KindTable by the record's +2, unchecked (ours
// aborts past its 5).
extern "C" void __cdecl Window_Handler2Kinds(void) {
    Run("Window_Handler2Kinds (0x5968E0)", Window_Handler2KindTable, Window_Handler2KindTable_count, Cur()[2],
        "the window record's +2");
}

// original 0x596900: Window_Handler2KindTable[0] - its state (Win2_ItemListStates
// by +3), then the item list (Win2_DrawItemList) of the record.
extern "C" void __cdecl Win2_ItemList(void) {
    Run("Win2_ItemList (0x596900)", Win2_ItemListStates, Win2_ItemListStates_count, Cur()[3], "the window record's +3");
    SH_CALL(Win2_DrawItemList)(Cur());
}

// original 0x596920: a slide state (Win2_ItemListStates[1], Win2_EquipCompareStates[1],
// and two other handlers' tables, 0x66B2E8 and 0x66B570): x - 0x20 a frame to
// the bound of its `mov ecx, -170` (s16), held there with +3 0; ours reads
// widescreen.cpp's value for it (g_slides). DIVERGENCE DIV-0041 (amended
// 2026-10-13, the faster slide): decided by that bound at every width, the box
// drawn to the wide one (Widescreen_SlideHeld, docs/ultrawide-slides.md).
extern "C" void __cdecl MenuSlide_LeftOff170(void) {
    const short bound = Widescreen_SlideLogic(Widescreen_SlideBound(kLeftOff170Bound.v));
    unsigned char* const moved = Cur();
    const short x_read = static_cast<short>(rv::As<records::WindowRecord>(moved)->x);
    rv::As<records::WindowRecord>(moved)->x = rv::As<records::WindowRecord>(moved)->x + 0xFFE0u;
    unsigned char* const r = Cur();
    if (static_cast<short>(rv::As<records::WindowRecord>(r)->x) < bound) {
        rv::As<records::WindowRecord>(r)->x = static_cast<std::uint16_t>(bound);
        Cur()[3] = 0;
    }
    Widescreen_SlideHeld(Cur(), x_read, bound);
}

// original 0x596950: Win2_ItemListStates[2]: x + 0x20 a frame to 0x50 (s16),
// held there with +3 0.
extern "C" void __cdecl MenuSlide_RightTo80(void) {
    unsigned char* const moved = Cur();
    rv::As<records::WindowRecord>(moved)->x = rv::As<records::WindowRecord>(moved)->x + 0x20u;
    unsigned char* const r = Cur();
    if (static_cast<short>(rv::As<records::WindowRecord>(r)->x) > 0x50) {
        rv::As<records::WindowRecord>(r)->x = 0x50;
        Cur()[3] = 0;
    }
}

// original 0x596980: Window_Handler2KindTable[2] - its state
// (Win2_ItemPanelStates by +3), then Menu_DrawItemPanel(the record).
extern "C" void __cdecl Win2_ItemPanel(void) {
    Run("Win2_ItemPanel (0x596980)", Win2_ItemPanelStates, Win2_ItemPanelStates_count, Cur()[3], "the window record's +3");
    SH_CALL(Menu_DrawItemPanel)(Cur());
}

// original 0x5969A0: Window_Handler2KindTable[3] - its state
// (Win2_EquipCompareStates by +3), then Menu_DrawEquipCompare(7, x, y, the
// record's +0x20, +0xD, the record).
extern "C" void __cdecl Win2_EquipCompare(void) {
    Run("Win2_EquipCompare (0x5969A0)", Win2_EquipCompareStates, Win2_EquipCompareStates_count, Cur()[3],
        "the window record's +3");
    unsigned char* const r = Cur();
    SH_CALL(Menu_DrawEquipCompare)(7, Wx(r), Wy(r), move_script::At(static_cast<U>(move_script::Long(r + 0x20))), r[0xD], r);
}

// original 0x5969E0: Window_Handler2KindTable[4] - its state
// (Win2_TitleBoxStates by +3), the title box (x, y, the word +0x12 wide, 0x13
// high, the window colour), and while the word +0x10 is not 0 the system
// message it names (Msg_SystemPtr) at (x + 7, y + 3), colour 0, count 0xFF.
extern "C" void __cdecl Win2_TitleBox(void) {
    Run("Win2_TitleBox (0x5969E0)", Win2_TitleBoxStates, Win2_TitleBoxStates_count, Cur()[3], "the window record's +3");
    const unsigned char colour = static_cast<unsigned char>(Config_WindowColour);
    const unsigned char* r = Cur();
    SH_CALL(Menu_DrawTitleBox)(Wx(r), Wy(r), static_cast<int>(move_script::Word(r + 0x12)), 0x13, colour);
    r = Cur();
    const U v = move_script::Word(r + 0x10);
    if (v == 0) return;
    const unsigned char* const text = SH_CALL(Msg_SystemPtr)(v);
    r = Cur();
    SH_CALL(Text_DrawAt)(static_cast<int>((rv::As<records::WindowRecord>(r)->x + 7u) & 0xFFFF), static_cast<int>((rv::As<records::WindowRecord>(r)->y + 3u) & 0xFFFF), 0, 0xFF,
                         text);
}

// original 0x596A60: Win2_TitleBoxStates[3]: y + 0x10 a frame to 0x28 (s16),
// held there with +3 0.
extern "C" void __cdecl MenuSlide_DownTo40(void) {
    unsigned char* const moved = Cur();
    rv::As<records::WindowRecord>(moved)->y = rv::As<records::WindowRecord>(moved)->y + 0x10u;
    unsigned char* const r = Cur();
    if (static_cast<short>(rv::As<records::WindowRecord>(r)->y) > 0x28) {
        rv::As<records::WindowRecord>(r)->y = 0x28;
        Cur()[3] = 0;
    }
}

// original 0x596A90: called by Win2_ItemList - the item list window, the
// shape of Menu_DrawItemList's (menu_windows.cpp) with nine rows: the box;
// Menu_ListScroll (top +0xB, its offset kept in the record pointer argument's
// own low byte as the original keeps it - ours a local -, moving, state +0x12);
// moving + 9 rows of category +0xA's ids and counts from the top: dim by
// Item_CanUse(+8, the shown member, category, id) unless +9 (then all dim),
// colour 7 dim / 0, 2 on the cursor +0xD; the count 1 for category 4; the
// cursor's and the marked (+0xC) row raised 2 over a colour-7 shadow (none when
// dim); the title and footer boxes; the category's title (0x66AF10) at x + 6 (13
// - Text_CharCount) - the call DIV-0059 re-aims; "%3d/%3d" of
// Inventory_CountUsed and the room (0x20 for category 4, else 0x80) in the
// 8-px font; the arrows by +0x10's bits 1 and 0, its high nibble counted down
// (or the word cleared); the frame's pieces; the scroll bar over 9 rows.
extern "C" void __cdecl Win2_DrawItemList(unsigned char* rec) {
    SH_CALL(Menu_DrawBox)(static_cast<int>((rv::As<records::WindowRecord>(rec)->x + 3u) & 0xFFFF), static_cast<int>((rv::As<records::WindowRecord>(rec)->y + 3u) & 0xFFFF), 0x99,
                          0x9A, rec[9], static_cast<unsigned char>(Config_WindowColour));
    unsigned char offset = static_cast<unsigned char>(Key(rec));
    unsigned char moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(rec + 0xB, &offset, &moving, rec + 0x12);
    const unsigned rows = moving + 9u;
    const U category = rec[0xA];
    const unsigned char* ids = move_script::At(static_cast<U>(move_script::Long(move_script::At(::bof3::Img(kIdLists.v) + 4 * category)))) + top;
    const unsigned char* counts = move_script::At(static_cast<U>(move_script::Long(move_script::At(::bof3::Img(kCountLists.v) + 4 * category)))) + top;
    U y = static_cast<std::uint16_t>(static_cast<signed char>(offset) + rv::As<records::WindowRecord>(rec)->y) + 0x1Au;
    for (unsigned char i = 0; i < rows; ++i, ++ids, ++counts, y += 0xD) {
        const unsigned char item = *ids;
        if (item == 0) continue;
        unsigned char dim = 1;
        if (rec[9] == 0) {
            const unsigned char who = BRest2f(::bof3::Img(Cond_Flags_addr) + 0xd2 + static_cast<std::uint32_t>(BattleActorCopies_ItemActor));
            const unsigned char member = BRest2f(::bof3::Img(kEffectState.v) + who);
            dim = SH_CALL(Item_CanUse)(rec[8], member, rec[0xA], item) ? 0 : 1;
        }
        unsigned char colour = dim ? 7 : 0;
        const U here = static_cast<U>(i) + rec[0xB];
        const U cursor = rec[0xD];
        if (here == cursor) colour = 2;
        const unsigned char cat = rec[0xA];
        const unsigned char count = cat == 4 ? 1 : *counts;
        if (here != cursor && here != rec[0xC]) {
            Row(Wx(rec) + 7, y, colour, cat, *ids, count, dim);
            continue;
        }
        if (colour != 7) Row(Wx(rec) + 7, y, 7, cat, *ids, count, 1);
        Row(Wx(rec) + 7, y - 2, colour, rec[0xA], *ids, count, dim);
    }
    SH_CALL(Menu_DrawBox)(static_cast<int>((rv::As<records::WindowRecord>(rec)->x + 3u) & 0xFFFF), static_cast<int>((rv::As<records::WindowRecord>(rec)->y + 3u) & 0xFFFF), 0x99,
                          0x14, rec[9], static_cast<unsigned char>(Config_WindowColour));
    {
        const unsigned char colour = static_cast<unsigned char>(Config_WindowColour);
        const unsigned char flags = rec[9];
        SH_CALL(Menu_DrawBox)(static_cast<int>((rv::As<records::WindowRecord>(rec)->x + 3u) & 0xFFFF), static_cast<int>((rv::As<records::WindowRecord>(rec)->y + 0x92u) & 0xFFFF),
                              0x99, 8, flags, colour);
    }
    {
        const U cat = rec[0xA];
        const unsigned grey = rec[9] ? 7 : 0;
        const int y7 = static_cast<int>((rv::As<records::WindowRecord>(rec)->y + 7u) & 0xFFFF);
        const unsigned char* const title = move_script::At(static_cast<U>(move_script::Long(move_script::At(::bof3::Img(kListTitles.v) + 4 * cat))));
        const unsigned n = SH_CALL(Text_CharCount)(title);
        const int x = static_cast<int>((6u * (13u - n) + rv::As<records::WindowRecord>(rec)->x) & 0xFFFF);
        const U site = rest_2f::g_title_call;
        if (site == 0 || site == bof3::addr::Text_DrawAt) SH_CALL(Text_DrawAt)(x, y7, static_cast<int>(grey), 0x10, title);
        else reinterpret_cast<TitleFn>(static_cast<std::uintptr_t>(::bof3::CodePlace(site)))(x, y7, static_cast<int>(grey), 0x10, title);
    }
    {
        const unsigned char cat = rec[0xA];
        const U room = cat == 4 ? 0x20 : 0x80;
        const unsigned char used = SH_CALL(Inventory_CountUsed)(cat);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(move_script::At(::bof3::Img(kPrint.v))), reinterpret_cast<const char*>(move_script::At(::bof3::Img(kUsedFormat.v))),
                             static_cast<U>(used), room);
        const unsigned grey = rec[9] ? 7 : 0;
        SH_CALL(Text_DrawFont8)(static_cast<int>((rv::As<records::WindowRecord>(rec)->x + 0x55u) & 0xFFFF), static_cast<int>((rv::As<records::WindowRecord>(rec)->y + 0x92u) & 0xFFFF),
                                static_cast<int>(grey), move_script::At(::bof3::Img(kPrint.v)));
    }
    SH_CALL(Menu_DrawPieces)(Wx(rec), Wy(rec), move_script::At(rec[0x10] & 2 ? ::bof3::Img(kListPieces2.v) : ::bof3::Img(kListPieces0.v)), 1);
    SH_CALL(Menu_DrawPieces)(Wx(rec), Wy(rec), move_script::At(rec[0x10] & 1 ? ::bof3::Img(kListPieces3.v) : ::bof3::Img(kListPieces1.v)), 1);
    {
        const U w = rv::As<records::ItemListWindow>(rec)->arrows;
        if ((w & 0xF0) == 0) rv::As<records::ItemListWindow>(rec)->arrows = 0;
        else rv::As<records::ItemListWindow>(rec)->arrows = w - 0x10;
    }
    for (unsigned i = 0; i < 10; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 0x28), Wy(rec), 1);
    for (unsigned i = 0; i < 15; ++i) Piece(Wx(rec), Wy(rec) + static_cast<int>(8 * i + 0x18), 4);
    Piece(Wx(rec) + 0x90, Wy(rec) + 0x18, 0x16);
    for (unsigned i = 0; i < 12; ++i) Piece(Wx(rec) + 0x90, Wy(rec) + static_cast<int>(8 * i + 0x28), 0x17);
    Piece(Wx(rec) + 0x90, Wy(rec) + 0x88, 0x18);
    Piece(Wx(rec), Wy(rec) + 0x90, 0x19);
    for (unsigned i = 0; i < 9; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 8), Wy(rec) + 0x90, 0x1A);
    Piece(Wx(rec) + 0x50, Wy(rec) + 0x90, 0x1B);
    for (unsigned i = 0; i < 6; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 0x58), Wy(rec) + 0x90, 0x1C);
    Piece(Wx(rec) + 0x88, Wy(rec) + 0x90, 0x1D);
    {
        const U cat = rec[0xA];
        const U total = cat == 4 ? 0x20 : 0x80;
        SH_CALL(Menu_DrawScrollBar)(move_script::At(static_cast<U>(move_script::Long(move_script::At(::bof3::Img(kIdLists.v) + 4 * cat)))), rec[0xB], Wx(rec) + 0x90, Wy(rec) + 0x18, 9,
                                    total, 0x74);
    }
}

void Window_Inject() {
    BOF3_INJECT(Window_Run);
    BOF3_INJECT(Window_Kind0States);
    BOF3_INJECT(Window_Kind0Open);
    BOF3_INJECT(Window_Grow);
    BOF3_INJECT(Window_Kind0Frame);
    BOF3_INJECT(Window_Shrink);
    BOF3_INJECT(Window_Kind0Close);
    BOF3_INJECT(Window_Kind1States);
    BOF3_INJECT(Window_Kind1Open);
    BOF3_INJECT(Window_Kind1Frame);
    BOF3_INJECT(Window_FreeState);
    BOF3_INJECT(Window_Kind2States);
    BOF3_INJECT(Window_Kind2Open);
    BOF3_INJECT(Window_Kind2Frame);
    BOF3_INJECT(Window_DrawFrame);
    BOF3_INJECT(Window_DrawOutline);
    BOF3_INJECT(Window_Kind2List);
    BOF3_INJECT(Window_Kind1List);
    BOF3_INJECT(Window_DrawCursor);
    BOF3_INJECT(Window_Kind1Cursor);
    BOF3_INJECT(Window_DrawLine);
    BOF3_INJECT(Text_DrawImmediate);
    BOF3_INJECT(Window_Kind1Layout);
    BOF3_INJECT(Window_Handler1Kinds);
    BOF3_INJECT(Win1_TitleStrip);
    BOF3_INJECT(Win1_ButtonRow);
    BOF3_INJECT(Win1_ShisuPanel);
    BOF3_INJECT(Win1_DrawShisuPanel);
    BOF3_INJECT(Window_Handler2Kinds);
    BOF3_INJECT(Win2_ItemList);
    BOF3_INJECT(MenuSlide_LeftOff170);
    BOF3_INJECT(MenuSlide_RightTo80);
    BOF3_INJECT(Win2_ItemPanel);
    BOF3_INJECT(Win2_EquipCompare);
    BOF3_INJECT(Win2_TitleBox);
    BOF3_INJECT(MenuSlide_DownTo40);
    BOF3_INJECT(Win2_DrawItemList);
}
