// The dialogue box, read to the last instruction of each function against the
// PSX twins where they pair (docs/msgbox.md). Forty-four functions: the
// script-side opener, the reset, the frame task, the control-code stepper and
// its 23-entry table, the eight state handlers under MsgBox_StateDispatch and
// their own stack-built tables, the box effect task and its five effects, the
// window slot allocator, and the two remaining text pens.
//
// DIVERGENCE DIV-0006 lives here now: MsgBox_Step's one draw call goes to
// MsgBox_DrawChar (src/game/text_advance.cpp) rather than straight to
// Text_DrawAt, which is what the RetargetCall in that file did to Capcom's
// body. With no advance table loaded - every shipped file - MsgBox_DrawChar
// IS Text_DrawAt, so this is the original. The RetargetCall stays where it
// is: it is what keeps DIV-0006 alive under BOF3X_ORIGINAL=MsgBox_Step.
#include "game/msgbox.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/msgbox_callees.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace msgbox {

const Callees kOriginals = {
    {MsgBox_StatePrint, MsgBox_StateDelay, MsgBox_State2, MsgBox_State3, MsgBox_State4, MsgBox_State5,
     MsgBox_State6, MsgBox_State7},
    {MsgBox_State2Press, MsgBox_State2Closed},
    {MsgBox_State3Arrow, MsgBox_State3Step},
    {MsgBox_State3Arrow, MsgBox_ChoiceOpen, MsgBox_ChoiceWaitOpen, MsgBox_ChoiceInput, MsgBox_ChoiceWaitShut,
     reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(kChoiceCommit)), MsgBox_ChoiceReopen,
     MsgBox_ChoiceDone},
    {MsgBox_MenuOpen, MsgBox_MenuWaitOpen, MsgBox_MenuInput,
     reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(kMenuCommit)), MsgBox_MenuDone,
     MsgBox_State3Step},
    {MsgBox_State7Delay, MsgBox_State2Closed},
    {reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(kRetOnly)), MsgBox_EffectShake,
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
    reinterpret_cast<void (__cdecl*)(unsigned, const unsigned char*)>(static_cast<std::uintptr_t>(kEffectDraw)),
    reinterpret_cast<void (__cdecl*)(int, int)>(static_cast<std::uintptr_t>(kPageArrow)),
    reinterpret_cast<unsigned (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(kAutoRepeat)),
    Text_DrawString,
    Gpu_SetCode6C,
    Gpu_SetSemiTrans,
    Gfx_CommitPrim,
};
Callees g = kOriginals;

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

}  // namespace
}  // namespace msgbox

using namespace msgbox;

