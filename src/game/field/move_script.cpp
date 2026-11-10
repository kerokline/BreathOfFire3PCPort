// Moved here by tools/move_funcs.py from field_o.cpp, field_objects.cpp, move_cmds.cpp, move_groups.cpp, move_script.cpp (docs/source-layout-round.md).
#include "game/field/move_script.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/field_o.h"
#include "game/field_o_callees.h"
#include "game/field_objects.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/move_cmds_callees.h"
#include "game/move_groups.h"
#include "game/move_script.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/sound_sp.h"
#include "game/wide_staging.h"
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

using move_script::SetLong;

using U = std::uint32_t;

U Key(const void* p) { return Low32(p); }

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Fam() { return Field_ActiveMember; }

std::int32_t Sb(unsigned v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

using AreaHandler = void (__cdecl*)();

// Where a generated data name lives (its name is a macro, so bof3::addr:: cannot be used with it).
std::uint32_t Address(const volatile void* p) { return Low32(p); }

// An 8.8 operand pair as the C and D groups add it: the high byte signed, the
// two shifted into bits 8..23 of a 16.16 value.
std::int32_t Fixed88(const unsigned char* at) {
    return static_cast<std::int32_t>((static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(at[0]))) << 8 | at[1]) << 8);
}

void AddLong(unsigned char* at, std::int32_t v) { SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + static_cast<std::uint32_t>(v))); }

void SubLong(unsigned char* at, std::int32_t v) { SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) - static_cast<std::uint32_t>(v))); }

// A signed 16-bit value from a big-endian operand pair, as `mov ah, hi; mov al, lo; movsx`.
std::int32_t S16(const unsigned char* at) { return static_cast<std::int16_t>(at[0] << 8 | at[1]); }

// Ops D4..D6: the party slot Field_ActiveMember is, as a byte - the original
// divides the pointer's distance from Sprite_ObjectsExtra by 0xA4, truncating.
unsigned char PartySlot() {
    const auto distance = static_cast<std::int32_t>(Low32(Field_ActiveMember.Native()) - Address(rv::Bytes(Sprite_ObjectsExtra)));
    return static_cast<unsigned char>(distance / 0xA4);
}

unsigned char* PartyRecord(int slot) { return MoveScript_PartyRecords + slot * 16; }

unsigned char* TintRecord(unsigned char n) { return MoveScript_TintRecords + n * 12u; }

unsigned char* ActiveMember() { return Field_ActiveMember; }

// Ops C8 and CE: the sprite's direction one step toward a target, then its
// facing (kind 1: the animation itself), and the op repeats.
unsigned char TurnStep(unsigned char* object, unsigned char direction, unsigned char mode) {
    rv::Current<records::SpriteHead>()->direction = direction;
    if (move_groups::g.kind() == 1) move_groups::g.set_animation(rv::Current<records::SpriteHead>()->direction);
    else move_groups::g.face(rv::Current<records::SpriteHead>()->direction);
    object[1] = mode;
    SetPos(object, Pos(object) - 1u);
    return rv::Current<records::SpriteHead>()->direction;
}

// Sprite_Current may be a Sprite_Objects record or another family whose
// first 0x80 bytes are a sprite object's (an ObjTrio member, the kind-2
// object), so only that part is named: rv::Current<records::SpriteHead>()
// (C2 audit frs F3; typed-data-round.md 3, "C2 audit as run").
using CounterOp = signed char (__cdecl*)(const unsigned char*);

// Ops 0D and F3 place an object: the coordinate words of Sprite_Current, or
// of Sprite_Kind2 itself on the kind-2 path.
void PlaceAndGround(unsigned char* sprite, const unsigned char* operands) {
    rv::Store<unsigned>(rv::As<records::SpriteHead>(sprite)->x_whole, operands[1]);
    rv::Store<unsigned>(rv::As<records::SpriteHead>(sprite)->x_frac, operands[2] ? 0x8000u : 0u);
    rv::Store<unsigned>(rv::As<records::SpriteHead>(sprite)->z_whole, operands[3]);
    rv::Store<unsigned>(rv::As<records::SpriteHead>(sprite)->z_frac, operands[4] ? 0x8000u : 0u);
}

}  // namespace

namespace move_script {
namespace {

const CalleesMoveScript kOriginalsMoveScript = {
    MoveScript_ObjectKind, MoveCmd_Move, MoveCmd_MoveKind2,
    MoveScript_Group6, MoveScript_Group8, MoveScript_Group9, MoveScript_GroupC, MoveScript_GroupD, MoveScript_GroupE,
    MoveScript_GroupB, MoveScript_FindLabel, MoveScript_Variable, ObjTrio_ClearBit40, ObjTrio_SetBit40,
    AreaMap_Elevation, Sprite_SetAnimation, Sprite_SetAnimationAt, ScriptFlags_Clear40, ScriptFlags_Set40,
    MoveCmd_OpF9, Sprite_ScriptTickOnce, Sprite_ScriptPeek, Sound_PlayEffect, MoveCmd_OpF7,
    MoveCmd_Attach, MoveCmd_TestFB, MoveCmd_TestFC,
};

}  // namespace

CalleesMoveScript gMoveScript = kOriginalsMoveScript;

}  // namespace move_script

// move_groups's callee seam, moved here from move_groups.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace move_groups {
namespace {

const move_groups::CalleesMoveGroups kOriginalsMoveGroups = {
    MoveScript_ObjectKind, Sprite_FaceDirection, Field_ObjectTalk, MoveCmd_OpE7, MoveCmd_OpE9,
    Sprite_SetTint, Sprite_ReleaseTint, Sprite_SetAnimation, Sprite_SetAnimationBank, AreaMap_Elevation,
    MapView_SetElevation, Sprite_ScriptPeek, Sound_PlayById, Sound_PlayEffect, Music_Play, Music_FadeOutStop,
    Music_FadeIn, Music_FadeOut, Port_DroppedCall, MoveCmd_OpDB, MoveCmd_TestFB, MoveCmd_TestFC, Effect_Spawn,
    Effect_SpawnAt, MoveCmd_HandlePosition, MoveCmd_Op87, MoveCmd_Op88, Sound_LoadStream, Sound_StreamDone,
};

}  // namespace

CalleesMoveGroups g = kOriginalsMoveGroups;

}  // namespace move_groups

// original 0x576B50: one step of an object's movement script (PSX
// FUN_801A9D38). The flow pass first, then one op by its high nibble, then
// position + 1 - read again after the op, since every handler may move it.
// Returns al: the flow pass's value, the op's group for 0x10-0x4F, or what
// the group handler returned.
extern "C" unsigned char __cdecl MoveScript_Step(unsigned char* object, const unsigned char* script) {
    const unsigned char start = rv::Current<records::SpriteHead>()->direction;
    MoveScript_Object = object;
    unsigned char result = MoveScript_Flow(object, script, start);
    const std::uint16_t pos = Pos(object);
    const unsigned char op = script[pos];
    if (wide_staging::Step(object, script, result)) return result;   // DIV-0122 / 0123: a site's walk, rewritten (wide only)
    switch (op & 0xF0) {
    case 0x10: case 0x20: case 0x30: case 0x40: {   // move in direction (op - 0x10) >> 3, op & 7 steps
        result = static_cast<unsigned char>(static_cast<unsigned char>(op - 0x10) >> 3);
        object[7] = op & 7;
        if (op & 7) {
            if (move_script::gMoveScript.kind() == 2) move_script::gMoveScript.move_kind2(result);
            else move_script::gMoveScript.move(object, result);
        }
        break;
    }
    case 0x50:
        object[1] = op & 0xF;
        break;
    case 0x60: result = move_script::gMoveScript.group6(object, script, result); break;
    case 0x80: result = move_script::gMoveScript.group8(object, script, result); break;
    case 0x90: result = move_script::gMoveScript.group9(object, script, result); break;
    case 0xC0: result = move_script::gMoveScript.group_c(object, script, result); break;
    case 0xD0: result = move_script::gMoveScript.group_d(object, script, result); break;
    case 0xE0: result = move_script::gMoveScript.group_e(object, script, result); break;
    case 0xF0: result = MoveScript_GroupF(object, script, result); break;
    case 0xA0:   // A0-AF that the flow pass stopped on: stay
        SetPos(object, pos - 1u);
        break;
    default:     // 0x70, and the high nibbles the flow pass never stops on
        break;
    }
    SetPos(object, Pos(object) + 1u);
    return result;
}

