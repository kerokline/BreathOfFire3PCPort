// Moved here by tools/move_funcs.py from item_use.cpp, menu_windows.cpp, mode_states.cpp, msg_pool.cpp, msgbox.cpp (docs/source-layout-round.md).
#include "game/text/msg_box.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/button_text.h"
#include "game/dat_load.h"
#include "game/item_use_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/menu_verbs.h"
#include "game/menu_windows.h"
#include "game/menu_windows_callees.h"
#include "game/mode_states_callees.h"
#include "game/move_script_bytes.h"
#include "game/msg_pool.h"
#include "game/msgbox.h"
#include "game/msgbox_callees.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/shoulder_glyphs.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

using namespace item_use;

using namespace menu_windows;

using namespace mode_states;

using namespace msgbox;

namespace item_use {

using move_script::At;

using move_script::SetWord;

using move_script::Word;

}  // namespace item_use

namespace {

// Choice ids below 0x80 through the area's own table, the rest through
// MsgBox_SystemChoice. The area descriptor is re-read for each call, as in
// the original: Game_AreaNumber zero-extended, the id's low byte.
void CommitChoice() {
    const unsigned id = ItemUse_ChoiceId;
    if (id < 0x80) {
        const unsigned char* const desc = Area_Descriptors[Game_AreaNumber];
        const auto table = reinterpret_cast<const ::bof3::CodeSlot<Choice>*>(static_cast<std::uintptr_t>(move_script::Long(desc + 0x34)));   // 32-bit code words
        table[id]();
    } else {
        item_use::g.system_choice();
    }
}

// The message word from the id's pair at 0x658F08 + 4 * k, by the cursor
// read as s8 (the original's movsx): a cursor outside 0..1 reads the next
// pair or the one before, as the original does. Returns the cursor.
int SysMessage(unsigned k) {
    const int cursor = AreaW0c_ChoiceAnswer;
    item_use::SetWord(item_use::At(::bof3::Img(item_use::at::kMessage.v)), item_use::Word(item_use::At(::bof3::Img(item_use::at::kSysMessages.v) + 4 * k + 2 * cursor)));
    return cursor;
}

void SysCounter(unsigned k, unsigned char if0, unsigned char if1) {
    const int cursor = SysMessage(k);
    if (cursor == 0) item_use::At(::bof3::Img(item_use::at::kSysCounter.v))[0] = if0;
    else if (cursor == 1) item_use::At(::bof3::Img(item_use::at::kSysCounter.v))[0] = if1;
}

}  // namespace

namespace menu_windows {

using move_script::At;

}  // namespace menu_windows

namespace {

using move_script::SetWord;

std::uint32_t Addr(const void* p) { return Low32(p); }

unsigned char* Next() { return Gfx_PacketNext; }

std::int16_t S16MenuWindows(std::uint32_t v) { return static_cast<std::int16_t>(v); }

// `fild dword` then `fstp dword`: an int rounded once to a float.
float F(std::int32_t v) { return static_cast<float>(v); }

void Put(unsigned char* at, float f) { std::memcpy(at, &f, sizeof f); }

std::uint32_t Address(const void* p) { return Low32(p); }

}  // namespace

namespace msgbox {

namespace {

// The original's `call dword ptr [esp + eax*4]` has no bound: a byte above
// the table's length calls whatever lies past the dispatcher's own stack
// frame. Every writer of each index byte is enumerated in docs/msgbox.md
// section 2 and none can leave the range, so this refuses instead - loudly,
// and in the one configuration the original would have crashed in.
void Call(void (__cdecl* const* table)(), unsigned n, unsigned index, const char* who) {
    if (index >= n) bof3::Fatal("msgbox: %s reached index %u of %u", who, index, n);
    table[index]();
}

// The most glyphs an instant page prints in one frame before it lets the
// frame end (and goes on in the next): a message with no stop code would
// otherwise hold the loop, as an unclosed 0x10 span does in the original
// (docs/msgbox.md section 6). A shipped page is a few hundred at most.
constexpr unsigned kInstantGlyphsPerFrame = 1024;

// The pacing floor (DIV-0158's amendment, issue 121): the glyphs an instant
// page has printed since its last stop code. On the original's Fast a glyph
// is a frame, so a page of N glyphs cannot reach its stop code - and a press
// cannot close it - before N frames; area 99's lift waits on exact script
// counts the dialogue advances, and three pages closed faster than that
// leave it waiting for ever (docs/issue-121-lift-stall.md). At the stop
// code the printer first waits these frames out (state 1), then takes the
// code: the page is read at once and closes no earlier than it could on
// Fast. Reset with the box.
unsigned g_instant_page = 0;

}  // namespace

}  // namespace msgbox

// menu_windows's callee seam, moved here from menu_windows.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace menu_windows {
namespace {

template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Text_DrawAt,
    Text_DrawAt,
    Text_DrawImmediate,
    Msg_SystemPtr,
    Msg_SystemPtr,
    MsgBox_Reset,
    reinterpret_cast<void (__cdecl*)(unsigned)>(reinterpret_cast<void*>(&Sound_PlayEffect)),
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_SetSprt,
    Gpu_SetSemiTrans,
    Gpu_GetTPage,
    Gpu_GetClut,
    Gpu_SetPolyFT4,
    Gpu_SetLineF2,
    Fn<void (__cdecl*)(unsigned char*)>(::bof3::Code(kSetLineF3.v)),
    Gpu_SetLineF4,
    Gpu_SetTile,
    reinterpret_cast<unsigned char (__cdecl*)(int, int)>(reinterpret_cast<void*>(&AreaMap_ByteAt)),
    Crt_sprintf,
    Fn<void (__cdecl*)(unsigned, int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kStatIcon.v)),
    Fn<void (__cdecl*)(int, int, int)>(::bof3::Code(kDrawHand.v)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned char*, unsigned short*)>(::bof3::Code(kEquipCompare.v)),
    Fn<const unsigned char* (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kItemName.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kItemKind.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kEquipMask.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kItemFlagsOf.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kHasKeyItem.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(kCountOwned.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kCountCategory.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kPriceScale.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(kSellPrice.v)),
    Text_DrawFont12,
    Text_DrawFont8,
    Menu_DrawPanel,
    Menu_DrawBox,
    Menu_DrawIcon8,
    Text_CharCount,
    Menu_DrawPieces,
    Menu_DrawPiece,
    Menu_PieceRect,
    Menu_DrawLine,
    Menu_DrawOutline,
    Menu_DrawItemIcon,
    Menu_DrawBigIcon,
    Char_ExpForLevel,
    Item_BasePrice,
    Item_CanUse,
    Menu_DrawItemRow,
    Menu_DrawScrollBar,
    Menu_ListScroll,
    Menu_DrawBorder,
    Gpu_SetSprt8,
};
Callees g = kOriginals;

}  // namespace menu_windows

// msgbox's callee seam, moved here from msgbox.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace msgbox {

const Callees kOriginals = {
    {MsgBox_StatePrint, MsgBox_StateDelay, MsgBox_State2, MsgBox_State3, MsgBox_State4, MsgBox_State5,
     MsgBox_State6, MsgBox_State7},
    {MsgBox_State2Press, MsgBox_State2Closed},
    {MsgBox_State3Arrow, MsgBox_State3Step},
    {MsgBox_State3Arrow, MsgBox_ChoiceOpen, MsgBox_ChoiceWaitOpen, MsgBox_ChoiceInput, MsgBox_ChoiceWaitShut,
     reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(::bof3::Code(kChoiceCommit))), MsgBox_ChoiceReopen,
     MsgBox_ChoiceDone},
    {MsgBox_MenuOpen, MsgBox_MenuWaitOpen, MsgBox_MenuInput,
     reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(::bof3::Code(kMenuCommit))), MsgBox_MenuDone,
     MsgBox_State3Step},
    {MsgBox_State7Delay, MsgBox_State2Closed},
    {reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(::bof3::Code(kRetOnly))), MsgBox_EffectShake,
     MsgBox_EffectGrow, MsgBox_EffectGrow, MsgBox_EffectWander, MsgBox_EffectRise},
    {MsgBox_ShakeOut, MsgBox_ShakeBack},
    {MsgBox_GrowStart, MsgBox_GrowStep},
    {MsgBox_RiseStart, MsgBox_RiseStep},
    MsgBox_Reset,
    Window_Alloc,
    MsgBox_StateDispatch,
    MsgBox_EffectTask,
    MsgBox_Step,
    MsgBox_Reopen,
    Sound_PlayEffect,
    MsgBox_DrawChar,   // DIV-0006; Text_DrawAt when no advance table is loaded
    MsgBox_EffectDraw,
    reinterpret_cast<void (__cdecl*)(int, int)>(static_cast<std::uintptr_t>(::bof3::Code(kPageArrow))),
    reinterpret_cast<unsigned (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(::bof3::Code(kAutoRepeat))),
    Text_DrawString,
    Gpu_SetCode6C,
    Gpu_SetSemiTrans,
    Gfx_CommitPrim,
    Gpu_GetClut,
};
Callees g = kOriginals;

}  // namespace msgbox