// original 0x4976D0 (PSX 0x8015034C). Opens message `id` of the script pool:
// the pool header's u16 at 2*id is the message's offset from the pool base.
//
// As the original has it: the id is taken as a WORD and sign-extended for the
// header index (movsx eax, si), so ids 0x8000..0xFFFF read below the pool -
// no bounds check either way. The message index is stored AFTER MsgBox_Reset,
// which reads the pointer this just wrote.
extern "C" void __cdecl Msg_OpenScript(unsigned short id) {
    const int index = static_cast<short>(id);
    const std::uint32_t off = W(kScriptPool + static_cast<std::uint32_t>(2 * index));
    const unsigned char* const at = At(kScriptPool) + off;
    SetPtr(kBase, at);
    SetPtr(kAt, at);
    g.reset();
    SetW(kMessage, id);
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
    SetB(kSubState, 0);
    SetB(kEffectKind, 0);
    SetB(kEffectPhase, 0);
    SetB(kOffsetX, 0);
    SetB(kOffsetY, 0);
    SetW(kEffectTimer, 0);
    if (at[0] == 0x0C) {
        ++at;
        SetPtr(kAt, at);
        const unsigned char placement = at[0];
        ++at;
        SetB(kPlacement, placement);
        SetPtr(kAt, at);
    } else {
        SetB(kPlacement, 0);
    }
    SetB(kDelay, 0);
    SetB(kStepCount, 0);
    SetW(kEffectOff, 0);
    SetB(kColorHigh, 0);
    g.window_alloc(0, 0);
    SetB(kWindow0Kind, 2);
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
    const int x = static_cast<short>(W(kWindow0X)) >> 4;
    const int y = static_cast<short>(W(kWindow0Y)) >> 4;
    MsgBox_LineX = static_cast<short>(x + 10);
    SetW(kOriginY, static_cast<unsigned>(y + 6));
    g.dispatch();
    g.effect_task();
    if ((B(kFlags) & 4) == 0) g.step();
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
extern "C" void __cdecl MsgBox_Step(void) {
    unsigned char flags = 0;                 // bl: bit 0 in a substitution, bit 5 the offset pen
    const unsigned char* at = Ptr(kBase);
    const unsigned char* resume = nullptr;
    unsigned char sub_count = 0;
    unsigned char drawn = 0;
    unsigned char color = 0;
    short pen_x = MsgBox_LineX;
    auto pen_y = static_cast<std::uint16_t>(W(kOriginY));
    MsgBox_PenX = pen_x;
    SetW(kPenY, pen_y);
    if (B(kStepCount) == 0) return;
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
                SetW(kPenY, pen_y);
                break;
            case 0x03: {
                flags = static_cast<unsigned char>(flags | 1u);
                resume = at;
                const unsigned member = B(kNameMap + B(kNameIndex));
                sub_count = 9;
                at = At(kCharRecords + 164u * member);
                break;
            }
            case 0x04: {
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned member = at[0];
                sub_count = 9;
                at = At(kCharRecords + 164u * member);
                break;
            }
            case 0x05:
                ++at;
                color = at[0];
                break;
            case 0x06:
                color = 0;
                break;
            case 0x07: {
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned record = at[0];
                sub_count = 0x21;
                at = At(kTextRecords + (record << 5));
                break;
            }
            case 0x08: {
                flags = static_cast<unsigned char>(flags | 1u);
                ++at;
                resume = at;
                const unsigned entry = at[0];
                const unsigned char* const pool = At(kScriptPool + L(kPoolSelector));
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
                    g.effect_draw(color, at);
                } else {
                    const auto clut = static_cast<unsigned char>((B(kColorHigh) << 4) + color);
                    const int x = static_cast<short>(MsgBox_PenX + static_cast<signed char>(B(kOffsetX)));
                    const int y = static_cast<short>(pen_y + static_cast<signed char>(B(kOffsetY)));
                    g.draw_char(x, y, clut, 1, at);
                }
            } else {
                const auto clut = static_cast<unsigned char>((B(kColorHigh) << 4) + color);
                g.draw_char(pen_x, pen_y, clut, 1, at);
            }
            if (at[0] & 0x80) ++at;
            drawn = static_cast<unsigned char>(drawn + 1);
            pen_x = MsgBox_PenX;
            pen_y = W(kPenY);
            pen_x = static_cast<short>(pen_x + 12);
            MsgBox_PenX = pen_x;
        }
        ++at;
        if (B(kStepCount) == drawn) return;
    }
}