// original 0x576E00: the flow-control pass that runs before every step
// (PSX FUN_801A9F94). It loops until the byte at the position has a high
// nibble other than 0x00, 0xA0 and 0xB0, and returns `value` - replaced by
// Sprite_Current[8] after op 03 and by 0xFF after op 0D on the kind-2 path.
//
// As the original has it: byte 00 is not one of the cases and loops for ever
// on the same position (unreachable by the fuzz; by reading); the position is
// read again after every call that could move it, and every position store
// is 16 bits; the area number indexes Area_Descriptors unchecked.
extern "C" unsigned char __cdecl MoveScript_Flow(unsigned char* object, const unsigned char* script, unsigned char value) {
    unsigned char result = value;
    for (;;) {
        const std::uint16_t pos = Pos(object);
        const unsigned char* const p = script + pos;
        const unsigned char op = p[0];
        const unsigned group = op & 0xF0u;
        if (group >= 0x10) {
            if (group == 0xA0) {
                const auto ops = reinterpret_cast<const ::bof3::CodeSlot<CounterOp>*>(MoveScript_CounterOps);   // 32-bit code words
                const signed char skip = ops[op & 3](p);
                if (skip < 0) return result;
                SetPos(object, Pos(object) + static_cast<unsigned>(skip) + 1u);
            } else if (group == 0xB0) {
                move_script::gMoveScript.group_b(object, script);
                SetPos(object, Pos(object) + 1u);
            } else {
                return result;
            }
            continue;
        }

        switch (op) {
        case 0x01: {   // 01 hi lo: jump by hi:lo, or to label lo when hi's top two bits are 01
            const unsigned char hi = p[1];
            const unsigned target = static_cast<unsigned>(hi) << 8 | p[2];
            if ((hi & 0xC0) == 0x40) {
                SetPos(object, target);
                SetPos(object, move_script::gMoveScript.find_label(object, p[2], script));
            } else {
                SetPos(object, target + pos);
            }
            break;
        }
        case 0x02: {   // 02 d n: loop back by d, n times, counting in object +2
            const unsigned char counter = object[2];
            if (counter == 0) {
                object[2] = p[2];
                SetPos(object, pos + static_cast<unsigned>(static_cast<signed char>(p[1])));
            } else {
                object[2] = static_cast<unsigned char>(counter - 1);
                if (object[2] == 0) SetPos(object, pos + 3u);
                else SetPos(object, pos + static_cast<unsigned>(static_cast<signed char>(p[1])));
            }
            break;
        }
        case 0x03: {   // 03 n: the area's handler n
            const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
            const auto handlers = static_cast<std::uint32_t>(Long(descriptor + 0x3C));   // a table of 32-bit code words
            const auto* const table = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(handlers));
            reinterpret_cast<AreaHandler>(static_cast<std::uintptr_t>(Long(table + 4u * p[1])))();
            result = rv::Current<records::SpriteHead>()->direction;
            SetPos(object, Pos(object) + 2u);
            break;
        }
        case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: {
            // op a b l1h l1l l2h l2l: compare, then go to label l1l or l2l. The
            // even ops take a as a constant, the odd ones as a variable.
            const int want = (op & 1) ? move_script::gMoveScript.variable(p[1]) : p[1];
            const int have = move_script::gMoveScript.variable(script[Pos(object) + 2u]);
            bool first;
            if (op <= 0x05) first = have == want;
            else if (op <= 0x07) first = have < want;
            else first = (want & have) != 0;
            const unsigned at = Pos(object);
            const unsigned char hi = script[at + (first ? 3u : 5u)];
            const unsigned char lo = script[at + (first ? 4u : 6u)];
            SetPos(object, static_cast<unsigned>(hi) << 8 | lo);
            SetPos(object, move_script::gMoveScript.find_label(object, lo, script));
            break;
        }
        case 0x0A:
            SetPos(object, pos + 2u);
            break;
        case 0x0B:
            SetPos(object, 0);
            break;
        case 0x0C:
            if (p[1]) move_script::gMoveScript.trio_clear();
            else move_script::gMoveScript.trio_set();
            SetPos(object, Pos(object) + 2u);
            break;
        case 0x0D: {   // 0D x xh z zh: place at (x.xh, z.zh) and take the ground's elevation
            if (move_script::gMoveScript.kind() == 2) {
                PlaceAndGround(rv::Bytes(&Sprite_Kind2), script + Pos(object));
                rv::Store<unsigned>(Sprite_Kind2.height_whole, static_cast<unsigned>(move_script::gMoveScript.elevation(Sprite_Kind2.x, Sprite_Kind2.z)));
                result = 0xFF;
            } else {
                PlaceAndGround(Sprite_Current, script + Pos(object));
                wide_staging::Placed0D(object, script, Pos(object));   // DIV-0122: a site's start moved out (wide only)
                const long h = move_script::gMoveScript.elevation(rv::Current<records::SpriteHead>()->x, rv::Current<records::SpriteHead>()->z);
                rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(h));
            }
            SetPos(object, Pos(object) + 5u);
            break;
        }
        case 0x0E: case 0x0F: {   // op a key flags [start]: an animation, by number or by the field's state
            const unsigned char animation = op == 0x0E
                ? p[1] : MoveScript_AnimByState[p[1] * 11u + Field_State[0x89]];
            object[0] |= 0x40;
            rv::Current<records::SpriteHead>()->flags2 |= 8;
            if (script[Pos(object) + 3u] & 1) rv::Current<records::SpriteHead>()->flags |= 0x10;
            else rv::Current<records::SpriteHead>()->flags &= 0xEF;
            rv::Current<records::SpriteHead>()->flip = script[Pos(object) + 2u];
            const unsigned at = Pos(object);
            if (script[at + 3] & 2) {
                move_script::gMoveScript.set_animation_at(animation, script[at + 4]);
                SetPos(object, Pos(object) + 1u);
                SetPos(object, Pos(object) + 4u);
            } else {
                move_script::gMoveScript.set_animation(animation);
                SetPos(object, Pos(object) + 4u);
            }
            break;
        }
        default:
            break;   // 00: the same position again, for ever
        }
    }
}

// original 0x577280: the flow pass's 0xB0 group (PSX FUN_801AA6E4) - sound and
// music, rewritten by the PC port (docs/movement-script.md section 1): every
// case calls a different function from the PSX's, B8's is a bare ret, and BC /
// BD pass operands the callee never reads. Our BC and BD pass only the one it
// reads.
extern "C" void __cdecl MoveScript_GroupB(unsigned char* object, const unsigned char* script) {
    const unsigned char* const p = script + Pos(object);
    switch (p[0]) {
    case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        move_groups::g.sound(static_cast<unsigned short>(p[1] + 0x100u * (p[0] - 0xB0u)));
        SetPos(object, Pos(object) + 1u);
        break;
    case 0xB4: case 0xBE:
        move_groups::g.music_play(p[1], 8);
        SetPos(object, Pos(object) + 1u);
        break;
    case 0xB5:
        move_groups::g.fade_out_stop(8);
        SetPos(object, Pos(object) + 1u);
        break;
    case 0xB8:   // DIVERGENCE DIV-0170: the PlayStation's 0x8015D300(b1, b2, b3, b3) - the song restarted at b3
        if (sound_sp::g_b8) sound_sp::OpB8(p[1], p[2], p[3]);
        else move_groups::g.op_b8(p[2]);
        SetPos(object, Pos(object) + 3u);
        break;
    case 0xB9:
        move_groups::g.fade_out_stop(10);
        SetPos(object, Pos(object) + 2u);
        break;
    case 0xBA:
        move_groups::g.fade_in(p[4]);
        SetPos(object, Pos(object) + 4u);
        break;
    case 0xBB:
        move_groups::g.fade_out(p[4]);
        SetPos(object, Pos(object) + 4u);
        break;
    case 0xBC:   // DIVERGENCE DIV-0172: the PlayStation's 0x8015D7C8(b1:b2, b3, b4) - the level table's b3 / b4
        if (sound_sp::Routed()) sound_sp::PlayLevel(static_cast<unsigned short>(p[1] << 8 | p[2]), p[3], p[4]);
        else move_groups::g.play_by_id(static_cast<unsigned short>(p[1] << 8 | p[2]));
        SetPos(object, Pos(object) + 4u);
        break;
    case 0xBD:   // DIVERGENCE DIV-0172: the PlayStation's 0x8015D820(b1:b2, b3) - only near object b3
        if (sound_sp::Routed()) sound_sp::PlayNear(static_cast<unsigned short>(p[1] << 8 | p[2]), p[3]);
        else move_groups::g.play_by_id(static_cast<unsigned short>(p[1] << 8 | p[2]));
        SetPos(object, Pos(object) + 3u);
        break;
    default:   // B6, B7, BF: nothing
        break;
    }
}