// original 0x4976D0 (PSX 0x8015034C). Opens message `id` of the script pool:
// the pool header's u16 at 2*id is the message's offset from the pool base.
//
// As the original has it: the id is taken as a WORD and sign-extended for the
// header index (movsx eax, si), so ids 0x8000..0xFFFF read below the pool -
// no bounds check either way. The message index is stored AFTER MsgBox_Reset,
// which reads the pointer this just wrote.
//
// The reading row (docs/reading-row.md 3): when the area's side chunk has a
// composed message for the slot, both pointers take it instead (a slot of
// the sign-extended index below 0 has none). No side chunk: the original.
extern "C" void __cdecl Msg_OpenScript(unsigned short id) {
    const int index = static_cast<short>(id);
    const std::uint32_t off = W(kScriptPool + static_cast<std::uint32_t>(2 * index));
    const unsigned char* at = msgbox::At(kScriptPool) + off;
    if (index >= 0) at = ruby::Open(0, 0, static_cast<unsigned>(index), at);
    SetPtr(kBase, at);
    SetPtr(kAt, at);
    msgbox::g.reset();
    SetW(kMessage, id);
}

// original 0x497710 (PSX 0x801503AC, the sibling's Msg_OpenSystem): the
// string of system message id, as the box's base and stepper pointer, the
// message index its low 16 bits, and MsgBox_Reset.
//
// The reading row (docs/reading-row.md 3): the composed copy of the message
// when the pool's side chunk has one (MsgBox_ReadingRowSystem); none loaded,
// the original.
extern "C" void __cdecl Msg_OpenSystem(unsigned id) {
    const unsigned char* const text = MsgBox_ReadingRowSystem(id & 0xFFFFu, menu_windows::g.msg_system_ptr(id));
    Msgbox_Base = Addr(text);
    MenuWindows_ListText = Addr(text);
    move_script::SetWord(menu_windows::At(::bof3::Img(menu_windows::at::kMessage.v)), id);
    menu_windows::g.msgbox_reset();
}

// original 0x497740. The low word of the argument is the id; bits 14-15 pick
// one of four header dwords, each the offset of a u16 offset table; the rest
// indexes it. No bound on either, as in the original.
//
// DIVERGENCE DIV-0007: the pool's base is a variable. Unrelocated it is the
// original's constant and the result is the original's.
extern "C" const unsigned char* __cdecl Msg_SystemPtr(unsigned id) {
    id &= 0xFFFF;
    std::uint32_t table;
    std::memcpy(&table, msg_pool::g_pool + (id >> 14) * 4, sizeof table);
    const std::uint8_t* block = msg_pool::g_pool + table;
    std::uint16_t off;
    std::memcpy(&off, block + 2 * (id & 0x3FFF), sizeof off);
    return block + off;
}

// original 0x497770 (PSX 0x8015042C). Starts the box on the message the
// pointer at 0x7DEE50 names: the state bytes cleared, the flag word cut to
// its two kept bits, a leading 0x0C eaten into the placement byte, and
// window 0 claimed.
//
// As the original has it: the flag word keeps only bit 6 (0x40); a 0x0C
// advances the read pointer twice and both stores happen (the pointer is
// written once between them, which nothing can observe); with no 0x0C the
// placement byte is zeroed; Window_Alloc is called (0, 0) and its answer
// discarded; window 0's kind byte 0x80316F is set to 2 after it.
extern "C" void __cdecl MsgBox_Reset(void) {
    const unsigned char* at = Ptr(kAt);
    SetW(kFlags, W(kFlags) & 0x40u);
    SetB(kState, 0);
    ItemUse_SubState = 0;
    Msgbox_EffectKind = 0;
    Msgbox_EffectPhase = 0;
    Msgbox_OffsetX = 0;
    Msgbox_OffsetY = 0;
    Msgbox_EffectTimer = 0;
    if (at[0] == 0x0C) {
        ++at;
        SetPtr(kAt, at);
        const unsigned char placement = at[0];
        ++at;
        Msgbox_Placement = placement;
        SetPtr(kAt, at);
    } else {
        Msgbox_Placement = 0;
    }
    Msgbox_Delay = 0;
    Msgbox_StepCount = 0;
    g_instant_page = 0;   // DIV-0158's pacing floor starts over with the box
    SetW(kEffectOff, 0);
    ItemUse_ColorHigh = 0;
    msgbox::g.window_alloc(0, 0);
    WindowRecords[0].pass = 2;
}

// original 0x4977F0 (PSX 0x80150508). The box's frame: the box origin from
// window 0's position, the state machine, the effect, and - unless flag 4
// holds it - one step of the printer. Returns "the box is not finished".
//
// As the original has it: the origin is (window x >> 4) + 10 and
// (window y >> 4) + 6, the shift arithmetic on the signed 16-bit word and the
// add on the full register, of which only the low word is stored; the flag
// byte is read again for the return value, after the three calls.
//
// The return is a BYTE: bit 1 of the flag word, inverted. The original leaves
// the rest of eax holding the box origin x it computed, which no caller can
// depend on - 0x595A80 tests al, and the two tail jumps at 0x595855 and
// 0x595A6E hand it to their own callers (docs/msgbox.md section 2).
extern "C" unsigned char __cdecl MsgBox_FrameTask(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    const int x = static_cast<short>(win0->x) >> 4;
    const int y = static_cast<short>(win0->y) >> 4;
    MsgBox_LineX = static_cast<short>(x + 10);
    Msgbox_OriginY = static_cast<unsigned>(y + 6);
    msgbox::g.dispatch();
    msgbox::g.effect_task();
    if ((B(kFlags) & 4) == 0) msgbox::g.step();
    return static_cast<unsigned char>((static_cast<unsigned char>(~B(kFlags)) >> 1) & 1);
}