// original 0x497AD0 (PSX 0x801508EC, which copies the same eight pointers
// from 0x80149A5C onto its own stack). One frame of the box's state machine.
extern "C" void __cdecl MsgBox_StateDispatch(void) {
    Call(g.state, 8, B(kState), "MsgBox_StateDispatch");
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
extern "C" void __cdecl MsgBox_StatePrint(void) {
    const unsigned char* at = Ptr(kAt);
    const unsigned char* resume = Ptr(kResume);
    bool again = true;
    do {
        if (B(kFlags) & 1) {
            const auto left = static_cast<unsigned char>(B(kSubCount) - 1);
            SetB(kSubCount, left);
            if (left == 0) {
                SetW(kFlags, W(kFlags) & 0xFFFEu);
                at = resume + 1;
                SetPtr(kAt, at);
            }
        }
        const unsigned char c = at[0];
        bool glyph = c > 0x16;
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
                const unsigned member = B(kNameMap + B(kNameIndex));
                SetB(kSubCount, 9);
                at = At(kCharRecords + 164u * member);
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
                SetB(kSubCount, 9);
                at = At(kCharRecords + 164u * member);
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
                SetB(kSubCount, 0x21);
                at = At(kTextRecords + (record << 5));
                break;
            }
            case 0x08: {
                SetB(kFlags, B(kFlags) | 1u);
                ++at;
                resume = at;
                SetPtr(kAt, at);
                SetPtr(kResume, resume);
                const unsigned entry = at[0];
                const unsigned char* const pool = At(kScriptPool + L(kPoolSelector));
                SetPtr(kAt, pool);
                std::uint16_t off;
                std::memcpy(&off, pool + 2 * entry, sizeof off);
                SetB(kSubCount, 0x11);
                at = pool + off;
                break;
            }
            case 0x0A: {
                ++at;
                SetPtr(kAt, at);
                const auto id = static_cast<std::uint16_t>(at[0] + 0x200);
                g.sound(id);
                at = Ptr(kAt);
                resume = Ptr(kResume);
                break;
            }
            case 0x0B: {
                const auto delay = static_cast<unsigned char>(B(kDelay) + 8);
                SetB(kState, 1);
                again = false;
                SetB(kDelay, delay);
                break;
            }
            case 0x0F: {
                ++at;
                SetPtr(kAt, at);
                SetB(kEffectPhase, 0);
                SetB(kSubState, 0);
                SetB(kEffectKind, B(kEffectTable + 4u * at[0]));
                SetW(kEffectTimer, W(kEffectTable + 2 + 4u * at[0]));
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
                SetB(kChoiceId, at[0]);
                at += 2;
                SetPtr(kAt, at);
                SetB(kChoiceLast, static_cast<unsigned char>((at[0] & 0x0F) - 1));
                const auto column = static_cast<unsigned char>(at[0] >> 4);
                SetB(kChoiceKind, column);
                SetB(kState, column == 0 ? 5 : 4);
                break;
            }
            case 0x16: {
                ++at;
                again = false;
                SetPtr(kAt, at);
                SetB(kState, 7);
                SetB(kDelay, at[0]);
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
                const unsigned speed = (static_cast<unsigned char>(Input_Held) & 0x20) ? 0 : 1;
                const auto row = static_cast<signed char>(B(kTextSpeed));
                SetB(kDelay, B(static_cast<std::uint32_t>(kSpeedTable + speed + row)));
                if (L(kFlags) & 0x100u) {
                    SetB(kDelay, 3);
                    SetB(kState, 1);
                } else {
                    SetB(kState, 0);
                    if (B(kDelay) != 0) SetB(kState, 1);
                }
                again = false;
            }
            SetB(kStepCount, B(kStepCount) + 1);
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
    const auto left = static_cast<unsigned char>(B(kDelay) - 1);
    SetB(kDelay, left);
    if (left == 0) SetB(kState, 0);
}

// original 0x497EB0. State 2, the wait at the end of a message: two entries
// on 0x7DEE41, built on the stack.
extern "C" void __cdecl MsgBox_State2(void) { Call(g.sub2, 2, B(kSubState), "MsgBox_State2"); }

// original 0x497EE0. Any button pressed - or either kept flag 0x40 / 0x80
// still set - closes window 0 and holds the printer (flag 4).
extern "C" void __cdecl MsgBox_State2Press(void) {
    if (Input_Pressed == 0 && (B(kFlags) & 0xC0) == 0) return;
    SetB(kWindow0State, B(kWindow0State) + 1);
    const auto next = static_cast<unsigned char>(B(kSubState) + 1);
    SetB(kFlags, B(kFlags) | 4u);
    SetB(kSubState, next);
}

// original 0x497F20, shared by state 2 and state 7: once window 0 reports 4
// the box is done (flag 2), which is what MsgBox_FrameTask returns.
extern "C" void __cdecl MsgBox_State2Closed(void) {
    if (B(kWindow0State) == 4) SetB(kFlags, B(kFlags) | 2u);
}

// original 0x497F40. State 3, a page break: two entries on 0x7DEE41.
extern "C" void __cdecl MsgBox_State3(void) { Call(g.sub3, 2, B(kSubState), "MsgBox_State3"); }

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
        const auto next = static_cast<unsigned char>(B(kSubState) + 1);
        SetB(kColorHigh, 0);
        SetB(kSubState, next);
    }
    const int y = static_cast<std::uint16_t>(W(kEffectOff) + W(kPenY)) + 0x0C;
    const int x = (B(kWindow0Rows) >> 1) + MsgBox_LineX - 0x0E;
    g.page_arrow(x, y);
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
    const unsigned char was = B(kDelay);
    SetB(kDelay, static_cast<unsigned char>(was - 1));
    if (was != 0) return;
    const auto row = static_cast<unsigned char>(B(kColorHigh) + 1);
    SetB(kDelay, 1);
    SetB(kColorHigh, row);
    if (row != 7) return;
    if (B(kState) == 4 || B(kState) == 5) {
        g.reopen();
        return;
    }
    const std::uint32_t at = L(kAt);
    SetW(kFlags, W(kFlags) & 0xFFF3u);
    SetB(kSubState, 0);
    SetB(kState, 1);
    SetB(kDelay, 8);
    SetL(kBase, at);
    SetW(kEffectOff, 0);
    SetB(kStepCount, 0);
    SetB(kColorHigh, 0);
}