// original 0x577420: the 0xE0 group (PSX FUN_801AA944).
extern "C" unsigned char __cdecl MoveScript_GroupE(unsigned char* object, const unsigned char* script, unsigned char value) {
    const std::uint16_t pos = Pos(object);
    const unsigned char* const p = script + pos;
    const unsigned char op = p[0];
    switch (op) {
    case 0xE1:   // E1 d t: face d, then turn toward t
        object[5] = rv::Current<records::SpriteHead>()->direction;
        move_groups::g.face(p[1]);
        SetWord(object + 8, script[Pos(object) + 2u]);
        move_groups::g.turn_target(object);
        SetPos(object, Pos(object) + 2u);
        object[0] = static_cast<unsigned char>((object[0] & 0xEF) | 0x20);
        return 0xFF;
    case 0xE0: case 0xE3: {   // E0 t: face the way ObjTrio 0 faces, reversed; E3 t: keep facing; then the talk: message or script t
        if (op == 0xE0) {
            object[0] |= 8;
            object[5] = rv::Current<records::SpriteHead>()->direction;
            value = static_cast<unsigned char>(ObjTrio[0].direction ^ 4);
            move_groups::g.face(value);
        } else {
            object[5] = rv::Current<records::SpriteHead>()->direction & 7;
        }
        SetPos(object, Pos(object) + 1u);
        SetWord(object + 8, script[Pos(object)]);
        move_groups::g.turn_target(object);
        const auto flags = static_cast<unsigned char>(object[0] & 0xEF);
        object[0] = flags;
        object[0] = static_cast<unsigned char>((rv::Current<records::SpriteHead>()->flags2 & 4) ? flags & 0xDF : flags | 0x20);
        return value;
    }
    case 0xE2:
        SetWord(object + 8, static_cast<unsigned>(p[1]) << 8 | p[2]);
        SetPos(object, pos + 2u);
        return value;
    case 0xE4:
        Scratch()[3] = 0xFF;
        rv::Current<records::SpriteHead>()->flags2 &= 0xD7;
        object[0] &= 0xBF;
        return value;
    case 0xE5:
        Field_ScriptFlags &= 0xFEFF;
        rv::Current<records::SpriteHead>()->state = Sprite_Current[3];
        return value;
    case 0xE6:   // the sprite's flags cleared: the object ends
        rv::Current<records::SpriteHead>()->flags = 0;
        return value;
    case 0xE7:
        SetPos(object, pos + 1u);
        move_groups::g.op_e7(script[Pos(object)]);
        object[7] = 0;
        return 0xFF;
    case 0xE8:
        SetPos(object, pos + 1u);
        object[4] = script[Pos(object)];
        return move_groups::g.kind() == 2 ? 0 : value;
    case 0xE9:
        if (move_groups::g.op_e9(object, static_cast<signed char>(p[1]), static_cast<signed char>(p[2]),
                    static_cast<unsigned short>(p[3] << 8 | p[4]), static_cast<unsigned short>(p[5] << 8 | p[6]), p[7], p[8])) {
            SetPos(object, Pos(object) - 1u);
            return move_groups::g.kind() == 2 ? rv::Current<records::SpriteHead>()->direction : 0xFF;
        }
        SetPos(object, Pos(object) + 8u);
        return value;
    case 0xEA: {   // EA r g b a: a tint; a type-0x0A sprite keeps r g b itself
        records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
        if (sprite->type == 0x0A) {
            sprite->shade[0] = p[1];
            rv::Current<records::SpriteHead>()->shade[1] = script[Pos(object) + 2u];
            rv::Current<records::SpriteHead>()->shade[2] = script[Pos(object) + 3u];
        } else {
            move_groups::g.tint(rv::Bytes(sprite), p[1], p[2], p[3], p[4]);
        }
        SetPos(object, Pos(object) + 4u);
        return value;
    }
    case 0xEB: rv::Current<records::SpriteHead>()->flags ^= 0x40; return value;
    case 0xEC: rv::Current<records::SpriteHead>()->flags2 ^= 4; return value;
    case 0xED: rv::Current<records::SpriteHead>()->flags2 ^= 8; return value;
    case 0xEE: rv::Current<records::SpriteHead>()->flags2 ^= 0x20; return value;
    case 0xEF:   // the end of the script: context bit 8
        object[0] |= 8;
        return value;
    default:
        return value;
    }
}