// original 0x497840 (PSX 0x80150598), 0x497840..0x497A6C with its 23-entry
// jump table at 0x497A70. One frame of the printer: it re-reads the message
// from the top every frame and walks it until it has drawn as many characters
// as 0x7DEE59 says, so the box fills in a character at a time.
//
//   0x00      end: outside a substitution it is skipped, so the walk runs on
//   0x01      newline: pen y += 14, pen x = the box's line start
//   0x03      the current member's name, 8 characters (index 0x802DC9
//             through 0x66972C, record 0x903A70 + n * 164)
//   0x04 nn   member nn's name, the same 8 characters
//   0x05 nn   take colour nn
//   0x06      colour 0
//   0x07 nn   the 32-byte record Text_Records + nn * 32, 32 characters
//   0x08 nn   message nn of the sub-pool at 0x803580 + [0x803584], 16
//             characters, starting ONE byte past where its header points
//   0x0A 0x0C 0x0F 0x14   two bytes, nothing drawn
//   0x0D      the offset pen on; 0x0E off
//   0x02 0x09 0x0B 0x10 0x11 0x16   one byte, nothing drawn
//   0x12 0x13 0x15 and every byte above 0x16   a glyph
//
// As the original has it:
//   - the pen is reset to the box origin on entry and the whole message
//     re-walked, so a colour or an offset set by a control is re-applied
//     every frame;
//   - a substitution is one deep: the countdown is decremented BEFORE the
//     character is read, so a count of 9 draws eight characters, and the
//     return position is the saved pointer plus one;
//   - the loop ends only when the drawn count EQUALS 0x7DEE59, never when it
//     passes it, and 0x7DEE59 of 0 returns before the first read;
//   - a first byte 0x2A or 0x3C exactly at the line start hangs 12 px into
//     the margin, whatever else it is (the PSX does the same for its own
//     quote at 8 px, SLUS-00422 0x80150680);
//   - under the offset pen (0x0D) the x comes from MsgBox_PenX in memory and
//     the y from the register copy, and both take the signed byte offsets
//     0x7DEE5A / 0x7DEE5B; without it both come from the registers and take
//     no offset;
//   - the colour handed on is (0x7DEE58 << 4) + the remembered byte, a byte;
//   - a second text byte is skipped when the FIRST has bit 7 set - read back
//     from memory after the draw, not from the register;
//   - the pen is reloaded from memory after every draw and then advanced.
//     DIV-0006 hangs on exactly that: g.draw_char is MsgBox_DrawChar, which
//     moves MsgBox_PenX by (advance - 12) so this `+ 12` lands it right.
//
// DIV-0140: at a button mention the `05 cc` takes the drawn button's colour
// and the icon is handed to the draw as the drawn button's glyph (its advance
// then moves the pen, DIV-0006's way); the walk steps over the text's own
// bytes, so the drawn count and every later byte are as before.
//
// The reading row (docs/reading-row.md 3): a message whose base lies in a
// side chunk is walked as row pairs by ruby::Step; no other message reaches
// it, and with no side chunk loaded the test is false before it reads
// anything.
extern "C" void __cdecl MsgBox_Step(void) {
    if (ruby::Annotated(Ptr(kBase))) {
        ruby::Step();
        return;
    }
    unsigned char flags = 0;                 // bl: bit 0 in a substitution, bit 5 the offset pen
    const unsigned char* at = Ptr(kBase);
    const unsigned char* resume = nullptr;
    unsigned char sub_count = 0;
    unsigned char drawn = 0;
    unsigned char color = 0;
    // DIV-0140 (src/game/button_text.h): 1 after a button code that resolved
    // (its `05 cc` next), 2 once the drawn colour is taken (the glyph next).
    int button = 0;
    button_text::Icon icon;
    short pen_x = MsgBox_LineX;
    auto pen_y = Msgbox_OriginY;
    MsgBox_PenX = pen_x;
    Msgbox_PenY = pen_y;
    if (Msgbox_StepCount == 0) return;
    for (;;) {
        if (flags & 1) {
            sub_count = static_cast<unsigned char>(sub_count - 1);
            if (sub_count == 0) {
                at = resume + 1;
                flags = static_cast<unsigned char>(flags & 0xFEu);
            }
        }
        const unsigned char c = at[0];
        bool glyph = c > 0x16;
        if (!glyph) {
            switch (c) {
            case 0x00:
                if (flags & 1) {
                    at = resume;
                    ++resume;
                    flags = static_cast<unsigned char>(flags & 0xFEu);
                }
                break;
            case 0x01:
                pen_x = MsgBox_LineX;
                pen_y = static_cast<std::uint16_t>(pen_y + 0x0E);
                MsgBox_PenX = pen_x;
                Msgbox_PenY = pen_y;
                break;
            case 0x03: {
                flags = static_cast<unsigned char>(flags | 1u);
                resume = at;
                const unsigned member = B(kNameMap + ObjTrio[0].char_id);
                sub_count = 9;
                at = msgbox::At(kCharRecords + 164u * member);
                break;
            }
            case 0x04: {
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned member = at[0];
                sub_count = 9;
                at = msgbox::At(kCharRecords + 164u * member);
                break;
            }
            case 0x05: {
                const unsigned char* const control = at;
                ++at;
                color = at[0];
                // DIV-0140: the code's own colour stands (06 clears it at
                // once); the `05 cc` after it, or a colour cue, takes the
                // drawn button's colour instead.
                if (button == 1 || (button == 0 && ButtonText_Cue(control, icon))) {
                    color = icon.colour;
                    button = 2;
                } else if (button == 0 && ButtonText_Code(control, icon)) {
                    button = 1;
                }
                break;
            }
            case 0x06:
                color = 0;
                break;
            case 0x07: {
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned record = at[0];
                sub_count = 0x21;
                at = msgbox::At(kTextRecords + (record << 5));
                break;
            }
            case 0x08: {
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned entry = at[0];
                const unsigned char* const pool = msgbox::At(kScriptPool + L(kPoolSelector));
                sub_count = 0x11;
                std::uint16_t off;
                std::memcpy(&off, pool + 2 * entry, sizeof off);
                at = pool + off;
                break;
            }
            case 0x0A:
            case 0x0C:
            case 0x0F:
            case 0x14:
                ++at;
                break;
            case 0x0D:
                flags = static_cast<unsigned char>(flags | 0x20u);
                break;
            case 0x0E:
                flags = static_cast<unsigned char>(flags & 0xDFu);
                break;
            case 0x12:
            case 0x13:
            case 0x15:
                glyph = true;
                break;
            default:   // 0x02 0x09 0x0B 0x10 0x11 0x16: one byte, nothing drawn
                break;
            }
        }
        if (glyph) {
            // DIV-0140: the drawn button's glyph in the fallback's place; the
            // walk still steps over the fallback's own bytes below.
            const unsigned char* const shown = button == 2 ? icon.glyph : at;
            button = 0;
            if ((c == 0x2A || c == 0x3C) &&
                static_cast<std::uint16_t>(pen_x) == static_cast<std::uint16_t>(MsgBox_LineX)) {
                pen_x = static_cast<short>(pen_x - 12);
                MsgBox_PenX = pen_x;
            }
            if (flags & 0x20) {
                if (B(kFlags) & 8) {
                    // The original pushes the colour local as a DWORD, whose
                    // upper three bytes are stack it never wrote; 0x4987E0
                    // reads `mov al, [esp+4]` and masks to 0xF, so only this
                    // byte can reach it.
                    msgbox::g.effect_draw(color, shown);
                } else {
                    const auto clut = static_cast<unsigned char>((ItemUse_ColorHigh << 4) + color);
                    const int x = static_cast<short>(MsgBox_PenX + Msgbox_OffsetX);
                    const int y = static_cast<short>(pen_y + Msgbox_OffsetY);
                    msgbox::g.draw_char(x, y, clut, 1, shown);
                }
            } else {
                const auto clut = static_cast<unsigned char>((ItemUse_ColorHigh << 4) + color);
                msgbox::g.draw_char(pen_x, pen_y, clut, 1, shown);
            }
            if (at[0] & 0x80) ++at;
            drawn = static_cast<unsigned char>(drawn + 1);
            pen_x = MsgBox_PenX;
            pen_y = Msgbox_PenY;
            pen_x = static_cast<short>(pen_x + 12);
            MsgBox_PenX = pen_x;
        }
        ++at;
        if (Msgbox_StepCount == drawn) return;
    }
}

// original 0x497AD0 (PSX 0x801508EC, which copies the same eight pointers
// from 0x80149A5C onto its own stack). One frame of the box's state machine.
extern "C" void __cdecl MsgBox_StateDispatch(void) {
    Call(msgbox::g.state, 8, B(kState), "MsgBox_StateDispatch");
}