// original 0x498050. State 4, a choice list in its own window: eight entries
// on 0x7DEE41, the first of them state 3's arrow.
extern "C" void __cdecl MsgBox_State4(void) { Call(g.sub4, 8, B(kSubState), "MsgBox_State4"); }

// original 0x4980B0. Claims window 1 as kind 1 (a choice list), tells window
// 0 to go to state 3, and holds the printer.
extern "C" void __cdecl MsgBox_ChoiceOpen(void) {
    SetB(kCursor, 0);
    g.window_alloc(1, 0);
    const auto next = static_cast<unsigned char>(B(kSubState) + 1);
    SetB(kFlags, B(kFlags) | 4u);
    SetB(kWindow1Kind, 1);
    SetB(kWindow0State, 3);
    SetB(kSubState, next);
}

// original 0x4980F0. Waits until window 1 is open and window 0 has finished
// its own move.
extern "C" void __cdecl MsgBox_ChoiceWaitOpen(void) {
    if (B(kWindow1State) == 2 && B(kWindow0State) == 4) SetB(kSubState, B(kSubState) + 1);
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
    const unsigned pressed = Input_Pressed;
    if ((Field_CancelButtons & pressed) != 0) {
        const auto next = static_cast<unsigned char>(B(kSubState) + 1);
        SetB(kWindow1State, 3);
        SetB(kWindow0State, 1);
        SetB(kSubState, next);
        return;
    }
    if ((Field_ConfirmButtons & pressed) != 0) {
        SetB(kSubState, B(kSubState) + 2);
        return;
    }
    const unsigned repeat = g.repeat(pressed);
    const unsigned char last = B(kChoiceLast);
    if ((repeat >> 8) & 0x10) {
        const auto up = static_cast<unsigned char>(B(kCursor) - 1);
        SetB(kCursor, up);
        if (static_cast<signed char>(up) < 0) SetB(kCursor, last);
    }
    if ((repeat >> 8) & 0x40) {
        const auto down = static_cast<unsigned char>(B(kCursor) + 1);
        SetB(kCursor, down);
        if (static_cast<signed char>(down) > static_cast<signed char>(last)) SetB(kCursor, 0);
    }
}

// original 0x4981A0. Once window 0 reports 2 the hold flag is toggled off and
// the state's own index goes back to 0.
extern "C" void __cdecl MsgBox_ChoiceWaitShut(void) {
    if (B(kWindow0State) != 2) return;
    SetW(kFlags, W(kFlags) ^ 4u);
    SetB(kSubState, 0);
}