// original 0x577760: the 0xF0 group (PSX FUN_801AADE8), called by the step
// with the position on the op; the step adds one after it. Returns `value`,
// replaced by b1 for F2 / F3 / FA and b6 for F7; F9 returns 0xFF while it
// waits, else Sprite_Current[8] & 7.
//
// As the original has it: F3 divides by Sprite_Current[9] times object +7 on
// one path and by (0x80 / MoveScript_F3Divisor) times Sprite_Kind2 +0x87 on
// the other, unguarded - a zero there faults, as the original does (by
// reading; the fuzz keeps them non-zero); the kind-2 path shifts a 16-bit
// difference into the top half, so a difference past 0x7FFF wraps.
extern "C" unsigned char __cdecl MoveScript_GroupF(unsigned char* object, const unsigned char* script, unsigned char value) {
    const std::uint16_t pos = Pos(object);
    const unsigned char* const p = script + pos;
    if (p[0] < 0xF0) return value;   // unreachable from the step
    switch (p[0]) {
    case 0xF0:
        move_script::gMoveScript.flags_clear();
        return value;
    case 0xF1:
        move_script::gMoveScript.flags_set();
        return value;
    case 0xF2: case 0xFA: case 0xF3: {
        value = p[1];
        object[7] = p[2];
        const bool moves = p[2] != 0;
        if (p[0] == 0xF2) {
            if (moves) {
                if (move_script::gMoveScript.kind() == 2) move_script::gMoveScript.move_kind2(value);
                else move_script::gMoveScript.move(object, value);
            }
            SetPos(object, Pos(object) + 2u);
        } else if (p[0] == 0xFA) {
            if (moves) {
                if (move_script::gMoveScript.kind() == 2) {
                    move_script::gMoveScript.move_kind2(value);
                    MoveScript_FAWord = 0;
                    object[0] |= 0x40;
                } else {
                    move_script::gMoveScript.move(object, value);
                    object[0] |= 0x80;
                }
            }
            rv::Current<records::SpriteHead>()->step_y = 0;
            const unsigned at = Pos(object);
            rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(script[at + 3]) << 8 | script[at + 4]);
            SetPos(object, Pos(object) + 4u);
        } else if (moves) {   // F3 d m th tl: move toward height th.tl, over the move's frames
            if (move_script::gMoveScript.kind() != 2) {
                move_script::gMoveScript.move(object, value);
                const unsigned at = Pos(object);
                const auto target = static_cast<std::int16_t>(script[at + 3] << 8 | script[at + 4]);
                records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
                const std::int32_t rise = target - static_cast<std::int16_t>(sprite->height_whole);
                const std::int32_t frames = static_cast<std::int32_t>(sprite->step_frames) * object[7];
                sprite->step_y = rise / frames;
                object[0] |= 0x80;
                SetPos(object, Pos(object) + 4u);
            } else {
                unsigned char* const scratch = Scratch();
                SetWord(scratch, rv::Current<records::SpriteHead>()->height_whole);
                const unsigned at = Pos(object);
                SetWord(scratch + 2, static_cast<unsigned>(script[at + 3]) << 8 | script[at + 4]);
                move_script::gMoveScript.move_kind2(value);
                const std::int32_t frames = 0x80 / static_cast<std::int32_t>(MoveScript_F3Divisor) * Sprite_Kind2.move_steps;
                const std::int32_t rise = static_cast<std::int16_t>(Word(scratch + 2)) - static_cast<std::int16_t>(Word(scratch));
                rv::Store<std::int32_t>(Sprite_Kind2.step_y, static_cast<std::int32_t>(static_cast<std::uint32_t>(rise) << 16) / frames);
                object[0] |= 0x40;
                SetPos(object, Pos(object) + 4u);
            }
        } else {
            SetPos(object, Pos(object) + 4u);
        }
        return value;
    }
    case 0xF4: {   // F4 th tl n: rise to th.tl over n frames (0 counts as 1)
        object[0] |= 0x80;
        rv::Current<records::SpriteHead>()->step_frames = p[3];
        if (rv::Current<records::SpriteHead>()->step_frames == 0) rv::Current<records::SpriteHead>()->step_frames = 1;
        records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
        const unsigned at = Pos(object);
        const auto target = static_cast<std::int16_t>(script[at + 1] << 8 | script[at + 2]);
        const std::int32_t rise = target - static_cast<std::int16_t>(sprite->height_whole);
        sprite->step_y = rise / static_cast<std::int32_t>(sprite->step_frames);
        rv::Current<records::SpriteHead>()->step_x = 0;
        rv::Current<records::SpriteHead>()->step_z = 0;
        SetPos(object, Pos(object) + 3u);
        return value;
    }
    case 0xF5:
        object[0] &= 0x7F;
        return value;
    case 0xF6:
        MoveScript_Var7 = static_cast<signed char>(p[1]);
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xF7: {
        const long x = static_cast<long>((static_cast<unsigned>(p[1]) << 8 | p[2]) << 8);
        const long z = static_cast<long>((static_cast<unsigned>(p[3]) << 8 | p[4]) << 8);
        move_script::gMoveScript.op_f7(x, z, p[5], object);
        value = script[Pos(object) + 6u];
        if (move_script::gMoveScript.kind() == 1) object[7] = 1;
        SetPos(object, Pos(object) + 6u);
        return value;
    }
    case 0xF8:
        move_script::gMoveScript.attach(object, p[1], p[2], p[3], p[4]);
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xF9:
        if (move_script::gMoveScript.wait_test(p[1], p[2]) != 0) {
            SetPos(object, Pos(object) - 1u);
            return 0xFF;
        }
        SetPos(object, Pos(object) + 2u);
        return static_cast<unsigned char>(rv::Current<records::SpriteHead>()->direction & 7);
    case 0xFB: case 0xFC: {
        unsigned char* const sprite = Sprite_Current;
        const auto x = static_cast<short>(rv::As<records::SpriteHead>(sprite)->x_whole), z = static_cast<short>(rv::As<records::SpriteHead>(sprite)->z_whole);
        if ((p[0] == 0xFB ? move_script::gMoveScript.test_fb : move_script::gMoveScript.test_fc)(x, z)) move_script::gMoveScript.sound(0x103);
        return value;
    }
    case 0xFD:
        if (!move_script::gMoveScript.tick_once()) {
            rv::Current<records::SpriteHead>()->flags2 |= 0x20;
            SetPos(object, Pos(object) - 1u);
        }
        return value;
    case 0xFE:
        if (Field_Request != 3) {
            const std::uint16_t next = move_script::gMoveScript.peek();
            SetWord(Scratch(), next);
            if (rv::Current<records::SpriteHead>()->script_pos != next) {
                const unsigned at = Pos(object);
                if (next == script[at + 2]) move_script::gMoveScript.sound(static_cast<unsigned short>(script[at + 1] + 0x200u));
            }
        }
        SetPos(object, Pos(object) + 2u);
        return value;
    default:   // FF: the end - the script stays on it
        SetPos(object, pos - 1u);
        return value;
    }
}

// original 0x577BD0: the 0xD0 group (PSX FUN_801AB470).
//
// As the original has it: D4 divides by its count operand, which it has just
// tested non-zero; the party slot is clamped to 4 by D4 only when above it,
// by D6 only when negative - D6 can reach MoveScript_PartyRecords slots past
// 4 (by reading; the fuzz keeps Field_ActiveMember inside the party objects).
extern "C" unsigned char __cdecl MoveScript_GroupD(unsigned char* object, const unsigned char* script, unsigned char value) {
    const std::uint16_t pos = Pos(object);
    const unsigned char* const p = script + pos;
    switch (p[0]) {
    case 0xD0: {   // place Sprite_Kind2 at Field_Kind2X / Z and ground it
        const long z = Field_Kind2Z, x = Field_Kind2X;
        rv::Store<std::int32_t>(Sprite_Kind2.x, x);
        rv::Store<std::int32_t>(Sprite_Kind2.z, z);
        rv::Store<unsigned>(Sprite_Kind2.height_whole, static_cast<unsigned>(move_groups::g.elevation(x, z)));
        return value;
    }
    case 0xD1:   // D1 hi lo x s: sound hi:lo when the sprite's script reaches x
        if (Field_Request != 3) {
            const std::uint16_t next = move_groups::g.peek();
            SetWord(Scratch(), next);
            if (rv::Current<records::SpriteHead>()->script_pos != next) {
                const unsigned char* const q = script + Pos(object);
                // DIVERGENCE DIV-0172: the PlayStation's 0x8015D820(hi:lo, s) (GAME.EMI US 0x801AA568).
                if (next == q[3] && sound_sp::Routed()) sound_sp::PlayNear(static_cast<unsigned short>(q[1] << 8 | q[2]), q[4]);
                else if (next == q[3]) move_groups::g.play_by_id(static_cast<unsigned short>(q[1] << 8 | q[2]));
            }
        }
        SetPos(object, Pos(object) + 4u);
        return value;
    case 0xD2:
        rv::Current<records::SpriteHead>()->flags2 ^= 0x40;
        rv::Current<records::SpriteHead>()->scale_mode = (rv::Current<records::SpriteHead>()->flags2 & 0x40) ? 0 : 1;
        return value;
    case 0xD3:
        rv::Current<records::SpriteHead>()->flags2 ^= 0x80;
        return value;
    case 0xD4: {   // D4 x x y y z z n: the party slot moves by x y z over n frames, or the sprite's +0x64..+0x6C
        unsigned char slot = PartySlot();
        Scratch()[1] = slot;
        if (slot > 4) {
            slot = 4;
            Scratch()[1] = slot;
        }
        const unsigned char n = script[Pos(object) + 7u];
        if (n != 0) {
            unsigned char* const r = PartyRecord(slot);
            r[1] = n;
            r[2] = n;
            SetLong(r + 0xC, S16(script + Pos(object) + 5) / static_cast<signed char>(n));
            SetLong(r + 8, S16(script + Pos(object) + 3) / static_cast<signed char>(r[2]));
            SetLong(r + 4, S16(script + Pos(object) + 1) / static_cast<signed char>(r[2]));
        } else {
            rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->angle[2], S16(script + Pos(object) + 5));
            rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->angle[1], S16(script + Pos(object) + 3));
            rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->angle[0], S16(script + Pos(object) + 1));
        }
        object[0] |= 8;
        SetPos(object, Pos(object) + 7u);
        return value;
    }
    case 0xD5: {   // wait while the party slot's counts are running
        const unsigned char slot = PartySlot();
        Scratch()[1] = slot;
        if (slot >= 4) return value;
        const unsigned char* const r = PartyRecord(slot);
        if (static_cast<signed char>(r[2]) + static_cast<signed char>(r[1]) != 0) SetPos(object, Pos(object) - 1u);
        return value;
    }
    case 0xD6: {
        unsigned char slot = PartySlot();
        Scratch()[1] = slot;
        if (static_cast<signed char>(slot) < 0) {
            slot = 4;
            Scratch()[1] = slot;
        }
        PartyRecord(static_cast<signed char>(slot))[0] ^= 1;
        return value;
    }
    case 0xD7:
        object[6] = p[1];
        SetPos(object, pos + 1u);
        return value;
    case 0xD8:   // the end of the script: context bit 8
        Field_ScriptFlags &= 0xFFF7;
        object[0] |= 8;
        return value;
    case 0xD9:
        if (Field_Request == 2) SetPos(object, pos - 1u);
        return value;
    case 0xDA:
        if (Transition_Busy != 0) SetPos(object, pos - 1u);
        return value;
    case 0xDB:
        move_groups::g.op_db();
        rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(move_groups::g.elevation(rv::Current<records::SpriteHead>()->x, rv::Current<records::SpriteHead>()->z)));
        [[fallthrough]];
    case 0xDC:
        move_groups::g.test_fb(static_cast<short>(rv::Current<records::SpriteHead>()->x_whole), static_cast<short>(rv::Current<records::SpriteHead>()->z_whole));
        return value;
    case 0xDD:
        move_groups::g.test_fc(static_cast<short>(rv::Current<records::SpriteHead>()->x_whole), static_cast<short>(rv::Current<records::SpriteHead>()->z_whole));
        return value;
    case 0xDE: {   // DE n: the area's handler n, as flow op 03
        const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
        const auto handlers = static_cast<std::uint32_t>(Long(descriptor + 0x3C));   // a table of 32-bit code words
        reinterpret_cast<AreaHandler>(static_cast<std::uintptr_t>(Long(At(handlers + 4u * p[1]))))();
        const unsigned char result = rv::Current<records::SpriteHead>()->direction;
        SetPos(object, Pos(object) + 1u);
        return result;
    }
    default:   // DF
        if (Field_Request == 3) SetPos(object, pos - 1u);
        return value;
    }
}

