// Moved here by tools/move_funcs.py from field_o.cpp, move_cmds.cpp (docs/source-layout-round.md).
#include "game/field/move_cmd.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_o.h"
#include "game/field_o_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/move_cmds.h"
#include "game/move_cmds_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_o::at;

}  // namespace

namespace {

using namespace move_script;

using namespace move_cmds;

}  // namespace

namespace {

constexpr auto& AtFieldO = move_script::At;

using move_script::Long;

using U = std::uint32_t;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Fam() { return Field_ActiveMember; }

std::int32_t Sb(unsigned v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// A handler read in place from a .data table: an entry outside .text is not
// code - the original jumps there; ours aborts (round nine's rule). While the
// fuzz runs, the table holds the harness's recorders, which live outside .text.
U CodeAt(U table, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = static_cast<U>(Long(AtFieldO(cell)));
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}

std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

}  // namespace

// original 0x579390: detach (PSX FUN_801ACEE0): +0x24 bit 5 cleared, and for
// the object kind 1 +2 too.
extern "C" void __cdecl MoveCmd_Detach(void) {
    rv::Current<records::SpriteHead>()->draw_flags &= 0xDF;
    if (move_cmds::g.object_kind() == 1) Sprite_Current[2] = 0;
}

// original 0x5793B0: the timed move to where the attachment puts the sprite
// (PSX FUN_801AD0A0): MoveCmd_HandlePosition(+0x18, +0x1C), then over n = t
// frames (0 taken as 1) the velocity to +0xC / +0x10 / +0x14 and n to +9.
// As the original has it: n is SIGNED - t from 0x80 divides by a negative
// count, and 0xFF by -1, which faults on a difference of exactly 0x80000000
// as the original's idiv does; +9 gets the byte; the position is read through
// the pointer MoveCmd_HandlePosition returns.
extern "C" void __cdecl MoveCmd_AttachMove(unsigned char t) {
    const unsigned char offset = Sprite_Current[0x1C];
    const unsigned char handle = Sprite_Current[0x18];
    long buffer[4];
    const long* const at = move_cmds::g.handle_position(buffer, handle, offset);
    const std::int32_t x = at[0], z = at[1], y = at[2];
    const unsigned char count = t ? t : 1;
    rv::Current<records::SpriteHead>()->step_frames = count;
    const std::int32_t n = static_cast<signed char>(count);
    rv::Current<records::SpriteHead>()->step_x = Sub(x, rv::Current<records::SpriteHead>()->x) / n;
    rv::Current<records::SpriteHead>()->step_z = Sub(z, rv::Current<records::SpriteHead>()->z) / n;
    rv::Current<records::SpriteHead>()->step_y = Sub(y, static_cast<short>(rv::Current<records::SpriteHead>()->height_whole)) / n;
}

// original 0x579450: the position of label `label` in `script` (PSX
// FUN_801AD8F0), when bit 0x4000 of the context's position asks for one;
// otherwise the position itself. The walk from 0 steps F8 07 by 5, other F8 by
// 2, 0E / 0F by 5 with bit 1 (mask 2) of their fourth byte and 4 without, and
// every other op by MoveScript_OpLengths.
// As the original has it: 16-bit positions that wrap; no bound - a label not
// there, or an op of length 0 before it, never returns.
extern "C" unsigned short __cdecl MoveScript_FindLabel(unsigned char* context, unsigned char label, const unsigned char* script) {
    const std::uint16_t position = Pos(context);
    if (!(position & 0x4000)) return position;
    std::uint16_t at = 0;
    for (;;) {
        const unsigned char* const op = script + at;
        if (op[0] == 0x0A && op[1] == label) return at;
        if (op[0] == 0xF8) at = static_cast<std::uint16_t>(at + (op[1] == 7 ? 5 : 2));
        else if (op[0] == 0x0E || op[0] == 0x0F) at = static_cast<std::uint16_t>(at + ((op[3] & 2) ? 5 : 4));
        else at = static_cast<std::uint16_t>(at + MoveScript_OpLengths[op[0]]);
    }
}

// original 0x5794D0: op 88 (MoveScript_Group8 0x578906): Sprite_Current +4
// picks the state from 0x663AFC - MoveCmd_Op88Start, MoveCmd_Op88Fade.
// As the original has it: the byte is unchecked - 2 and 3 reach op 87's
// states (the next table); past them the words are not code (ours aborts).
extern "C" void __cdecl MoveCmd_Op88(void) {
    reinterpret_cast<void (__cdecl*)()>(CodeAt(::bof3::Img(MoveCmd_Op88States_addr), Sc()[4], "MoveCmd_Op88"))();
}

// original 0x5794F0: op 88's state 0 - a tint record from Sprite_SetTint(the
// sprite, 0, 0, 0, 1) kept in Field_ActiveMember +0x9F, the sprite's +0 bit 5,
// its colour 0xC0 0xC0 0xC0 with +0x5C 1, the wait +0x8A of Field_ActiveMember
// less 1, state 1.
extern "C" void __cdecl MoveCmd_Op88Start(void) {
    const unsigned char n = SH_CALL(Sprite_SetTint)(Sc(), 0, 0, 0, 1);
    Fam()[0x9F] = n;
    rv::As<records::SpriteHead>(Sc())->flags |= 0x20;
    rv::As<records::SpriteHead>(Sc())->shade[2] = 0xC0;
    rv::As<records::SpriteHead>(Sc())->shade[1] = 0xC0;
    rv::As<records::SpriteHead>(Sc())->shade[0] = 0xC0;
    rv::As<records::SpriteHead>(Sc())->draw_mode = 1;
    rv::As<records::SpriteObject>(Fam())->script_position = rv::As<records::SpriteObject>(Fam())->script_position - 1u;
    Sc()[4] = 1;
}

// original 0x579560: op 88's state 1 - each of the colour bytes +0x5D..+0x5F
// not yet 0x80 less 4; all three at 0x80: Tint_Release(Field_ActiveMember
// +0x9F), +0 bit 6, state 0; else the wait less 1.
// As the original has it: a byte that is not a multiple of 4 from 0x80 steps
// past it and wraps round.
extern "C" void __cdecl MoveCmd_Op88Fade(void) {
    for (unsigned c = 0x5D; c <= 0x5F; ++c)
        if (Sc()[c] != 0x80) Sc()[c] = static_cast<unsigned char>(Sc()[c] - 4);
    records::SpriteHead* const sc = rv::As<records::SpriteHead>(Sc());
    if (sc->shade[0] == 0x80 && sc->shade[1] == 0x80 && sc->shade[2] == 0x80) {
        SH_CALL(Tint_Release)(Fam()[0x9F]);
        rv::As<records::SpriteHead>(Sc())->flags |= 0x40;
        Sc()[4] = 0;
        return;
    }
    rv::As<records::SpriteObject>(Fam())->script_position = rv::As<records::SpriteObject>(Fam())->script_position - 1u;
}

// original 0x5795F0: op 87 (MoveScript_Group8 0x5788F5): Sprite_Current +4
// picks the state from 0x663B04 - MoveCmd_Op87Start, MoveCmd_Op87Fade.
// As the original has it: the byte is unchecked; past 1 the words are
// EventScript_OpLengths' bytes, not code (ours aborts).
extern "C" void __cdecl MoveCmd_Op87(void) {
    reinterpret_cast<void (__cdecl*)()>(CodeAt(::bof3::Img(MoveCmd_Op87States_addr), Sc()[4], "MoveCmd_Op87"))();
}

// original 0x579610: op 87's state 0 - the tint record kept as op 88's, the
// sprite's +0 bit 5 set and bit 6 cleared, +0x5C 1, the colour 0x80 0x80 0x80,
// state 1, the wait less 1.
extern "C" void __cdecl MoveCmd_Op87Start(void) {
    const unsigned char n = SH_CALL(Sprite_SetTint)(Sc(), 0, 0, 0, 1);
    Fam()[0x9F] = n;
    rv::As<records::SpriteHead>(Sc())->flags |= 0x20;
    rv::As<records::SpriteHead>(Sc())->flags &= 0xBF;
    rv::As<records::SpriteHead>(Sc())->draw_mode = 1;
    rv::As<records::SpriteHead>(Sc())->shade[2] = 0x80;
    rv::As<records::SpriteHead>(Sc())->shade[1] = 0x80;
    rv::As<records::SpriteHead>(Sc())->shade[0] = 0x80;
    Sc()[4] = 1;
    rv::As<records::SpriteObject>(Fam())->script_position = rv::As<records::SpriteObject>(Fam())->script_position - 1u;
}

// original 0x579690: op 87's state 1 - each colour byte below 0xC0 as a
// signed byte (0x80..0xBF) plus 4; all three at 0xC0: Tint_Release, +0 bit 5
// cleared, +0x5C..+0x5F and the state 0; else the wait less 1.
extern "C" void __cdecl MoveCmd_Op87Fade(void) {
    for (unsigned c = 0x5D; c <= 0x5F; ++c)
        if (Sb(Sc()[c]) < Sb(0xC0)) Sc()[c] = static_cast<unsigned char>(Sc()[c] + 4);
    records::SpriteHead* const sc = rv::As<records::SpriteHead>(Sc());
    if (sc->shade[0] == 0xC0 && sc->shade[1] == 0xC0 && sc->shade[2] == 0xC0) {
        SH_CALL(Tint_Release)(Fam()[0x9F]);
        rv::As<records::SpriteHead>(Sc())->flags &= 0xDF;
        rv::As<records::SpriteHead>(Sc())->draw_mode = 0;
        rv::As<records::SpriteHead>(Sc())->shade[2] = 0;
        rv::As<records::SpriteHead>(Sc())->shade[1] = 0;
        rv::As<records::SpriteHead>(Sc())->shade[0] = 0;
        Sc()[4] = 0;
        return;
    }
    rv::As<records::SpriteObject>(Fam())->script_position = rv::As<records::SpriteObject>(Fam())->script_position - 1u;
}

void MoveCmd_Inject() {
    BOF3_INJECT(MoveCmd_Detach);
    BOF3_INJECT(MoveCmd_AttachMove);
    BOF3_INJECT(MoveScript_FindLabel);
    BOF3_INJECT(MoveCmd_Op88);
    BOF3_INJECT(MoveCmd_Op88Start);
    BOF3_INJECT(MoveCmd_Op88Fade);
    BOF3_INJECT(MoveCmd_Op87);
    BOF3_INJECT(MoveCmd_Op87Start);
    BOF3_INJECT(MoveCmd_Op87Fade);
}