// original 0x498230. Re-opens the message at the answer's own text and
// releases the hold.
extern "C" void __cdecl MsgBox_ChoiceReopen(void) {
    if (B(kWindow0State) != 2) return;
    SetB(kDelay, 0x10);
    g.reopen();
    SetW(kFlags, W(kFlags) ^ 4u);
}

// original 0x498250. Once window 1 reports 4 the box is finished: flag 0x80
// set (which MsgBox_Reset keeps only 0x40 of, so it is cleared on the next
// message), window 0 to state 4, and state 2 sub 1 - the wait for it to shut.
extern "C" void __cdecl MsgBox_ChoiceDone(void) {
    if (B(kWindow1State) != 4) return;
    SetB(kFlags, B(kFlags) | 0x80u);
    SetB(kWindow0State, 4);
    SetB(kState, 2);
    SetB(kSubState, 1);
}

// original 0x498280. Re-opens the message the box is on, from the top: the
// same lookup as Msg_OpenScript but with the id taken UNSIGNED, and without
// MsgBox_Reset - only the delay, the state and the two counters are set.
extern "C" void __cdecl MsgBox_Reopen(void) {
    const unsigned id = W(kMessage);
    SetB(kDelay, 8);
    SetB(kState, 1);
    const std::uint32_t off = W(kScriptPool + 2u * id);
    const unsigned char* const at = At(kScriptPool) + off;
    SetPtr(kBase, at);
    SetPtr(kAt, at);
    SetB(kStepCount, 0);
    SetB(kSubState, 0);
}

// original 0x4982C0. State 5, the same list drawn as a menu (code 0x14 with a
// zero high nibble): six entries on 0x7DEE41, the last of them state 3's
// scroll.
extern "C" void __cdecl MsgBox_State5(void) { Call(g.sub5, 6, B(kSubState), "MsgBox_State5"); }

// original 0x498310. Claims window 1 as kind 2 (a menu). Unlike the choice
// list's opener it does not touch window 0 or the hold flag.
extern "C" void __cdecl MsgBox_MenuOpen(void) {
    SetB(kCursor, 0);
    g.window_alloc(1, 0);
    const auto next = static_cast<unsigned char>(B(kSubState) + 1);
    SetB(kWindow1Kind, 2);
    SetB(kSubState, next);
}

// original 0x498340. Waits until window 1 reports 2.
extern "C" void __cdecl MsgBox_MenuWaitOpen(void) {
    if (B(kWindow1State) == 2) SetB(kSubState, B(kSubState) + 1);
}

// original 0x498350. The menu's input: confirm moves on, and the cursor moves
// on the auto-repeat of the pad masked to 0x5000 - up and down only, so a
// left or right press cannot restart the repeat timer here.
extern "C" void __cdecl MsgBox_MenuInput(void) {
    const unsigned pressed = Input_Pressed;
    if ((Field_ConfirmButtons & pressed) != 0) SetB(kSubState, B(kSubState) + 1);
    const unsigned repeat = g.repeat(pressed & 0x5000u);
    const unsigned char last = B(kChoiceLast);
    if ((repeat >> 8) & 0x10) {
        const auto up = static_cast<unsigned char>(B(kCursor) - 1);
        SetB(kCursor, up);
        if (static_cast<signed char>(up) < 0) SetB(kCursor, last);
    }
    if ((repeat >> 8) & 0x40) {
        const auto down = static_cast<unsigned char>(B(kCursor) + 1);
        SetB(kCursor, down);
        if (static_cast<signed char>(down) > static_cast<signed char>(last)) SetB(kCursor, 0);
    }
}

// original 0x498410. Once window 1 reports 4: a message id of 0xFFFF means
// there is nothing left to say and the box is finished; otherwise on to the
// next entry.
extern "C" void __cdecl MsgBox_MenuDone(void) {
    if (B(kWindow1State) != 4) return;
    if (W(kMessage) != 0xFFFF) {
        SetB(kSubState, B(kSubState) + 1);
        return;
    }
    SetB(kFlags, B(kFlags) | 0x80u);
    SetB(kState, 2);
    SetB(kSubState, 0);
}