// original 0x578010: the 0xC0 group (PSX FUN_801ABB00).
//
// As the original has it: C1 is 4 bytes, where MoveScript_OpLengths says 5
// (docs/movement-script.md section 4); a tint record's index is a byte and
// unchecked.
extern "C" unsigned char __cdecl MoveScript_GroupC(unsigned char* object, const unsigned char* script, unsigned char value) {
    const std::uint16_t pos = Pos(object);
    const unsigned char* const p = script + pos;
    const auto operand = [&](unsigned i) { return script[Pos(object) + i]; };
    switch (p[0]) {
    case 0xC0: {   // C0 r g b a: a tint, its record kept by the field (kind 1) or the party member
        const bool field = move_groups::g.kind() == 1;
        const unsigned char n = move_groups::g.tint(Sprite_Current, operand(1), operand(2), operand(3), operand(4));
        if (field) Field_State[0x149] = n;
        else ActiveMember()[0x9F] = n;
        SetPos(object, Pos(object) + 4u);
        return value;
    }
    case 0xC1: case 0xC2: {   // C1 / C2 r g b: brighten / darken that record
        const bool add = p[0] == 0xC1;
        const unsigned char* const owner = move_groups::g.kind() == 1 ? Field_State + 0x149 : ActiveMember() + 0x9F;
        for (unsigned i = 0; i < 3; ++i) {
            unsigned char* const c = TintRecord(*owner) + 2 + i;
            *c = static_cast<unsigned char>(add ? *c + operand(1 + i) : *c - operand(1 + i));
        }
        SetPos(object, Pos(object) + 3u);
        return value;
    }
    case 0xC3: case 0xC4: {
        rv::Current<records::SpriteHead>()->scale_mode = p[1];
        const std::int32_t a = Fixed88(script + Pos(object) + 2);
        (p[0] == 0xC3 ? AddLong : SubLong)(Sprite_Current + 0x40, a);
        const std::int32_t b = Fixed88(script + Pos(object) + 4);
        (p[0] == 0xC3 ? AddLong : SubLong)(Sprite_Current + 0x44, b);
        SetPos(object, Pos(object) + 5u);
        return value;
    }
    case 0xC5:
        if (p[1] & 0x80) rv::Current<records::SpriteHead>()->flags |= 0x20;
        else rv::Current<records::SpriteHead>()->flags &= 0xDF;
        rv::Current<records::SpriteHead>()->draw_mode = operand(1) & 3;
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xC6:
        rv::Current<records::SpriteHead>()->shade[0] = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->shade[0] + p[1]);
        rv::Current<records::SpriteHead>()->shade[1] = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->shade[1] + operand(2));
        rv::Current<records::SpriteHead>()->shade[2] = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->shade[2] + operand(3));
        SetPos(object, Pos(object) + 3u);
        return value;
    case 0xC7: {
        rv::Current<records::SpriteHead>()->scale_mode = 2;
        static const unsigned kFields[] = {0x40, 0x44, 0x34, 0x38, 0x3C};
        for (unsigned i = 0; i < 5; ++i) AddLong(Sprite_Current + kFields[i], Fixed88(script + Pos(object) + 1 + 2 * i));
        SetPos(object, Pos(object) + 10u);
        return value;
    }
    case 0xC8: {   // C8 d s: turn toward direction d, one step a frame, s & 0xF to context +1
        records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
        if (sprite->direction == p[1]) {
            SetPos(object, pos + 2u);
            return rv::Current<records::SpriteHead>()->direction;
        }
        const unsigned char s = p[2];
        return TurnStep(object, static_cast<unsigned char>((sprite->direction + ((s & 0x80) ? 1 : -1)) & 7), s & 0xF);
    }
    case 0xC9:
        rv::Current<records::SpriteHead>()->key_class = p[1];
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xCA:
        ActiveMember()[0xA0] = p[1];
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xCB:
        move_groups::g.animation_bank(static_cast<unsigned short>(p[1] << 8 | p[2]));
        move_groups::g.set_animation(operand(3));
        rv::Current<records::SpriteHead>()->flip = operand(4) & 1;
        SetPos(object, Pos(object) + 4u);
        return value;
    case 0xCC:
        if (!(object[0] & 0x20) && (object[0] & 0x10)) SetPos(object, pos - 1u);
        return value;
    case 0xCD:
        move_groups::g.tint_release(Sprite_Current);
        return value;
    case 0xCE: {   // turn toward ObjTrio 0's direction, reversed
        records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
        const auto target = static_cast<unsigned char>(ObjTrio[0].direction ^ 4);
        const auto now = static_cast<unsigned char>(sprite->direction & 7);
        if (now == target) return sprite->direction;
        const int step = static_cast<signed char>(target - now) <= 0 ? -1 : 1;
        return TurnStep(object, static_cast<unsigned char>((now + step) & 7), 2);
    }
    default:
        return value;
    }
}