// original 0x497B30 (PSX 0x8015096C), 0x497B30..0x497E30 with its own
// 23-entry jump table at 0x497E34 - a second table inside what pe_funcs.py
// calls one 256-byte function. State 0: it consumes the message's control
// codes ahead of the printer and decides how long the next character waits.
//
// It loops while its "keep going" flag is set; almost every code clears it,
// so one frame normally consumes one code. A glyph sets the per-character
// delay from the speed table and leaves for state 0 or 1; instant print
// (flag 0x10) makes the tail set the flag again, so the whole span between
// the 0x10 and the 0x11 that toggles it back is consumed in one frame.
//
// As the original has it:
//   - a substitution's countdown is decremented at the TOP, before the code
//     is read, and the return position is the saved pointer plus one;
//   - codes 0x05 and 0x0C step over one argument byte and keep going;
//   - code 0x08 does NOT clear the flag - it starts a sub-pool substitution
//     and the loop runs on into it;
//   - code 0x0A adds 0x200 to its argument for Sound_PlayEffect and reloads
//     both pointers from memory after the call;
//   - code 0x0B adds 8 to the delay rather than setting it;
//   - code 0x0F reads its argument back from memory three times, once for
//     each field of the 4-byte effect record;
//   - code 0x14's second byte is (rows | column << 4): the low nibble minus
//     one is the last row, and a zero high nibble picks state 5, anything
//     else state 4;
//   - the two tests against 0x100 at 0x497B80 / 0x497B86 are dead - the code
//     is a byte - so the flag-word bit 8 they would set is never set here;
//   - a glyph with bit 7 set steps over its second byte; under flag 0x10 it
//     only counts up 0x7DEE59; the speed comes from 0x658E94 (bit 5 of
//     Input_Held held) or 0x658E95, indexed by the signed speed byte, and
//     bit 8 of the flag word overrides it with 3 and forces state 1.
//
// The reading row (docs/reading-row.md 3): outside a substitution, a ruby row
// of an annotated message is stepped over whole (ruby::SkipRow), so the count
// 0x7DEE59 is the text's glyphs and a reading adds no delay.
//
// The faster text (DIV-0158, g_text_faster): Med. takes Fast's row of the
// speed table, and on Fast a glyph keeps the loop going instead of waiting,
// so the frame runs on to the next code that stops it - the page's end
// (0x00), its break (0x02), a choice (0x14), a timed wait (0x16) or a pause
// (0x0B). Unlike flag 0x10 it never forces the keep-going flag past those,
// so the box stops where the script stops it. The delay byte is still set
// to Fast's, which is what 0x0B adds its 8 frames to: the script's pauses
// last as long as they do on Fast.
//
// The pacing floor (DIV-0158's amendment, g_instant_page): when the instant
// page reaches its stop code with glyphs printed, the code is left where it
// is and state 1 counts a frame per glyph first - the frames Fast would have
// spent printing them - so the page's end, break, choice, wait or pause is
// reached, and a press can close the page, no earlier than on Fast. A stop
// code inside a 0x10 span is the original's instant case and takes none.
extern "C" void __cdecl MsgBox_StatePrint(void) {
    const unsigned char* at = Ptr(kAt);
    const unsigned char* resume = Ptr(kResume);
    bool again = true;
    unsigned instant_glyphs = 0;
    do {
        if (B(kFlags) & 1) {
            const auto left = static_cast<unsigned char>(Msgbox_SubCount - 1);
            Msgbox_SubCount = left;
            if (left == 0) {
                SetW(kFlags, W(kFlags) & 0xFFFEu);
                at = resume + 1;
                SetPtr(kAt, at);
            }
        } else if (const unsigned char* const text = ruby::SkipRow(Ptr(kBase), at); text != at) {
            // The reading row (docs/reading-row.md 3): a ruby row of an
            // annotated message is stepped over whole, costing nothing.
            at = text;
            SetPtr(kAt, at);
        }
        const unsigned char c = at[0];
        bool glyph = c > 0x16;
        if (!glyph && g_instant_page != 0 && (B(kFlags) & 0x10) == 0) {
            // DIV-0158's pacing floor: the stop code waits for the frames
            // Fast would have printed the page in. kAt already points at it.
            const bool stop = c == 0x00 ? (B(kFlags) & 1) == 0 : (c == 0x02 || c == 0x0B || c == 0x14 || c == 0x16);
            if (stop) {
                Msgbox_Delay = static_cast<unsigned char>(g_instant_page > 0xFF ? 0xFF : g_instant_page);
                g_instant_page = 0;
                SetB(kState, 1);
                return;
            }
        }
        if (!glyph) {
            switch (c) {
            case 0x00:
                if (B(kFlags) & 1) {
                    SetW(kFlags, W(kFlags) & 0xFFFEu);
                    at = resume;
                    break;
                }
                again = false;
                SetB(kState, 2);
                break;
            case 0x02:
                again = false;
                SetB(kState, 3);
                break;
            case 0x03: {
                SetB(kFlags, B(kFlags) | 1u);
                resume = at;
                again = false;
                SetPtr(kResume, resume);
                const unsigned member = B(kNameMap + ObjTrio[0].char_id);
                Msgbox_SubCount = 9;
                at = msgbox::At(kCharRecords + 164u * member);
                break;
            }
            case 0x04: {
                SetB(kFlags, B(kFlags) | 1u);
                again = false;
                ++at;
                resume = at;
                SetPtr(kAt, at);
                SetPtr(kResume, resume);
                const unsigned member = at[0];
                Msgbox_SubCount = 9;
                at = msgbox::At(kCharRecords + 164u * member);
                break;
            }
            case 0x05:
            case 0x0C:
                ++at;
                break;
            case 0x07: {
                SetB(kFlags, B(kFlags) | 1u);
                again = false;
                ++at;
                resume = at;
                SetPtr(kAt, at);
                SetPtr(kResume, resume);
                const unsigned record = at[0];
                Msgbox_SubCount = 0x21;
                at = msgbox::At(kTextRecords + (record << 5));
                break;
            }
            case 0x08: {
                SetB(kFlags, B(kFlags) | 1u);
                ++at;
                resume = at;
                SetPtr(kAt, at);
                SetPtr(kResume, resume);
                const unsigned entry = at[0];
                const unsigned char* const pool = msgbox::At(kScriptPool + L(kPoolSelector));
                SetPtr(kAt, pool);
                std::uint16_t off;
                std::memcpy(&off, pool + 2 * entry, sizeof off);
                Msgbox_SubCount = 0x11;
                at = pool + off;
                break;
            }
            case 0x0A: {
                ++at;
                SetPtr(kAt, at);
                const auto id = static_cast<std::uint16_t>(at[0] + 0x200);
                msgbox::g.sound(id);
                at = Ptr(kAt);
                resume = Ptr(kResume);
                break;
            }
            case 0x0B: {
                const auto delay = static_cast<unsigned char>(Msgbox_Delay + 8);
                SetB(kState, 1);
                again = false;
                Msgbox_Delay = delay;
                break;
            }
            case 0x0F: {
                ++at;
                SetPtr(kAt, at);
                Msgbox_EffectPhase = 0;
                ItemUse_SubState = 0;
                Msgbox_EffectKind = B(kEffectTable + 4u * at[0]);
                Msgbox_EffectTimer = W(kEffectTable + 2 + 4u * at[0]);
                SetW(kEffectOff, static_cast<unsigned>(static_cast<signed char>(B(kEffectTable + 1 + 4u * at[0]))));
                break;
            }
            case 0x10:
            case 0x11:
                SetW(kFlags, W(kFlags) ^ 0x10u);
                break;
            case 0x14: {
                again = false;
                ++at;
                SetPtr(kAt, at);
                ItemUse_ChoiceId = at[0];
                at += 2;
                SetPtr(kAt, at);
                MenuWindows_ListCount = static_cast<unsigned char>((at[0] & 0x0F) - 1);
                const auto column = static_cast<unsigned char>(at[0] >> 4);
                MenuWindows_ListSet = column;
                SetB(kState, column == 0 ? 5 : 4);
                break;
            }
            case 0x16: {
                ++at;
                again = false;
                SetPtr(kAt, at);
                SetB(kState, 7);
                Msgbox_Delay = at[0];
                break;
            }
            case 0x12:
            case 0x13:
            case 0x15:
                // The table's entries 0x12, 0x13 and 0x15 are the glyph
                // handler 0x497DAA, exactly as MsgBox_Step's are.
                glyph = true;
                break;
            default:   // 0x01 0x06 0x09 0x0D 0x0E: one byte, nothing else
                break;
            }
        }
        if (glyph) {
            if (c & 0x80) ++at;
            if ((B(kFlags) & 0x10) == 0) {
                // Holding the Speak line's button speeds the text: Japan's
                // circle asked through sigma, not word 0, which also holds
                // L2 / R2 (docs/button-tests.md 3, DIV-0139).
                const unsigned speed =
                    (static_cast<unsigned char>(Input_Held) & bof3x::input::sigma::Sigma(0x20)) ? 0 : 1;
                auto row = ModeFlow_ConfigBytes;
                const bool instant = msgbox::g_text_faster && row == 0;
                if (msgbox::g_text_faster && row == 1) row = 0;   // Med. at Fast's delays
                Msgbox_Delay = B(kSpeedTable + speed + row);
                if (L(kFlags) & 0x100u) {
                    Msgbox_Delay = 3;
                    SetB(kState, 1);
                    again = false;
                } else {
                    SetB(kState, 0);
                    // Instant: no wait, on to the next glyph this frame; the
                    // glyph is owed a frame at the page's stop code.
                    if (instant) ++g_instant_page;
                    if (!instant || ++instant_glyphs >= kInstantGlyphsPerFrame) {
                        if (Msgbox_Delay != 0 && !instant) SetB(kState, 1);
                        again = false;
                    }
                }
            }
            Msgbox_StepCount = Msgbox_StepCount + 1;
        }
        if (B(kFlags) & 0x10) again = true;
        ++at;
        SetPtr(kAt, at);
    } while (again);
}

// original 0x497E90. State 1: count the per-character delay down; at zero
// back to state 0 for the next character. As the original has it, the byte is
// decremented first, so a delay of 0 wraps to 0xFF and waits 255 frames more.
extern "C" void __cdecl MsgBox_StateDelay(void) {
    const auto left = static_cast<unsigned char>(Msgbox_Delay - 1);
    Msgbox_Delay = left;
    if (left == 0) SetB(kState, 0);
}

// original 0x497EB0. State 2, the wait at the end of a message: two entries
// on 0x7DEE41, built on the stack.
extern "C" void __cdecl MsgBox_State2(void) { Call(msgbox::g.sub2, 2, ItemUse_SubState, "MsgBox_State2"); }

// original 0x497EE0. Any button pressed - or either kept flag 0x40 / 0x80
// still set - closes window 0 and holds the printer (flag 4).
extern "C" void __cdecl MsgBox_State2Press(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    if (Input_Pressed == 0 && (B(kFlags) & 0xC0) == 0) return;
    rv::Store<unsigned>(win0->state, win0->state + 1);
    const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
    SetB(kFlags, B(kFlags) | 4u);
    ItemUse_SubState = next;
}

// original 0x497F20, shared by state 2 and state 7: once window 0 reports 4
// the box is done (flag 2), which is what MsgBox_FrameTask returns.
extern "C" void __cdecl MsgBox_State2Closed(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    if (win0->state == 4) SetB(kFlags, B(kFlags) | 2u);
}

// original 0x497F40. State 3, a page break: two entries on 0x7DEE41.
extern "C" void __cdecl MsgBox_State3(void) { Call(msgbox::g.sub3, 2, ItemUse_SubState, "MsgBox_State3"); }