// original 0x498450. State 6: back to state 0 once window 0 reports 2. No
// path in this file reaches it - nothing writes 6 to 0x7DEE40 (section 2).
extern "C" void __cdecl MsgBox_State6(void) {
    if (B(kWindow0State) == 2) SetB(kState, 0);
}

// original 0x498470. State 7, the wait code 0x16 asks for: two entries on
// 0x7DEE41, the second of them state 2's "window closed" test.
extern "C" void __cdecl MsgBox_State7(void) { Call(g.sub7, 2, B(kSubState), "MsgBox_State7"); }

// original 0x4984A0. Counts code 0x16's argument down; at zero window 0 is
// told to move on, the printer is held, and the state's index advances.
//
// As the original has it: the delay is tested BEFORE the decrement, so a 0
// does the work and leaves 0xFF.
extern "C" void __cdecl MsgBox_State7Delay(void) {
    const unsigned char was = B(kDelay);
    SetB(kDelay, static_cast<unsigned char>(was - 1));
    if (was != 0) return;
    const auto window = static_cast<unsigned char>(B(kWindow0State) + 1);
    const auto next = static_cast<unsigned char>(B(kSubState) + 1);
    SetB(kFlags, B(kFlags) | 4u);
    SetB(kWindow0State, window);
    SetB(kSubState, next);
}

// original 0x4984E0 (PSX 0x80151A1C). The box's effect, one frame: six
// entries on 0x7DEE42, which code 0x0F sets from the record table 0x658E98.
// Kind 0 is a bare `ret` at 0x437CC0 - the only entry the attract sequence
// ever reaches - and kinds 2 and 3 share one handler.
extern "C" void __cdecl MsgBox_EffectTask(void) {
    Call(g.effect, 6, B(kEffectKind), "MsgBox_EffectTask");
}

// original 0x498520. Effect 1: the pen x offset rocks between -2 and +2, two
// units a frame, until the effect's own timer runs out.
//
// As the original has it: the timer is only decremented when it is not
// 0xFFFF (which means forever), and the kind is cleared when the timer READS
// zero - including the 0xFFFF case's zero, which it cannot be.
extern "C" void __cdecl MsgBox_EffectShake(void) {
    Call(g.shake, 2, B(kEffectPhase), "MsgBox_EffectShake");
    std::uint16_t left = W(kEffectTimer);
    if (left != 0xFFFF) {
        left = static_cast<std::uint16_t>(left - 1);
        SetW(kEffectTimer, left);
    }
    if (left == 0) SetB(kEffectKind, 0);
}

// original 0x498560 / 0x498580: out to -2, then back to +2, then out again.
extern "C" void __cdecl MsgBox_ShakeOut(void) {
    if (B(kOffsetX) == 0xFE) {
        SetB(kEffectPhase, B(kEffectPhase) + 1);
        return;
    }
    SetB(kOffsetX, B(kOffsetX) + 0xFE);
}
extern "C" void __cdecl MsgBox_ShakeBack(void) {
    if (B(kOffsetX) == 2) {
        SetB(kEffectPhase, B(kEffectPhase) - 1);
        return;
    }
    SetB(kOffsetX, B(kOffsetX) + 2);
}

// original 0x4985A0, effects 2 and 3: the effect draw's own offset 0x7DEE68
// walks to its limit and back. Two entries on 0x7DEE43, then the timer down
// unless it is 0xFFFF.
extern "C" void __cdecl MsgBox_EffectGrow(void) {
    Call(g.grow, 2, B(kEffectPhase), "MsgBox_EffectGrow");
    if (W(kEffectTimer) != 0xFFFF) SetW(kEffectTimer, W(kEffectTimer) - 1);
}

// original 0x4985E0. Turns the effect draw on (flag 8) and, once the timer
// has reached zero, sets it to 1 and moves to the second phase.
extern "C" void __cdecl MsgBox_GrowStart(void) {
    SetB(kFlags, B(kFlags) | 8u);
    if (W(kEffectTimer) != 0) return;
    const auto next = static_cast<unsigned char>(B(kEffectPhase) + 1);
    SetW(kEffectTimer, 1);
    SetB(kEffectPhase, next);
}