// original 0x5786C0: the 0x80 group (PSX FUN_801AC458).
//
// As the original has it: 8A only advances - the PSX calls 0x80164A10 there,
// the port dropped it; 86 sets the position to 0xFFFF, so the step's + 1
// restarts the script.
extern "C" unsigned char __cdecl MoveScript_Group8(unsigned char* object, const unsigned char* script, unsigned char value) {
    const std::uint16_t pos = Pos(object);
    const unsigned char* const p = script + pos;
    switch (p[0]) {
    case 0x80: Field_ScriptFlags |= 0x100; return value;
    case 0x81: Field_ScriptFlags &= 0xFEFF; return value;
    case 0x82: object[0] |= 2; return value;
    case 0x83: object[0] |= 4; return value;
    case 0x84: {   // 84 h b: to the position of the object handle h names
        long buffer[4];
        const long* const at = move_groups::g.handle_position(buffer, p[1], p[2]);
        const long x = at[0], y = at[1], z = at[2];
        rv::Current<records::SpriteHead>()->x = x;
        rv::Current<records::SpriteHead>()->z = y;
        rv::Current<records::SpriteHead>()->height = z;
        SetPos(object, Pos(object) + 2u);
        return value;
    }
    case 0x85: {   // the kind-2 object toward ObjTrio 0, repeating until it is there
        const long x = Field_Kind2X, to_x = ObjTrio[0].x, to_z = ObjTrio[0].z, z = Field_Kind2Z;
        if (x == to_x && z == to_z) {
            if (Field_Kind2Hold == 0) {
                move_groups::g.set_elevation(move_groups::g.elevation(x, z));
                return value;
            }
            SetPos(object, pos - 1u);
            return value;
        }
        const auto away = [](long a, long b) {
            const std::uint32_t d = static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b);
            const std::uint32_t magnitude = static_cast<std::int32_t>(d) < 0 ? 0u - d : d;
            return static_cast<unsigned char>(static_cast<std::int32_t>(magnitude) >> 15);
        };
        unsigned char steps = away(x, to_x);
        const unsigned char steps_z = away(z, to_z);
        if (steps < steps_z) steps = steps_z;
        move_groups::g.elevation(x, z);
        const long nx = ObjTrio[0].x, nz = ObjTrio[0].z;
        Field_Kind2X = nx;
        Field_Kind2Z = nz;
        const long h = move_groups::g.elevation(nx, nz);
        MoveScript_F3Divisor = 0x20;
        if (steps == 0) {
            MoveScript_FAWord = 0;
        } else {
            MoveScript_FAWord = static_cast<unsigned short>((static_cast<std::int16_t>(h) - MapView_Elevation) / (steps * 4));
        }
        SetPos(object, Pos(object) - 1u);
        return value;
    }
    case 0x86: {   // 86 n: hand over to the area's +0x18 script n (kind 1), else n to context +3; restart
        if (move_groups::g.kind() == 1) {
            const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
            const auto scripts = static_cast<std::uint32_t>(Long(descriptor + 0x18));   // a table of 32-bit places
            const std::int32_t next = Long(At(scripts + 4u * script[Pos(object) + 1u]));
            SetLong(Field_State + 0x130, next);   // the word, into the 32-bit slot
        } else {
            object[3] = script[Pos(object) + 1u];
        }
        SetPos(object, 0xFFFF);
        return value;
    }
    case 0x87: move_groups::g.op_87(); return value;
    case 0x88: move_groups::g.op_88(); return value;
    case 0x89:
        move_groups::g.load_stream(static_cast<unsigned short>(p[1] << 8 | p[2]));
        SetPos(object, Pos(object) + 2u);
        return value;
    case 0x8A:
        SetPos(object, pos + 2u);
        return value;
    case 0x8B:
        if (move_groups::g.stream_done() == 0) SetPos(object, Pos(object) - 1u);
        return value;
    case 0x8C: {
        records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
        const auto arg = static_cast<signed char>(MoveScript_EffectArg[Character_IndexById[Field_State[0x89]]]);
        const unsigned char n = move_groups::g.effect_spawn_at(static_cast<unsigned char>(p[1] - 0x90), p[1] == 0x94 ? 10 : 0, arg,
                                                  sprite->x, sprite->z, sprite->height);
        Sprite_Current[0xB] = n;
        SetPos(object, Pos(object) + 1u);
        return value;
    }
    default:
        return value;
    }
}

// original 0x578A00: the 0x90 group (PSX FUN_801AC8B0) - effects.
extern "C" unsigned char __cdecl MoveScript_Group9(unsigned char* object, const unsigned char* script, unsigned char value) {
    const std::uint16_t pos = Pos(object);
    const unsigned char* const p = script + pos;
    const unsigned char op = p[0];
    if (op >= 0x90 && op <= 0x95) {
        records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
        const unsigned char n = move_groups::g.effect_spawn(static_cast<unsigned char>(op - 0x90), static_cast<signed char>(p[1]),
                                               static_cast<signed char>(p[2]), static_cast<short>(static_cast<std::uint16_t>(sprite->screen_x)),
                                               static_cast<short>(static_cast<std::uint16_t>(sprite->screen_y)));
        Sprite_Current[0xB] = n;
        SetPos(object, Pos(object) + 2u);
    } else if (op >= 0x98 && op <= 0x9D) {
        records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
        const unsigned char n = move_groups::g.effect_spawn_at(static_cast<unsigned char>(op - 0x98), static_cast<signed char>(p[1]),
                                                  static_cast<signed char>(p[2]), sprite->x, sprite->z,
                                                  sprite->height);
        Sprite_Current[0xB] = n;
        SetPos(object, Pos(object) + 2u);
    } else if (op == 0x9F) {   // wait while the effect lives
        if (Effect_Objects[Sprite_Current[0xB]].flags & 1) SetPos(object, pos - 1u);
    }
    return value;
}

// original 0x578B00: the 0x60 group (PSX FUN_801ACA24) - an animation from the
// sprite's direction. Returns Sprite_Current[8], not `value`.
extern "C" unsigned char __cdecl MoveScript_Group6(unsigned char* object, const unsigned char* script, unsigned char value) {
    (void)value;
    records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
    const unsigned char d = sprite->direction;
    unsigned char animation;
    switch (script[Pos(object)]) {
    case 0x61: animation = static_cast<unsigned char>((d & 7) + 0x14); break;
    case 0x62: animation = static_cast<unsigned char>((d & 7) + 0x1C); break;
    case 0x63: animation = static_cast<unsigned char>((d & 7) + 0x24); break;
    case 0x64: animation = static_cast<unsigned char>((d & 7) + 0x2C); break;
    case 0x65: case 0x66:
        animation = static_cast<unsigned char>(((d >> 1) & 3) + (script[Pos(object)] == 0x65 ? 0x34 : 0x10));
        sprite->direction = static_cast<unsigned char>((d & 6) + 1);
        break;
    default: animation = d & 7; break;
    }
    object[0] |= 0x40;
    rv::Current<records::SpriteHead>()->flags2 |= 8;
    move_groups::g.set_animation(animation);
    return rv::Current<records::SpriteHead>()->direction;
}

// original 0x578C10 (PSX 0x801ACBA8): a step's move of Sprite_Current toward `direction`
// (PSX FUN_801ACBA8), the twin of MoveCmd_MoveKind2 for every other object:
// speed = Field_MoveSpeeds[object +4]; the frame count +9 = 0x20 for
// directions 2 and 6, else 0x10, divided by the speed; the velocity +0xC /
// +0x10 = the direction's MoveCmd_F9Steps pair times the speed; then, unless
// bit 7 of object +0, the ground where the move lands - AreaMap_Slope there,
// or AreaMap_Elevation when the slope says the ground is sloped (SharedScratch's
// byte) - and the rise to it spread over +9 frames into +0x14. Flag 0x10.
//
// As the original has it: the speed byte indexes the table unmasked; the
// division of +9 is signed and a speed of 0 faults (Field_MoveSpeeds' entry 0
// - ours aborts, by name, where the original raises the divide fault); a
// speed above 0x10 (0x20 on the diagonals) leaves +9 at 0, which only the
// rise divides by - aborted likewise; the products and the landing wrap in 32
// bits; the flat answer is AreaMap_Slope's own return, the sloped one
// AreaMap_Elevation's, each taken as its low 16 bits sign-extended, less the
// height +0x3E as a short; +9 and +0x3E are read again through Sprite_Current
// after the calls; object +0 is read after the velocity is stored. The
// original pushes its whole direction dword to AreaMap_Slope, which reads only
// its low byte (area_slope.cpp); ours passes the byte. Nothing is returned
// (eax holds Sprite_Current; every caller discards it).
extern "C" void __cdecl MoveCmd_Move(unsigned char* object, unsigned char direction) {
    const unsigned char speed = Field_MoveSpeeds[object[4]];
    rv::Current<records::SpriteHead>()->step_frames = (direction == 2 || direction == 6) ? 0x20 : 0x10;
    if (speed == 0) bof3::Fatal("MoveCmd_Move: speed index %u is a speed of 0, a divide fault in the original", object[4]);
    records::SpriteHead* sprite = rv::Current<records::SpriteHead>();
    sprite->step_frames = static_cast<unsigned char>(static_cast<std::int32_t>(sprite->step_frames) / static_cast<std::int32_t>(speed));
    const long* const unit = MoveCmd_F9Steps + direction * 2u;
    sprite = rv::Current<records::SpriteHead>();
    sprite->step_x = Mul(unit[0], speed);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_z, Mul(unit[1], speed));
    sprite = rv::Current<records::SpriteHead>();
    const std::int32_t frames = sprite->step_frames;
    const std::int32_t x = Add(Mul(sprite->step_x, frames), sprite->x);
    const std::int32_t z = Add(Mul(sprite->step_z, frames), sprite->z);
    if (!(object[0] & 0x80)) {
        long ground = move_cmds::g.slope(x, z, direction);
        if (Scratch()[0] != 0) ground = move_cmds::g.elevation(x, z);
        sprite = rv::Current<records::SpriteHead>();
        const std::int32_t rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<std::uint32_t>(ground))) -
                                  static_cast<short>(Word(rv::Bytes(sprite) + 0x3E));
        const std::int32_t count = sprite->step_frames;
        if (count == 0) bof3::Fatal("MoveCmd_Move: a frame count of 0 (speed %u), a divide fault in the original", speed);
        sprite->step_y = rise / count;
        sprite = rv::Current<records::SpriteHead>();
    }
    sprite->flags |= 0x10;
}