// original 0x497F70, shared by state 3 and state 4: draws the "there is more"
// arrow under the box every frame (0x498D20 shows it only while bit 5 of the
// frame counter is set - it blinks), and moves on once a button is pressed or
// the kept flag 0x40 is set.
//
// As the original has it: the colour nibble 0x7DEE58 is zeroed on the way
// out; the arrow's x is (window 0's row byte >> 1) + the box line start - 14
// and its y the effect offset plus the pen y plus 12; both reach 0x498D20 as
// 16 bits (movsx word at 0x498D7C), so the stale halves the original's
// registers carry there cannot be observed.
extern "C" void __cdecl MsgBox_State3Arrow(void) {
    if (Input_Pressed != 0 || (B(kFlags) & 0x40) != 0) {
        const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
        ItemUse_ColorHigh = 0;
        ItemUse_SubState = next;
    }
    const int y = static_cast<std::uint16_t>(W(kEffectOff) + Msgbox_PenY) + 0x0C;
    const int x = (Window0_ParamA >> 1) + MsgBox_LineX - 0x0E;
    msgbox::g.page_arrow(x, y);
}

// original 0x497FD0, shared by state 3 and state 5. Scrolls the box one row
// every frame the delay expires: seven rows, then either the choice list is
// re-opened (states 4 and 5) or the printer restarts on what is left of the
// message, with the hold and the effect-draw flags cleared.
//
// As the original has it: the delay byte is tested BEFORE it is decremented,
// so a delay of 0 does the work and leaves 0xFF behind; the row counter is
// stored whatever it reached; the restart takes the message base from the
// read position, so the rest of the message becomes the whole message.
extern "C" void __cdecl MsgBox_State3Step(void) {
    const unsigned char was = Msgbox_Delay;
    Msgbox_Delay = static_cast<unsigned char>(was - 1);
    if (was != 0) return;
    const auto row = static_cast<unsigned char>(ItemUse_ColorHigh + 1);
    Msgbox_Delay = 1;
    ItemUse_ColorHigh = row;
    if (row != 7) return;
    if (B(kState) == 4 || B(kState) == 5) {
        msgbox::g.reopen();
        return;
    }
    const std::uint32_t at = MenuWindows_ListText;
    SetW(kFlags, W(kFlags) & 0xFFF3u);
    ItemUse_SubState = 0;
    SetB(kState, 1);
    Msgbox_Delay = 8;
    Msgbox_Base = at;
    SetW(kEffectOff, 0);
    Msgbox_StepCount = 0;
    ItemUse_ColorHigh = 0;
}

// original 0x498050. State 4, a choice list in its own window: eight entries
// on 0x7DEE41, the first of them state 3's arrow.
extern "C" void __cdecl MsgBox_State4(void) { Call(msgbox::g.sub4, 8, ItemUse_SubState, "MsgBox_State4"); }

// original 0x4980B0. Claims window 1 as kind 1 (a choice list), tells window
// 0 to go to state 3, and holds the printer.
extern "C" void __cdecl MsgBox_ChoiceOpen(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    AreaW0c_ChoiceAnswer = 0;
    msgbox::g.window_alloc(1, 0);
    const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
    SetB(kFlags, B(kFlags) | 4u);
    WindowRecords[1].kind = 1;
    rv::Store<unsigned>(win0->state, 3);
    ItemUse_SubState = next;
}

// original 0x4980F0. Waits until window 1 is open and window 0 has finished
// its own move.
extern "C" void __cdecl MsgBox_ChoiceWaitOpen(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    if (WindowRecords[1].state == 2 && win0->state == 4) ItemUse_SubState = ItemUse_SubState + 1;
}

// original 0x498110. The choice list's input: cancel closes it, confirm
// jumps two entries on, and up / down move the cursor through the auto-repeat
// at 0x461EB0 - which is handed the whole pad word here (the menu's copy at
// 0x498350 masks it to 0x5000 first).
//
// As the original has it: the cursor wraps at both ends, the bottom test is
// SIGNED against the last row, and the top one on the sign of the decremented
// byte - so a cursor of 0x80 stays where it is going down.
extern "C" void __cdecl MsgBox_ChoiceInput(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    const unsigned pressed = Input_Pressed;
    if ((Field_CancelButtons & pressed) != 0) {
        const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
        WindowRecords[1].state = 3;
        rv::Store<unsigned>(win0->state, 1);
        ItemUse_SubState = next;
        return;
    }
    if ((Field_ConfirmButtons & pressed) != 0) {
        ItemUse_SubState = ItemUse_SubState + 2;
        return;
    }
    const unsigned repeat = msgbox::g.repeat(pressed);
    const unsigned char last = static_cast<unsigned char>(MenuWindows_ListCount);
    if ((repeat >> 8) & 0x10) {
        const auto up = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_ChoiceAnswer) - 1);
        AreaW0c_ChoiceAnswer = up;
        if (static_cast<signed char>(up) < 0) AreaW0c_ChoiceAnswer = last;
    }
    if ((repeat >> 8) & 0x40) {
        const auto down = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_ChoiceAnswer) + 1);
        AreaW0c_ChoiceAnswer = down;
        if (static_cast<signed char>(down) > static_cast<signed char>(last)) AreaW0c_ChoiceAnswer = 0;
    }
}

// original 0x4981A0. Once window 0 reports 2 the hold flag is toggled off and
// the state's own index goes back to 0.
extern "C" void __cdecl MsgBox_ChoiceWaitShut(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    if (win0->state != 2) return;
    SetW(kFlags, W(kFlags) ^ 4u);
    ItemUse_SubState = 0;
}

// original 0x4981C0 (PSX 0x80151494): MsgBox_State4's entry 5. After the
// commit, window 1's state 3; with a message left (0x7DEE48 not 0xFFFF)
// window 0's state 1 and the next sub-state, else sub-state 7.
extern "C" void __cdecl MsgBox_ChoiceCommit(void) {
    CommitChoice();
    const bool more = item_use::Word(item_use::At(::bof3::Img(item_use::at::kMessage.v))) != 0xFFFF;
    WindowRecords[1].state = 3;
    if (more) {
        const unsigned char sub = ItemUse_SubState;
        WindowRecords[0].state = 1;
        ItemUse_ColorHigh = 0;
        ItemUse_SubState = static_cast<unsigned char>(sub + 1);
        return;
    }
    ItemUse_SubState = 7;
    ItemUse_ColorHigh = 0;
}

// original 0x498230. Re-opens the message at the answer's own text and
// releases the hold.
extern "C" void __cdecl MsgBox_ChoiceReopen(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    if (win0->state != 2) return;
    Msgbox_Delay = 0x10;
    msgbox::g.reopen();
    SetW(kFlags, W(kFlags) ^ 4u);
}

// original 0x498250. Once window 1 reports 4 the box is finished: flag 0x80
// set (which MsgBox_Reset keeps only 0x40 of, so it is cleared on the next
// message), window 0 to state 4, and state 2 sub 1 - the wait for it to shut.
extern "C" void __cdecl MsgBox_ChoiceDone(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    if (WindowRecords[1].state != 4) return;
    SetB(kFlags, B(kFlags) | 0x80u);
    rv::Store<unsigned>(win0->state, 4);
    SetB(kState, 2);
    ItemUse_SubState = 1;
}

// original 0x498280. Re-opens the message the box is on, from the top: the
// same lookup as Msg_OpenScript but with the id taken UNSIGNED, and without
// MsgBox_Reset - only the delay, the state and the two counters are set.
//
// The reading row (docs/reading-row.md 3): the slot's composed message when
// the area's side chunk has one, as Msg_OpenScript.
extern "C" void __cdecl MsgBox_Reopen(void) {
    const unsigned id = W(kMessage);
    Msgbox_Delay = 8;
    SetB(kState, 1);
    const std::uint32_t off = W(kScriptPool + 2u * id);
    const unsigned char* const at = ruby::Open(0, 0, id, msgbox::At(kScriptPool) + off);
    SetPtr(kBase, at);
    SetPtr(kAt, at);
    Msgbox_StepCount = 0;
    ItemUse_SubState = 0;
}

// original 0x4982C0. State 5, the same list drawn as a menu (code 0x14 with a
// zero high nibble): six entries on 0x7DEE41, the last of them state 3's
// scroll.
extern "C" void __cdecl MsgBox_State5(void) { Call(msgbox::g.sub5, 6, ItemUse_SubState, "MsgBox_State5"); }

// original 0x498310. Claims window 1 as kind 2 (a menu). Unlike the choice
// list's opener it does not touch window 0 or the hold flag.
extern "C" void __cdecl MsgBox_MenuOpen(void) {
    AreaW0c_ChoiceAnswer = 0;
    msgbox::g.window_alloc(1, 0);
    const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
    WindowRecords[1].kind = 2;
    ItemUse_SubState = next;
}