// original 0x498610. Kind 2 walks the offset down by 2 and stops when it goes
// negative; every other kind walks it up by 2 and stops when it goes
// positive. Stopping clears flag 8, the offset and the kind. Either way the
// timer is put back to 1, so this runs every frame.
extern "C" void __cdecl MsgBox_GrowStep(void) {
    if (W(kEffectTimer) != 0) return;
    bool stop;
    if (B(kEffectKind) == 2) {
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
        SetB(kEffectKind, 0);
    }
    SetW(kEffectTimer, 1);
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
    if (B(kEffectPhase) == 0) {
        if (B(kOffsetX) == 0xFD) {
            SetB(kEffectPhase, 1);
            return;
        }
        x = static_cast<unsigned char>(B(kOffsetX) - 1);
    } else {
        if (B(kOffsetX) == 3) {
            SetB(kEffectPhase, B(kEffectPhase) - 1);
            return;
        }
        x = static_cast<unsigned char>(B(kOffsetX) + 1);
    }
    const bool waiting = W(kEffectWait) != 0;
    SetB(kOffsetX, x);
    if (waiting) {
        SetW(kEffectWait, W(kEffectWait) - 1);
        return;
    }
    const auto y = static_cast<unsigned char>(B(kOffsetY) + static_cast<unsigned char>(W(kEffectOff)));
    SetB(kOffsetY, y);
    if (y == 0xF6) {
        SetW(kEffectWait, 0x0F);
        SetW(kEffectOff, 1);
        return;
    }
    if (y != 0) {
        SetW(kEffectWait, 3);
        return;
    }
    const std::uint16_t left = W(kEffectTimer);
    SetW(kEffectWait, 0x0F);
    SetW(kEffectOff, 0xFFFF);
    if (left == 0xFFFF) return;
    SetW(kEffectTimer, static_cast<std::uint16_t>(left - 1));
    if (left != 0) return;
    SetB(kEffectKind, 0);
    SetB(kOffsetY, 0);
    SetB(kOffsetX, 0);
}

// original 0x498740. Effect 5: two entries on 0x7DEE43 - a set-up, then a
// per-frame integration of 0x7DEE68 into the y offset.
extern "C" void __cdecl MsgBox_EffectRise(void) { Call(g.rise, 2, B(kEffectPhase), "MsgBox_EffectRise"); }

// original 0x498770. The record's frame count becomes the accumulator's
// starting value and its offset is scaled by 16 - an 8.8 fixed-point speed.
extern "C" void __cdecl MsgBox_RiseStart(void) {
    const std::uint16_t frames = W(kEffectTimer);
    SetW(kEffectOff, W(kEffectOff) << 4);
    SetW(kEffectWait, frames);
    SetB(kEffectPhase, B(kEffectPhase) + 1);
}

// original 0x4987A0. Adds the speed to the accumulator every frame and the
// accumulator's HIGH byte - before the add - to the y offset; once the y
// offset is no longer negative it and the kind are cleared, which ends the
// effect. As the original has it, the high byte is taken with `sar edx, 8`
// on a register whose upper half is stale, which cannot reach dl.
extern "C" void __cdecl MsgBox_RiseStep(void) {
    const std::uint16_t was = W(kEffectWait);
    const auto y = static_cast<unsigned char>(B(kOffsetY) + static_cast<unsigned char>(was >> 8));
    SetW(kEffectWait, static_cast<std::uint16_t>(was + W(kEffectOff)));
    SetB(kOffsetY, y);
    if (static_cast<signed char>(y) < 0) return;
    SetB(kOffsetY, 0);
    SetB(kEffectKind, 0);
}