// original 0x578D10: op F7's timed move to (x, z) on the ground (PSX
// FUN_801ACD68), when the context's +4 is set: over n = frames (0 taken as
// 1) frames, the velocity to +0xC / +0x10 / +0x14, n to +9, n + 1 to +0xA,
// and flag 0x10.
// As the original has it: the differences are from the position on entry and
// wrap; the rise is 16 bits; +0xA wraps for n = 0xFF; the ground is asked
// for even when nothing moves. The original returns Sprite_Current[8] as on
// entry in al; its one caller, MoveScript_GroupF, clears eax at once (0x577AEA).
extern "C" void __cdecl MoveCmd_OpF7(long x, long z, unsigned char frames, unsigned char* context) {
    const records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
    const std::int32_t dz = Sub(z, sprite->z);
    const std::int32_t dx = Sub(x, sprite->x);
    const long ground = move_cmds::g.elevation(x, z);
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<std::uint32_t>(ground) - rv::Current<records::SpriteHead>()->height_whole));
    if (context[4] == 0) return;
    const unsigned char n = frames ? frames : 1;
    rv::Current<records::SpriteHead>()->step_frames = n;
    Sprite_Current[0xA] = static_cast<unsigned char>(n + 1);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_x, dx / n);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_z, dz / n);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_y, rise / n);
    rv::Current<records::SpriteHead>()->flags |= 0x10;
}

// original 0x578DC0: where the object a handle names stands, plus attachment
// offset `offset` (PSX FUN_801ACF30): out = its +0x34, +0x38 and +0x3E (s16)
// plus MoveCmd_AttachOffset's three - computed with Sprite_Current pointed at
// that object, then put back. Returns out.
// The handle: without bit 7, Sprite_ObjectsExtra[handle] - all seven bits,
// where Sprite_ObjectByHandle takes (handle & 0x3F) + 0x1E; with it, the
// Sprite_Objects entry whose number is the COUNT of type-0x0A objects met
// before the (handle & 0x3F)-th one, which is handle & 0x3F when there is
// one, and the number of type-0x0A objects among the 30 when there is not -
// the PSX's quirk too.
// As the original has it: Sprite_Current is kept in the dword kSavedSprite
// and read back from there; out[3] is the fourth dword of the original's own
// 16-byte buffer, which MoveCmd_AttachOffset never writes - an uninitialised
// stack dword there (and on the PSX); 0 here. Neither caller reads it
// (MoveScript_Group8's op 84 copies three; MoveCmd_AttachMove's copy is dead).
extern "C" long* __cdecl MoveCmd_HandlePosition(long* out, unsigned char handle, unsigned char offset) {
    std::memcpy(At(::bof3::Img(MapView_CornerPtr_addr)), &Sprite_Current, 4);
    unsigned char* object;
    if (handle & 0x80) {
        unsigned char count = 0;
        for (unsigned char n = 0; n < 0x1E; ++n) {
            if (Sprite_Objects[n].type != 0x0A) continue;
            if (count == (handle & 0x3F)) break;
            ++count;
        }
        object = rv::Bytes(Sprite_Objects) + count * 0xA4u;
    } else {
        object = rv::Bytes(Sprite_ObjectsExtra) + handle * 0xA4u;
    }
    Sprite_Current = object;
    long buffer[4] = {};
    move_cmds::g.attach_offset(buffer, offset);
    const records::SpriteHead* const at = rv::Current<records::SpriteHead>();
    const std::int32_t x = Add(buffer[0], at->x);
    const std::int32_t z = Add(buffer[1], at->z);
    const std::int32_t y = Add(buffer[2], static_cast<short>(at->height_whole));
    std::memcpy(&Sprite_Current, At(::bof3::Img(MapView_CornerPtr_addr)), 4);
    out[0] = x;
    out[1] = z;
    out[2] = y;
    out[3] = buffer[3];
    return out;
}

// original 0x578EB0: an attachment's offset (PSX FUN_801AD1DC) - vector
// `index` of the area descriptor's +0x0C table, turned by Sprite_Current's
// angles through the GTE; out = (x << 9, y << 9, -z * 2).
extern "C" void __cdecl MoveCmd_AttachOffset(long* out, unsigned char index) {
    field_objects::g.push_matrix();
    const unsigned char* const sprite = Sprite_Current;
    short vector[4];
    vector[0] = static_cast<short>(Word(sprite + 0x64));
    vector[1] = static_cast<short>(Word(sprite + 0x68));
    vector[2] = static_cast<short>(Word(sprite + 0x6C));
    unsigned long matrix[8];
    field_objects::g.rot_matrix(vector, reinterpret_cast<short*>(matrix));
    const unsigned long zero[3] = {0, 0, 0};
    field_objects::g.trans_matrix(matrix, zero);
    field_objects::g.set_rot(matrix);
    field_objects::g.set_trans(matrix);
    const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
    const short* const vectors = reinterpret_cast<const short*>(At(static_cast<std::uint32_t>(Long(descriptor + 0xC))));   // a 32-bit word
    vector[0] = vectors[index * 3u];
    vector[1] = vectors[index * 3u + 1];
    vector[2] = vectors[index * 3u + 2];
    long turned[3];
    field_objects::g.rot_trans(vector, turned);
    out[0] = static_cast<long>(static_cast<std::uint32_t>(turned[0]) << 9);
    out[1] = static_cast<long>(static_cast<std::uint32_t>(turned[1]) << 9);
    out[2] = static_cast<long>((0u - static_cast<std::uint32_t>(turned[2])) << 1);
    field_objects::g.pop_matrix();
}