// original 0x498340. Waits until window 1 reports 2.
extern "C" void __cdecl MsgBox_MenuWaitOpen(void) {
    if (WindowRecords[1].state == 2) ItemUse_SubState = ItemUse_SubState + 1;
}

// original 0x498350. The menu's input: confirm moves on, and the cursor moves
// on the auto-repeat of the pad masked to 0x5000 - up and down only, so a
// left or right press cannot restart the repeat timer here.
extern "C" void __cdecl MsgBox_MenuInput(void) {
    const unsigned pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) ItemUse_SubState = ItemUse_SubState + 1;
    const unsigned repeat = msgbox::g.repeat(pressed & 0x5000u);
    const unsigned char last = static_cast<unsigned char>(MenuWindows_ListCount);
    if ((repeat >> 8) & 0x10) {
        const auto up = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_ChoiceAnswer) - 1);
        AreaW0c_ChoiceAnswer = up;
        if (static_cast<signed char>(up) < 0) AreaW0c_ChoiceAnswer = last;
    }
    if ((repeat >> 8) & 0x40) {
        const auto down = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_ChoiceAnswer) + 1);
        AreaW0c_ChoiceAnswer = down;
        if (static_cast<signed char>(down) > static_cast<signed char>(last)) AreaW0c_ChoiceAnswer = 0;
    }
}

// original 0x4983C0 (PSX 0x80151838): MsgBox_State5's entry 3.
extern "C" void __cdecl MsgBox_MenuCommit(void) {
    CommitChoice();
    const unsigned char sub = ItemUse_SubState;
    ItemUse_ColorHigh = 0;
    WindowRecords[1].state = 3;
    ItemUse_SubState = static_cast<unsigned char>(sub + 1);
}

// original 0x498410. Once window 1 reports 4: a message id of 0xFFFF means
// there is nothing left to say and the box is finished; otherwise on to the
// next entry.
extern "C" void __cdecl MsgBox_MenuDone(void) {
    if (WindowRecords[1].state != 4) return;
    if (W(kMessage) != 0xFFFF) {
        ItemUse_SubState = ItemUse_SubState + 1;
        return;
    }
    SetB(kFlags, B(kFlags) | 0x80u);
    SetB(kState, 2);
    ItemUse_SubState = 0;
}

// original 0x498450. State 6: back to state 0 once window 0 reports 2. No
// path in this file reaches it - nothing writes 6 to 0x7DEE40 (section 2).
extern "C" void __cdecl MsgBox_State6(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    if (win0->state == 2) SetB(kState, 0);
}

// original 0x498470. State 7, the wait code 0x16 asks for: two entries on
// 0x7DEE41, the second of them state 2's "window closed" test.
extern "C" void __cdecl MsgBox_State7(void) { Call(msgbox::g.sub7, 2, ItemUse_SubState, "MsgBox_State7"); }

// original 0x4984A0. Counts code 0x16's argument down; at zero window 0 is
// told to move on, the printer is held, and the state's index advances.
//
// As the original has it: the delay is tested BEFORE the decrement, so a 0
// does the work and leaves 0xFF.
extern "C" void __cdecl MsgBox_State7Delay(void) {
    auto* const win0 = rv::As<records::WindowRecord>(msgbox::At(::bof3::Img(WindowRecords_addr)) + 0 * sizeof(records::WindowRecord));
    const unsigned char was = Msgbox_Delay;
    Msgbox_Delay = static_cast<unsigned char>(was - 1);
    if (was != 0) return;
    const auto window = static_cast<unsigned char>(win0->state + 1);
    const auto next = static_cast<unsigned char>(ItemUse_SubState + 1);
    SetB(kFlags, B(kFlags) | 4u);
    rv::Store<unsigned>(win0->state, window);
    ItemUse_SubState = next;
}

// original 0x4984E0 (PSX 0x80151A1C). The box's effect, one frame: six
// entries on 0x7DEE42, which code 0x0F sets from the record table 0x658E98.
// Kind 0 is a bare `ret` at 0x437CC0 - the only entry the attract sequence
// ever reaches - and kinds 2 and 3 share one handler.
extern "C" void __cdecl MsgBox_EffectTask(void) {
    Call(msgbox::g.effect, 6, Msgbox_EffectKind, "MsgBox_EffectTask");
}

// original 0x498520. Effect 1: the pen x offset rocks between -2 and +2, two
// units a frame, until the effect's own timer runs out.
//
// As the original has it: the timer is only decremented when it is not
// 0xFFFF (which means forever), and the kind is cleared when the timer READS
// zero - including the 0xFFFF case's zero, which it cannot be.
extern "C" void __cdecl MsgBox_EffectShake(void) {
    Call(msgbox::g.shake, 2, Msgbox_EffectPhase, "MsgBox_EffectShake");
    std::uint16_t left = Msgbox_EffectTimer;
    if (left != 0xFFFF) {
        left = static_cast<std::uint16_t>(left - 1);
        Msgbox_EffectTimer = left;
    }
    if (left == 0) Msgbox_EffectKind = 0;
}

// original 0x498560 / 0x498580: out to -2, then back to +2, then out again.
extern "C" void __cdecl MsgBox_ShakeOut(void) {
    if (static_cast<unsigned char>(Msgbox_OffsetX) == 0xFE) {
        Msgbox_EffectPhase = Msgbox_EffectPhase + 1;
        return;
    }
    Msgbox_OffsetX = static_cast<unsigned char>(Msgbox_OffsetX) + 0xFE;
}

extern "C" void __cdecl MsgBox_ShakeBack(void) {
    if (static_cast<unsigned char>(Msgbox_OffsetX) == 2) {
        Msgbox_EffectPhase = Msgbox_EffectPhase - 1;
        return;
    }
    Msgbox_OffsetX = static_cast<unsigned char>(Msgbox_OffsetX) + 2;
}

// original 0x4985A0, effects 2 and 3: the effect draw's own offset 0x7DEE68
// walks to its limit and back. Two entries on 0x7DEE43, then the timer down
// unless it is 0xFFFF.
extern "C" void __cdecl MsgBox_EffectGrow(void) {
    Call(msgbox::g.grow, 2, Msgbox_EffectPhase, "MsgBox_EffectGrow");
    if (Msgbox_EffectTimer != 0xFFFF) Msgbox_EffectTimer = Msgbox_EffectTimer - 1;
}

// original 0x4985E0. Turns the effect draw on (flag 8) and, once the timer
// has reached zero, sets it to 1 and moves to the second phase.
extern "C" void __cdecl MsgBox_GrowStart(void) {
    SetB(kFlags, B(kFlags) | 8u);
    if (Msgbox_EffectTimer != 0) return;
    const auto next = static_cast<unsigned char>(Msgbox_EffectPhase + 1);
    Msgbox_EffectTimer = 1;
    Msgbox_EffectPhase = next;
}

// original 0x498610. Kind 2 walks the offset down by 2 and stops when it goes
// negative; every other kind walks it up by 2 and stops when it goes
// positive. Stopping clears flag 8, the offset and the kind. Either way the
// timer is put back to 1, so this runs every frame.
extern "C" void __cdecl MsgBox_GrowStep(void) {
    if (Msgbox_EffectTimer != 0) return;
    bool stop;
    if (Msgbox_EffectKind == 2) {
        const auto v = static_cast<std::int16_t>(W(kEffectOff) - 2);
        SetW(kEffectOff, static_cast<std::uint16_t>(v));
        stop = v < 0;
    } else {
        const auto v = static_cast<std::int16_t>(W(kEffectOff) + 2);
        SetW(kEffectOff, static_cast<std::uint16_t>(v));
        stop = v > 0;
    }
    if (stop) {
        SetW(kFlags, W(kFlags) & 0xFFF7u);
        SetW(kEffectOff, 0);
        Msgbox_EffectKind = 0;
    }
    Msgbox_EffectTimer = 1;
}