// original 0x59E2D0 (PSX 0x80159874). Claims window record `slot` (stride
// 0x24 off 0x803160) for `kind` if its first byte is free.
//
// As the original has it: the slot is masked to a byte for the index but the
// ANSWER is the argument dword - unchanged on success, with its low byte
// forced to 0xFF on failure (`or al, 0xFF`), so a caller that passes a slot
// with high bytes set gets them back. No bound on the slot.
extern "C" unsigned __cdecl Window_Alloc(unsigned slot, unsigned kind) {
    unsigned char* const rec = At(kWindows + (slot & 0xFF) * 0x24u);
    if (rec[0] != 0) return slot | 0xFFu;
    rec[0] = 1;
    rec[1] = static_cast<unsigned char>(kind);
    rec[2] = 0;
    rec[3] = 0;
    return slot;
}

// original 0x516B30. The port's general text draw, 341 callers: it plants the
// pen and hands the string to Text_DrawString.
//
// As the original has it: x is taken as a WORD and stored to both the pen and
// the line start; y is incremented as a dword and stored as a word, so a y of
// 0xFFFF draws at 0; the colour and the count go on unchanged.
extern "C" const unsigned char* __cdecl Text_DrawAt(int x, int y, int color, int count,
                                                    const unsigned char* text) {
    Text_PenX = static_cast<short>(x);
    Text_LineX = static_cast<short>(x);
    Text_PenY = static_cast<short>(y + 1);
    return g.draw_string(static_cast<unsigned>(color), static_cast<unsigned>(count), text);
}

// original 0x516D50. One glyph: fills the POLY_FT4-shaped record at
// Gfx_PacketNext and hands it on. Its only caller is Text_DrawString, as
// (pen x, pen y, 12, 12 - v, u, v, clut).
//
// As the original has it: the texture window is a flat 12 units wide (0 and
// 12 in every u) and `u` is the row - what DIV-0010 named, in the config
// screen's words, "the texture extent fixed at 0xC whatever the quad"; the
// three shade bytes are 0x80; the v coordinates are u and (u - v + 12) as
// BYTES, so they wrap; the corners are (x, y + u), (x + w, y + u),
// (x, y + h + u), (x + w, y + h + u), each stored as a word; the primitive
// pointer is re-read from Gfx_PacketNext before every single store, which
// nothing between them can change; +0x16 (the glyph index) is the caller's.
//
// This is faithful - no divergence. DIV-0010 is the config screen's own
// stand-in for Text_DrawAt (src/game/config_text.cpp), not a change here.
extern "C" void __cdecl Text_EmitGlyph(int x, int y, int w, int h, int u, int v, int clut) {
    unsigned char* const p = Gfx_PacketNext;
    const auto row = static_cast<unsigned char>(u);
    const auto far_row = static_cast<unsigned char>(u - v + 12);
    const auto put = [](unsigned char* at, unsigned value) {
        const auto word = static_cast<std::uint16_t>(value);
        std::memcpy(at, &word, sizeof word);
    };
    put(p + 0x0E, static_cast<unsigned>(clut));
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    p[0x0C] = 0;
    p[0x0D] = row;
    p[0x14] = 0x0C;
    p[0x15] = row;
    p[0x1C] = 0;
    p[0x1D] = far_row;
    p[0x24] = 0x0C;
    p[0x25] = far_row;
    put(p + 0x08, static_cast<unsigned>(x));
    put(p + 0x0A, static_cast<unsigned>(y + u));
    put(p + 0x10, static_cast<unsigned>(x + w));
    put(p + 0x12, static_cast<unsigned>(y + u));
    put(p + 0x18, static_cast<unsigned>(x));
    put(p + 0x1A, static_cast<unsigned>(y + h + u));
    put(p + 0x20, static_cast<unsigned>(x + w));
    put(p + 0x22, static_cast<unsigned>(y + h + u));
    g.set_code6c(p);
    g.set_semitrans(p, 1);
    g.commit(1, 0x28);
}

void MsgBox_Inject() {
    if (bof3::WantsShadow("msgbox")) msgbox::SelfTest();
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
    BOF3_INJECT(Window_Alloc);
    BOF3_INJECT(Text_DrawAt);
    BOF3_INJECT(Text_EmitGlyph);
}