// original 0x578FA0 (PSX 0x801AD33C): op F9 a b (MoveScript_GroupF 0x57798A repeats the op
// while eax is not 0). The step per frame is 0x6696DC's (x, z) pair by the
// facing Sprite_Current +8 times the speed Field_MoveSpeeds[Field_ActiveMember
// +0x84]; the height +0x3E from AreaMap_Elevation. With the acceleration pair
// +0x18 / +0x1C both 0 (the op's first frame): the target (+0x8C / +0x90 of
// Field_ActiveMember) is Field_DirectionSteps' pair * 2 * b past the position,
// and toward a: the velocity +0xC / +0x10 from 0 with the step >> b as the
// acceleration; else from the step with minus that; +0x14 0. Every frame the
// velocity is added to the position and the acceleration to the velocity; a
// party object (+6 0x0A) moves its member (Party_MoveMember) when x or z
// crosses a half unit and runs its party record's tilt counts (+1 / +2) into
// +0x64..+0x6C. Reached (both coordinates at or past the target along the
// step's sign): the target taken, the velocity the step, the acceleration 0,
// eax 0; else eax 1.
// As the original has it: the speed is a whole byte into Field_MoveSpeeds'
// six; the party slot is (Field_ActiveMember - Sprite_ObjectsExtra) / 0xA4 as
// a signed byte into the party records, unchecked.
// Sprite_Current (Sc()) is any sprite head - a party member's too - so it
// reads as SpriteHead; Field_ActiveMember (Fam()) is a Sprite_Objects /
// Sprite_ObjectsExtra record (symbols.toml), so its +0x84 / +0x8C / +0x90
// stay SpriteObject's (C2 audit frs F3).
extern "C" int __cdecl MoveCmd_OpF9(unsigned char toward, unsigned char shift) {
    const unsigned char speed = Field_MoveSpeeds[rv::As<records::SpriteObject>(Fam())->speed];
    records::SpriteHead* sc = rv::As<records::SpriteHead>(Sc());
    const unsigned dir = rv::Bytes(sc)[8] & 7u;
    const auto vx = static_cast<std::int32_t>(static_cast<U>(Long(AtFieldO(::bof3::Img(MoveCmd_F9Steps_addr) + 8 * dir))) * speed);
    const auto vz = static_cast<std::int32_t>(static_cast<U>(Long(AtFieldO(::bof3::Img(MoveCmd_F9Steps_addr) + 8 * dir + 4))) * speed);
    const long ground = SH_CALL(AreaMap_Elevation)(sc->x, sc->z);
    rv::Store<unsigned>(rv::As<records::SpriteHead>(Sc())->height_whole, static_cast<U>(ground));
    sc = rv::As<records::SpriteHead>(Sc());
    const unsigned s = shift & 31u;
    if (static_cast<U>(sc->work_1c) + static_cast<U>(sc->work_18) == 0) {
        const unsigned d = rv::Bytes(sc)[8] & 7u;
        const std::int32_t x0 = sc->x;
        rv::As<records::SpriteObject>(Fam())->target_x = static_cast<std::int32_t>((static_cast<U>(Field_DirectionSteps[2 * d]) << 1) * shift + static_cast<U>(x0));
        const unsigned d2 = Sc()[8] & 7u;
        const std::int32_t z0 = rv::As<records::SpriteHead>(Sc())->z;
        rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Fam())->target_z, static_cast<std::int32_t>((static_cast<U>(Field_DirectionSteps[2 * d2 + 1]) << 1) * shift + static_cast<U>(z0)));
        if (toward) {
            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->work_18, vx >> s);
            rv::As<records::SpriteHead>(Sc())->work_1c = vz >> s;
            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->step_x, 0);
            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->step_z, 0);
        } else {
            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->work_18, static_cast<std::int32_t>(0u - static_cast<U>(vx >> s)));
            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->work_1c, static_cast<std::int32_t>(0u - static_cast<U>(vz >> s)));
            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->step_x, vx);
            rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->step_z, vz);
        }
        rv::As<records::SpriteHead>(Sc())->step_y = 0;
    }
    const auto add = [](unsigned char* to, const unsigned char* by) {
        SetLong(to, static_cast<std::int32_t>(static_cast<U>(Long(to)) + static_cast<U>(Long(by))));
    };
    add(Sc() + 0x34, Sc() + 0xC);
    add(Sc() + 0x38, Sc() + 0x10);
    add(Sc() + 0xC, Sc() + 0x18);
    add(Sc() + 0x10, Sc() + 0x1C);
    sc = rv::As<records::SpriteHead>(Sc());
    records::SpriteObject* fam = rv::As<records::SpriteObject>(Fam());
    if (sc->type == 0x0A) {
        const std::int32_t slot = static_cast<std::int32_t>(static_cast<U>(Key(rv::Bytes(fam))) - static_cast<U>(Key(rv::Bytes(Sprite_ObjectsExtra)))) / 0xA4;
        const auto crossed = [](const unsigned char* pos, const unsigned char* vel) {
            const U p = static_cast<U>(Long(pos));
            return (((static_cast<U>(Long(vel)) + p) ^ p) & 0x8000u) != 0;
        };
        if (crossed(rv::Bytes(sc) + 0x34, rv::Bytes(sc) + 0xC) || crossed(rv::Bytes(sc) + 0x38, rv::Bytes(sc) + 0x10)) {
            SH_CALL(Party_MoveMember)(rv::Bytes(fam), static_cast<unsigned char>(sc->direction & 7));
            fam = rv::As<records::SpriteObject>(Fam());
            sc = rv::As<records::SpriteHead>(Sc());
        }
        unsigned char* const r = MoveScript_PartyRecords + Sb(static_cast<U>(slot)) * 16;
        if (r[1] != 0) {
            r[1] = static_cast<unsigned char>(r[1] - 1);
            add(rv::Bytes(sc) + 0x64, r + 4);
            add(Sc() + 0x68, r + 8);
            fam = rv::As<records::SpriteObject>(Fam());
            sc = rv::As<records::SpriteHead>(Sc());
        }
        if (r[2] != 0) {
            r[2] = static_cast<unsigned char>(r[2] - 1);
            add(rv::Bytes(sc) + 0x6C, r + 0xC);
            fam = rv::As<records::SpriteObject>(Fam());
            sc = rv::As<records::SpriteHead>(Sc());
        }
    }
    const std::int32_t tx = fam->target_x;
    const std::int32_t x = sc->x;
    const bool at_x = vx < 0 ? !(x > tx) : !(x < tx);
    const std::int32_t z = sc->z;
    const bool at_z = vz < 0 ? !(z > fam->target_z) : !(z < fam->target_z);
    if (!(at_x && at_z)) return 1;
    sc->x = tx;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->z, rv::As<records::SpriteObject>(Fam())->target_z);
    rv::As<records::SpriteHead>(Sc())->step_x = vx;
    rv::As<records::SpriteHead>(Sc())->step_z = vz;
    rv::As<records::SpriteHead>(Sc())->work_1c = 0;
    rv::As<records::SpriteHead>(Sc())->work_18 = 0;
    return 0;
}

// original 0x5792A0: op F8 m h s t, attach (PSX FUN_801AD768;
// docs/movement-script.md section 2). For the object kind 1 (the ObjTrio
// three): detach when +2 is 1; mode 7 sets +2 to 1 and the handle and s as
// dwords at +0x18 / +0x1C, then with the context's +4 set the move there over
// t frames and context +7 = 1; any other mode clears +2 and does not advance.
// Otherwise: detach when +1 is 7; mode to +1; for 7 the handle and s, and with
// +4 the move. An attachment advances the position by 3.
// As the original has it: +1 is read back after the store.
extern "C" void __cdecl MoveCmd_Attach(unsigned char* context, unsigned char mode, unsigned char handle, unsigned char s,
                                       unsigned char t) {
    if (move_cmds::g.object_kind() == 1) {
        if (Sprite_Current[2] == 1) move_cmds::g.detach();
        if (mode != 7) {
            Sprite_Current[2] = 0;
            return;
        }
        Sprite_Current[2] = 1;
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_18, handle);
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_1c, s);
        if (context[4] != 0) {
            move_cmds::g.attach_move(t);
            context[7] = 1;
        }
        SetPos(context, Pos(context) + 3u);
        return;
    }
    if (rv::Current<records::SpriteHead>()->state == 7) move_cmds::g.detach();
    rv::Current<records::SpriteHead>()->state = mode;
    if (rv::Current<records::SpriteHead>()->state != 7) return;
    rv::Current<records::SpriteHead>()->work_18 = handle;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_1c, s);
    if (context[4] != 0) move_cmds::g.attach_move(t);
    SetPos(context, Pos(context) + 3u);
}

void FieldMoveScript_Inject() {
    BOF3_INJECT(MoveScript_Step);
    BOF3_INJECT(MoveScript_Flow);
    BOF3_INJECT(MoveScript_GroupB);
    BOF3_INJECT(MoveScript_GroupE);
    BOF3_INJECT(MoveScript_GroupF);
    BOF3_INJECT(MoveScript_GroupD);
    BOF3_INJECT(MoveScript_GroupC);
    BOF3_INJECT(MoveScript_Group8);
    BOF3_INJECT(MoveScript_Group9);
    BOF3_INJECT(MoveScript_Group6);
    BOF3_INJECT(MoveCmd_Move);
    BOF3_INJECT(MoveCmd_OpF7);
    BOF3_INJECT(MoveCmd_HandlePosition);
    BOF3_INJECT(MoveCmd_AttachOffset);
    BOF3_INJECT(MoveCmd_OpF9);
    BOF3_INJECT(MoveCmd_Attach);
}