// original 0x498670. Effect 4: the pen x offset wanders between -3 and +3 one
// unit at a time, and each time it comes back through zero the y offset takes
// a step of 0x7DEE68's low byte; a y of 0xF6 turns the step around (offset 1,
// wait 15) and a y of exactly 0 counts one repetition off the timer and, at
// the last one, stops the effect.
//
// As the original has it: the x offset is stored BEFORE the wait counter is
// tested, so a frame that only counts the wait down still moves it; the wait
// is 3 between steps and 15 at each turn; a timer of 0xFFFF never ends.
extern "C" void __cdecl MsgBox_EffectWander(void) {
    unsigned char x;
    if (Msgbox_EffectPhase == 0) {
        if (static_cast<unsigned char>(Msgbox_OffsetX) == 0xFD) {
            Msgbox_EffectPhase = 1;
            return;
        }
        x = static_cast<unsigned char>(static_cast<unsigned char>(Msgbox_OffsetX) - 1);
    } else {
        if (static_cast<unsigned char>(Msgbox_OffsetX) == 3) {
            Msgbox_EffectPhase = Msgbox_EffectPhase - 1;
            return;
        }
        x = static_cast<unsigned char>(static_cast<unsigned char>(Msgbox_OffsetX) + 1);
    }
    const bool waiting = Msgbox_EffectWait != 0;
    Msgbox_OffsetX = x;
    if (waiting) {
        Msgbox_EffectWait = Msgbox_EffectWait - 1;
        return;
    }
    const auto y = static_cast<unsigned char>(static_cast<unsigned char>(Msgbox_OffsetY) + static_cast<unsigned char>(W(kEffectOff)));
    Msgbox_OffsetY = y;
    if (y == 0xF6) {
        Msgbox_EffectWait = 0x0F;
        SetW(kEffectOff, 1);
        return;
    }
    if (y != 0) {
        Msgbox_EffectWait = 3;
        return;
    }
    const std::uint16_t left = Msgbox_EffectTimer;
    Msgbox_EffectWait = 0x0F;
    SetW(kEffectOff, 0xFFFF);
    if (left == 0xFFFF) return;
    Msgbox_EffectTimer = static_cast<std::uint16_t>(left - 1);
    if (left != 0) return;
    Msgbox_EffectKind = 0;
    Msgbox_OffsetY = 0;
    Msgbox_OffsetX = 0;
}

// original 0x498740. Effect 5: two entries on 0x7DEE43 - a set-up, then a
// per-frame integration of 0x7DEE68 into the y offset.
extern "C" void __cdecl MsgBox_EffectRise(void) { Call(msgbox::g.rise, 2, Msgbox_EffectPhase, "MsgBox_EffectRise"); }

// original 0x498770. The record's frame count becomes the accumulator's
// starting value and its offset is scaled by 16 - an 8.8 fixed-point speed.
extern "C" void __cdecl MsgBox_RiseStart(void) {
    const std::uint16_t frames = Msgbox_EffectTimer;
    SetW(kEffectOff, W(kEffectOff) << 4);
    Msgbox_EffectWait = frames;
    Msgbox_EffectPhase = Msgbox_EffectPhase + 1;
}

// original 0x4987A0. Adds the speed to the accumulator every frame and the
// accumulator's HIGH byte - before the add - to the y offset; once the y
// offset is no longer negative it and the kind are cleared, which ends the
// effect. As the original has it, the high byte is taken with `sar edx, 8`
// on a register whose upper half is stale, which cannot reach dl.
extern "C" void __cdecl MsgBox_RiseStep(void) {
    const std::uint16_t was = Msgbox_EffectWait;
    const auto y = static_cast<unsigned char>(static_cast<unsigned char>(Msgbox_OffsetY) + static_cast<unsigned char>(was >> 8));
    Msgbox_EffectWait = static_cast<std::uint16_t>(was + W(kEffectOff));
    Msgbox_OffsetY = y;
    if (static_cast<signed char>(y) < 0) return;
    Msgbox_OffsetY = 0;
    Msgbox_EffectKind = 0;
}

// original 0x4987E0 (PSX 0x80151F4C), 0x4987E0..0x498A2A. MsgBox_Step's draw
// for a character inside an offset-pen span (0x0D .. 0x0E) while flag 8 of
// 0x7DEE44 is set - effects 2 and 3, the grow and the shrink. One
// POLY_FT4-shaped record at Gfx_PacketNext, scaled: the quad is 12 + P wide
// and (about) 12 + P tall, P the signed word 0x7DEE68 that MsgBox_GrowStep
// walks, out of the same 12-unit glyph cell; then the pen is moved by P, and
// the stepper's own + 12 makes the advance the drawn width.
//
// As the original has it:
//   - the CLUT word +0x0E is Gpu_GetClut((colour & 0xF) << 4, 0x1E0), only
//     the low nibble of the pushed colour taken;
//   - the texture extent W is 12 while P <= 12 (a signed compare), 11 above;
//   - the glyph word +0x16 is ((c & 0x7F) << 8) + the next byte for a lead
//     byte with bit 7 set, else c - 0x26 as a WORD (so 0x12..0x25 wrap);
//   - the texture bytes are (0, 0), (W, 0), (0, W - T), (W, W - T), T the
//     clip byte 0x658F00[0x7DEE58] - the row the page scroll is on;
//   - the corners are (x, y), (x + P + 12, y), (x, y'), (x + P + 12, y') with
//     y' = y + P + 12 - (T * (((P + 12) << 4) / 12) >> 4), the division
//     signed and truncating (the 0x2AAAAAAB multiply), each stored as a word;
//   - shade 0x80, code 0x6C, semi-transparency 1, Gfx_CommitPrim(1, 0x28);
//   - a SPACE (0x20, `cmp cl, 0x20 / je 0x4988BE` at 0x498819) skips the
//     glyph word and all eight texture bytes but still writes the shade and
//     the corners and commits the primitive - whatever glyph word and
//     texture the packet slot last held are drawn in the gap. The PSX twin
//     does the same for its word separator 0xFF (0x80152010 -> 0x80152BF8:
//     no tpage, no u, v, the corners and the commit), so the slip is the
//     PlayStation's, carried over with 0x20 standing for 0xFF.
//
// DIVERGENCE DIV-0070: a space commits nothing. The clut word is written and
// the pen moves by P exactly as the original's do; the quad is not built and
// not committed. The stale glyph word was what crashed Font_UnpackGlyph on
// 2026-10-02 (area 0x63, message 0x24, docs/msgbox.md section 4).
//
// Not DIV-0006's: the advance is 12 + P for every character, the space too;
// the advance table is not consulted (docs/msgbox.md section 4).
extern "C" void __cdecl MsgBox_EffectDraw(unsigned color, const unsigned char* text) {
    const auto put = [](unsigned char* at, unsigned value) {
        const auto word = static_cast<std::uint16_t>(value);
        std::memcpy(at, &word, sizeof word);
    };
    const unsigned clut = msgbox::g.get_clut(static_cast<int>((color & 0xFu) << 4), 0x1E0);
    put(Gfx_PacketNext + 0x0E, clut);
    const unsigned char extent = static_cast<std::int16_t>(W(kEffectOff)) <= 0x0C ? 0x0C : 0x0B;
    const unsigned char c = text[0];
    if (c != 0x20) {
        const auto glyph = static_cast<std::uint16_t>((c & 0x80) ? ((c & 0x7Fu) << 8) + text[1] : c - 0x26u);
        unsigned char* const p = Gfx_PacketNext;
        put(p + 0x16, glyph);
        p[0x0C] = 0;
        p[0x0D] = 0;
        p[0x14] = extent;
        p[0x15] = 0;
        p[0x1C] = 0;
        p[0x1D] = static_cast<unsigned char>(extent - B(kClipTable + ItemUse_ColorHigh));
        p[0x24] = extent;
        p[0x25] = static_cast<unsigned char>(extent - B(kClipTable + ItemUse_ColorHigh));
    } else if (g_effect_space_skips) {
        MsgBox_PenX = static_cast<short>(MsgBox_PenX + W(kEffectOff));   // DIV-0070
        return;
    }
    unsigned char* const p = Gfx_PacketNext;
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    const auto x = static_cast<std::uint16_t>(MsgBox_PenX);
    const std::uint16_t y = Msgbox_PenY;
    const auto off = static_cast<std::int16_t>(W(kEffectOff));
    const auto right = static_cast<unsigned>(x + static_cast<std::uint16_t>(off) + 0x0C);
    const int scale = ((off + 0x0C) << 4) / 0x0C;
    const int cut = static_cast<int>(B(kClipTable + ItemUse_ColorHigh)) * scale >> 4;
    const auto bottom = static_cast<unsigned>(y - static_cast<unsigned>(cut) + static_cast<std::uint16_t>(off) + 0x0C);
    put(p + 0x08, x);
    put(p + 0x0A, y);
    put(p + 0x10, right);
    put(p + 0x12, y);
    put(p + 0x18, x);
    put(p + 0x1A, bottom);
    put(p + 0x20, right);
    put(p + 0x22, bottom);
    msgbox::g.set_code6c(Gfx_PacketNext);
    msgbox::g.set_semitrans(Gfx_PacketNext, 1);
    msgbox::g.commit(1, 0x28);
    MsgBox_PenX = static_cast<short>(MsgBox_PenX + W(kEffectOff));
}

// original 0x498A30 (PSX 0x80152DB4): the handler of a message-box choice id
// 0x7DEE64 of 0x80 or more, called by MsgBox_ChoiceCommit and
// MsgBox_MenuCommit. The original stores sixteen handlers on its stack
// (MsgBox_SysChoice80..8F, 0x498AD0..0x498D00) and calls [esp + 4 * id -
// 0x200] unbounded (D22).
//
// Ids 0x80..0x8F call their handler. Id 0x90 calls the word above the table:
// the original's own return address, so the caller's tail runs, returns here
// and runs again - reproduced exactly when the caller is one of the two
// commits' original bodies (their tails 0x4981F1 / 0x4983F1 touch no stack
// but their own return), by calling the return address. Every other id -
// 0x91 and up call further words of the caller's frame, below 0x80 words
// below the table - and 0x90 from any other caller (both commits are ours,
// item_use.cpp, so the frame the original would call into is compiled code
// of ours) abort (CLAUDE.md rule 4): there is no original frame left to
// reproduce. docs/mode_states.md section 2.
extern "C" __attribute__((noinline, disable_tail_calls)) void __cdecl MsgBox_SystemChoice(void) {
    const unsigned id = ItemUse_ChoiceId;
    if (id - 0x80u < 16u) {
        mode_states::g.choice[id - 0x80u]();
        return;
    }
    if (id == 0x90) {
        const std::uint32_t resume = Address(__builtin_return_address(0));
        if (resume == mode_states::g.tails[0] || resume == mode_states::g.tails[1]) {
            reinterpret_cast<mode_states::Handler>(static_cast<std::uintptr_t>(resume))();
            return;
        }
        bof3::Fatal("MsgBox_SystemChoice: choice id 0x90 calls its caller's resume point 0x%X, not a commit's tail",
                    static_cast<unsigned>(resume));
    }
    bof3::Fatal("MsgBox_SystemChoice: choice id 0x%X, outside the sixteen-entry table", id);
}

// original 0x498AD0: cursor 0 sets the byte 0x929F0B, cursor 1 clears it.
extern "C" void __cdecl MsgBox_SysChoice80(void) {
    const int cursor = SysMessage(0);
    if (cursor == 0) FieldS_Answer = 1;
    else if (cursor == 1) FieldS_Answer = 0;
}

// original 0x498B00, 0x498B30, 0x498B60, 0x498B90: MoveScript variable 3
// (0x903848) by the cursor.
extern "C" void __cdecl MsgBox_SysChoice81(void) { SysCounter(1, 0x1E, 0x28); }

extern "C" void __cdecl MsgBox_SysChoice82(void) { SysCounter(2, 0x1E, 0x32); }

extern "C" void __cdecl MsgBox_SysChoice83(void) { SysCounter(3, 0x3C, 0x46); }

extern "C" void __cdecl MsgBox_SysChoice84(void) { SysCounter(4, 0x1E, 0x32); }

// original 0x498BC0..0x498D00, 0x20 apart: the message word only.
extern "C" void __cdecl MsgBox_SysChoice85(void) { SysMessage(5); }

extern "C" void __cdecl MsgBox_SysChoice86(void) { SysMessage(6); }

extern "C" void __cdecl MsgBox_SysChoice87(void) { SysMessage(7); }

extern "C" void __cdecl MsgBox_SysChoice88(void) { SysMessage(8); }

extern "C" void __cdecl MsgBox_SysChoice89(void) { SysMessage(9); }

extern "C" void __cdecl MsgBox_SysChoice8A(void) { SysMessage(10); }

extern "C" void __cdecl MsgBox_SysChoice8B(void) { SysMessage(11); }

extern "C" void __cdecl MsgBox_SysChoice8C(void) { SysMessage(12); }

extern "C" void __cdecl MsgBox_SysChoice8D(void) { SysMessage(13); }

extern "C" void __cdecl MsgBox_SysChoice8E(void) { SysMessage(14); }

extern "C" void __cdecl MsgBox_SysChoice8F(void) { SysMessage(15); }

// original 0x498D20: the "there is more" arrow under the message box, an 8 x 8
// SPRT at (u 0xD0, v 0xF8) of page (0x3C0, 0) under abr 1, shown only while
// bit 5 of Frame_Counter is set - so it blinks, 32 frames on and 32 off. x and
// y are read as 16-bit (movsx).
extern "C" void __cdecl MsgBox_DrawArrow(int x, int y) {
    if (!(menu_windows::At(::bof3::Img(Frame_Counter_addr))[0] & 0x20)) return;
    const unsigned tpage = menu_windows::g.get_tpage(0, 1, 0x3C0, 0);
    menu_windows::g.draw_mode(Next(), 0, 0, tpage & 0xFFFF, 0);
    menu_windows::g.commit(1, 0xC);
    unsigned char* const p = Next();
    menu_windows::g.set_sprt(p);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, menu_windows::g.get_clut(0xC0, 0x1E0));
    p[4] = p[5] = p[6] = 0x80;
    Put(p + 8, F(S16MenuWindows(x)));
    Put(p + 0xC, F(S16MenuWindows(y)));
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, 8);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, 8);
    p[0x14] = 0xD0;
    p[0x15] = 0xF8;
    menu_windows::g.set_semi(p, 1);
    menu_windows::g.commit(1, 0x1C);
}

void TextMsgBoxFromItemUse_Inject() {
    BOF3_INJECT(MsgBox_ChoiceCommit);
    BOF3_INJECT(MsgBox_MenuCommit);
    BOF3_INJECT(MsgBox_SysChoice80);
    BOF3_INJECT(MsgBox_SysChoice81);
    BOF3_INJECT(MsgBox_SysChoice82);
    BOF3_INJECT(MsgBox_SysChoice83);
    BOF3_INJECT(MsgBox_SysChoice84);
    BOF3_INJECT(MsgBox_SysChoice85);
    BOF3_INJECT(MsgBox_SysChoice86);
    BOF3_INJECT(MsgBox_SysChoice87);
    BOF3_INJECT(MsgBox_SysChoice88);
    BOF3_INJECT(MsgBox_SysChoice89);
    BOF3_INJECT(MsgBox_SysChoice8A);
    BOF3_INJECT(MsgBox_SysChoice8B);
    BOF3_INJECT(MsgBox_SysChoice8C);
    BOF3_INJECT(MsgBox_SysChoice8D);
    BOF3_INJECT(MsgBox_SysChoice8E);
    BOF3_INJECT(MsgBox_SysChoice8F);
}

void TextMsgBoxFromMenuWindows_Inject() {
    BOF3_INJECT(Msg_OpenSystem);
    BOF3_INJECT(MsgBox_DrawArrow);
}

void TextMsgBoxFromModeStates_Inject() {
    BOF3_INJECT(MsgBox_SystemChoice);
}

void TextMsgBoxFromMsgPool_Inject() {
    BOF3_INJECT(Msg_SystemPtr);
}

void TextMsgBoxFromMsgbox_Inject() {
    BOF3_INJECT(Msg_OpenScript);
    BOF3_INJECT(MsgBox_Reset);
    BOF3_INJECT(MsgBox_FrameTask);
    BOF3_INJECT(MsgBox_Step);
    BOF3_INJECT(MsgBox_StateDispatch);
    BOF3_INJECT(MsgBox_StatePrint);
    BOF3_INJECT(MsgBox_StateDelay);
    BOF3_INJECT(MsgBox_State2);
    BOF3_INJECT(MsgBox_State2Press);
    BOF3_INJECT(MsgBox_State2Closed);
    BOF3_INJECT(MsgBox_State3);
    BOF3_INJECT(MsgBox_State3Arrow);
    BOF3_INJECT(MsgBox_State3Step);
    BOF3_INJECT(MsgBox_State4);
    BOF3_INJECT(MsgBox_ChoiceOpen);
    BOF3_INJECT(MsgBox_ChoiceWaitOpen);
    BOF3_INJECT(MsgBox_ChoiceInput);
    BOF3_INJECT(MsgBox_ChoiceWaitShut);
    BOF3_INJECT(MsgBox_ChoiceReopen);
    BOF3_INJECT(MsgBox_ChoiceDone);
    BOF3_INJECT(MsgBox_Reopen);
    BOF3_INJECT(MsgBox_State5);
    BOF3_INJECT(MsgBox_MenuOpen);
    BOF3_INJECT(MsgBox_MenuWaitOpen);
    BOF3_INJECT(MsgBox_MenuInput);
    BOF3_INJECT(MsgBox_MenuDone);
    BOF3_INJECT(MsgBox_State6);
    BOF3_INJECT(MsgBox_State7);
    BOF3_INJECT(MsgBox_State7Delay);
    BOF3_INJECT(MsgBox_EffectTask);
    BOF3_INJECT(MsgBox_EffectShake);
    BOF3_INJECT(MsgBox_ShakeOut);
    BOF3_INJECT(MsgBox_ShakeBack);
    BOF3_INJECT(MsgBox_EffectGrow);
    BOF3_INJECT(MsgBox_GrowStart);
    BOF3_INJECT(MsgBox_GrowStep);
    BOF3_INJECT(MsgBox_EffectWander);
    BOF3_INJECT(MsgBox_EffectRise);
    BOF3_INJECT(MsgBox_RiseStart);
    BOF3_INJECT(MsgBox_RiseStep);
    BOF3_INJECT(MsgBox_EffectDraw);
}
