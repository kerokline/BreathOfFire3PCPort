// The movement script's group handlers: originals 0x578B00 (0x60), 0x5786C0
// (0x80), 0x578A00 (0x90), 0x578010 (0xC0), 0x577BD0 (0xD0), 0x577420 (0xE0)
// and the flow pass's 0x577280 (0xB0). docs/movement-script.md section 1.
#include "game/move_groups.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

using AreaHandler = void (__cdecl*)();

// Where a generated data name lives (its name is a macro, so bof3::addr:: cannot be used with it).
std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// Every callee, through pointers so that the start-up fuzz can stand recording
// functions in for them, for the original's copies and for ours alike. The
// area's handler table (op DE) and its +0x18 script array (op 86) are read
// from memory, as the original reads them.
struct Callees {
    int (__cdecl* kind)();
    void (__cdecl* face)(unsigned char);
    unsigned char (__cdecl* turn_target)(unsigned char*);   // its result unread by group E
    void (__cdecl* op_e7)(unsigned char);
    unsigned char (__cdecl* op_e9)(unsigned char*, signed char, signed char, unsigned short, unsigned short, unsigned char, unsigned char);
    unsigned char (__cdecl* tint)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char);
    void (__cdecl* tint_release)(unsigned char*);
    void (__cdecl* set_animation)(unsigned char);
    unsigned char (__cdecl* animation_bank)(unsigned short);
    long (__cdecl* elevation)(long, long);
    void (__cdecl* set_elevation)(int);
    unsigned short (__cdecl* peek)();
    void (__cdecl* play_by_id)(unsigned short);
    void (__cdecl* sound)(unsigned short);
    void (__cdecl* music_play)(unsigned int, int);
    void (__cdecl* fade_out_stop)(int);
    void (__cdecl* fade_in)(int);
    void (__cdecl* fade_out)(int);
    void (__cdecl* op_b8)(unsigned char);
    void (__cdecl* op_db)();
    unsigned char (__cdecl* test_fb)(short, short);
    unsigned char (__cdecl* test_fc)(short, short);
    unsigned char (__cdecl* effect_spawn)(unsigned char, signed char, signed char, short, short);
    unsigned char (__cdecl* effect_spawn_at)(unsigned char, signed char, signed char, long, long, long);
    long* (__cdecl* handle_position)(long*, unsigned char, unsigned char);
    void (__cdecl* op_87)();
    void (__cdecl* op_88)();
    void (__cdecl* load_stream)(unsigned);   // Sound_LoadStream reads the whole dword (save_menu.cpp)
    int (__cdecl* stream_done)();
};
const Callees kOriginals = {
    MoveScript_ObjectKind, Sprite_FaceDirection, MoveScript_SetTurnTarget, MoveCmd_OpE7, MoveCmd_OpE9,
    Sprite_SetTint, Sprite_ReleaseTint, Sprite_SetAnimation, Sprite_SetAnimationBank, AreaMap_Elevation,
    MapView_SetElevation, Sprite_ScriptPeek, Sound_PlayById, Sound_PlayEffect, Music_Play, Music_FadeOutStop,
    Music_FadeIn, Music_FadeOut, Port_DroppedCall, MoveCmd_OpDB, MoveCmd_TestFB, MoveCmd_TestFC, Effect_Spawn,
    Effect_SpawnAt, MoveCmd_HandlePosition, MoveCmd_Op87, MoveCmd_Op88, Sound_LoadStream, Sound_StreamDone,
};
Callees g = kOriginals;

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
    const auto distance = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(Field_ActiveMember) -
                                                    Address(Sprite_ObjectsExtra));
    return static_cast<unsigned char>(distance / 0xA4);
}
unsigned char* PartyRecord(int slot) { return MoveScript_PartyRecords + slot * 16; }
unsigned char* TintRecord(unsigned char n) { return MoveScript_TintRecords + n * 12u; }
unsigned char* ActiveMember() { return Field_ActiveMember; }

// Ops C8 and CE: the sprite's direction one step toward a target, then its
// facing (kind 1: the animation itself), and the op repeats.
unsigned char TurnStep(unsigned char* object, unsigned char direction, unsigned char mode) {
    Sprite_Current[8] = direction;
    if (g.kind() == 1) g.set_animation(Sprite_Current[8]);
    else g.face(Sprite_Current[8]);
    object[1] = mode;
    SetPos(object, Pos(object) - 1u);
    return Sprite_Current[8];
}

}  // namespace

// original 0x578A00: the 0x90 group (PSX FUN_801AC8B0) - effects.
extern "C" unsigned char __cdecl MoveScript_Group9(unsigned char* object, const unsigned char* script, unsigned char value) {
    const std::uint16_t pos = Pos(object);
    const unsigned char* const p = script + pos;
    const unsigned char op = p[0];
    if (op >= 0x90 && op <= 0x95) {
        unsigned char* const sprite = Sprite_Current;
        const unsigned char n = g.effect_spawn(static_cast<unsigned char>(op - 0x90), static_cast<signed char>(p[1]),
                                               static_cast<signed char>(p[2]), static_cast<short>(Word(sprite + 0x2E)),
                                               static_cast<short>(Word(sprite + 0x30)));
        Sprite_Current[0xB] = n;
        SetPos(object, Pos(object) + 2u);
    } else if (op >= 0x98 && op <= 0x9D) {
        unsigned char* const sprite = Sprite_Current;
        const unsigned char n = g.effect_spawn_at(static_cast<unsigned char>(op - 0x98), static_cast<signed char>(p[1]),
                                                  static_cast<signed char>(p[2]), Long(sprite + 0x34), Long(sprite + 0x38),
                                                  Long(sprite + 0x3C));
        Sprite_Current[0xB] = n;
        SetPos(object, Pos(object) + 2u);
    } else if (op == 0x9F) {   // wait while the effect lives
        if (Effect_Objects[Sprite_Current[0xB] * 0x80u] & 1) SetPos(object, pos - 1u);
    }
    return value;
}

// original 0x578B00: the 0x60 group (PSX FUN_801ACA24) - an animation from the
// sprite's direction. Returns Sprite_Current[8], not `value`.
extern "C" unsigned char __cdecl MoveScript_Group6(unsigned char* object, const unsigned char* script, unsigned char value) {
    (void)value;
    unsigned char* const sprite = Sprite_Current;
    const unsigned char d = sprite[8];
    unsigned char animation;
    switch (script[Pos(object)]) {
    case 0x61: animation = static_cast<unsigned char>((d & 7) + 0x14); break;
    case 0x62: animation = static_cast<unsigned char>((d & 7) + 0x1C); break;
    case 0x63: animation = static_cast<unsigned char>((d & 7) + 0x24); break;
    case 0x64: animation = static_cast<unsigned char>((d & 7) + 0x2C); break;
    case 0x65: case 0x66:
        animation = static_cast<unsigned char>(((d >> 1) & 3) + (script[Pos(object)] == 0x65 ? 0x34 : 0x10));
        sprite[8] = static_cast<unsigned char>((d & 6) + 1);
        break;
    default: animation = d & 7; break;
    }
    object[0] |= 0x40;
    Sprite_Current[7] |= 8;
    g.set_animation(animation);
    return Sprite_Current[8];
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
        g.sound(static_cast<unsigned short>(p[1] + 0x100u * (p[0] - 0xB0u)));
        SetPos(object, Pos(object) + 1u);
        break;
    case 0xB4: case 0xBE:
        g.music_play(p[1], 8);
        SetPos(object, Pos(object) + 1u);
        break;
    case 0xB5:
        g.fade_out_stop(8);
        SetPos(object, Pos(object) + 1u);
        break;
    case 0xB8:
        g.op_b8(p[2]);
        SetPos(object, Pos(object) + 3u);
        break;
    case 0xB9:
        g.fade_out_stop(10);
        SetPos(object, Pos(object) + 2u);
        break;
    case 0xBA:
        g.fade_in(p[4]);
        SetPos(object, Pos(object) + 4u);
        break;
    case 0xBB:
        g.fade_out(p[4]);
        SetPos(object, Pos(object) + 4u);
        break;
    case 0xBC:
        g.play_by_id(static_cast<unsigned short>(p[1] << 8 | p[2]));
        SetPos(object, Pos(object) + 4u);
        break;
    case 0xBD:
        g.play_by_id(static_cast<unsigned short>(p[1] << 8 | p[2]));
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
        object[5] = Sprite_Current[8];
        g.face(p[1]);
        SetWord(object + 8, script[Pos(object) + 2u]);
        g.turn_target(object);
        SetPos(object, Pos(object) + 2u);
        object[0] = static_cast<unsigned char>((object[0] & 0xEF) | 0x20);
        return 0xFF;
    case 0xE0: case 0xE3: {   // E0 t: face the way ObjTrio 0 faces, reversed; E3 t: keep facing; then turn toward t
        if (op == 0xE0) {
            object[0] |= 8;
            object[5] = Sprite_Current[8];
            value = static_cast<unsigned char>(ObjTrio[8] ^ 4);
            g.face(value);
        } else {
            object[5] = Sprite_Current[8] & 7;
        }
        SetPos(object, Pos(object) + 1u);
        SetWord(object + 8, script[Pos(object)]);
        g.turn_target(object);
        const auto flags = static_cast<unsigned char>(object[0] & 0xEF);
        object[0] = flags;
        object[0] = static_cast<unsigned char>((Sprite_Current[7] & 4) ? flags & 0xDF : flags | 0x20);
        return value;
    }
    case 0xE2:
        SetWord(object + 8, static_cast<unsigned>(p[1]) << 8 | p[2]);
        SetPos(object, pos + 2u);
        return value;
    case 0xE4:
        Scratch()[3] = 0xFF;
        Sprite_Current[7] &= 0xD7;
        object[0] &= 0xBF;
        return value;
    case 0xE5:
        Field_ScriptFlags &= 0xFEFF;
        Sprite_Current[1] = Sprite_Current[3];
        return value;
    case 0xE6:   // the sprite's flags cleared: the object ends
        Sprite_Current[0] = 0;
        return value;
    case 0xE7:
        SetPos(object, pos + 1u);
        g.op_e7(script[Pos(object)]);
        object[7] = 0;
        return 0xFF;
    case 0xE8:
        SetPos(object, pos + 1u);
        object[4] = script[Pos(object)];
        return g.kind() == 2 ? 0 : value;
    case 0xE9:
        if (g.op_e9(object, static_cast<signed char>(p[1]), static_cast<signed char>(p[2]),
                    static_cast<unsigned short>(p[3] << 8 | p[4]), static_cast<unsigned short>(p[5] << 8 | p[6]), p[7], p[8])) {
            SetPos(object, Pos(object) - 1u);
            return g.kind() == 2 ? Sprite_Current[8] : 0xFF;
        }
        SetPos(object, Pos(object) + 8u);
        return value;
    case 0xEA: {   // EA r g b a: a tint; a type-0x0A sprite keeps r g b itself
        unsigned char* const sprite = Sprite_Current;
        if (sprite[6] == 0x0A) {
            sprite[0x5D] = p[1];
            Sprite_Current[0x5E] = script[Pos(object) + 2u];
            Sprite_Current[0x5F] = script[Pos(object) + 3u];
        } else {
            g.tint(sprite, p[1], p[2], p[3], p[4]);
        }
        SetPos(object, Pos(object) + 4u);
        return value;
    }
    case 0xEB: Sprite_Current[0] ^= 0x40; return value;
    case 0xEC: Sprite_Current[7] ^= 4; return value;
    case 0xED: Sprite_Current[7] ^= 8; return value;
    case 0xEE: Sprite_Current[7] ^= 0x20; return value;
    case 0xEF:   // the end of the script: context bit 8
        object[0] |= 8;
        return value;
    default:
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
        SetLong(Sprite_Kind2 + 0x34, x);
        SetLong(Sprite_Kind2 + 0x38, z);
        SetWord(Sprite_Kind2 + 0x3E, static_cast<unsigned>(g.elevation(x, z)));
        return value;
    }
    case 0xD1:   // D1 hi lo x s: sound hi:lo when the sprite's script reaches x
        if (Field_Request != 3) {
            const std::uint16_t next = g.peek();
            SetWord(Scratch(), next);
            if (Word(Sprite_Current + 0x58) != next) {
                const unsigned char* const q = script + Pos(object);
                if (next == q[3]) g.play_by_id(static_cast<unsigned short>(q[1] << 8 | q[2]));
            }
        }
        SetPos(object, Pos(object) + 4u);
        return value;
    case 0xD2:
        Sprite_Current[7] ^= 0x40;
        Sprite_Current[0x48] = (Sprite_Current[7] & 0x40) ? 0 : 1;
        return value;
    case 0xD3:
        Sprite_Current[7] ^= 0x80;
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
            SetLong(Sprite_Current + 0x6C, S16(script + Pos(object) + 5));
            SetLong(Sprite_Current + 0x68, S16(script + Pos(object) + 3));
            SetLong(Sprite_Current + 0x64, S16(script + Pos(object) + 1));
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
        if (MoveScript_WaitWordDA != 0) SetPos(object, pos - 1u);
        return value;
    case 0xDB:
        g.op_db();
        SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(g.elevation(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38))));
        [[fallthrough]];
    case 0xDC:
        g.test_fb(static_cast<short>(Word(Sprite_Current + 0x36)), static_cast<short>(Word(Sprite_Current + 0x3A)));
        return value;
    case 0xDD:
        g.test_fc(static_cast<short>(Word(Sprite_Current + 0x36)), static_cast<short>(Word(Sprite_Current + 0x3A)));
        return value;
    case 0xDE: {   // DE n: the area's handler n, as flow op 03
        const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
        AreaHandler const* handlers;
        std::memcpy(&handlers, descriptor + 0x3C, sizeof handlers);
        handlers[p[1]]();
        const unsigned char result = Sprite_Current[8];
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
        const bool field = g.kind() == 1;
        const unsigned char n = g.tint(Sprite_Current, operand(1), operand(2), operand(3), operand(4));
        if (field) Field_State[0x149] = n;
        else ActiveMember()[0x9F] = n;
        SetPos(object, Pos(object) + 4u);
        return value;
    }
    case 0xC1: case 0xC2: {   // C1 / C2 r g b: brighten / darken that record
        const bool add = p[0] == 0xC1;
        const unsigned char* const owner = g.kind() == 1 ? Field_State + 0x149 : ActiveMember() + 0x9F;
        for (unsigned i = 0; i < 3; ++i) {
            unsigned char* const c = TintRecord(*owner) + 2 + i;
            *c = static_cast<unsigned char>(add ? *c + operand(1 + i) : *c - operand(1 + i));
        }
        SetPos(object, Pos(object) + 3u);
        return value;
    }
    case 0xC3: case 0xC4: {
        Sprite_Current[0x48] = p[1];
        const std::int32_t a = Fixed88(script + Pos(object) + 2);
        (p[0] == 0xC3 ? AddLong : SubLong)(Sprite_Current + 0x40, a);
        const std::int32_t b = Fixed88(script + Pos(object) + 4);
        (p[0] == 0xC3 ? AddLong : SubLong)(Sprite_Current + 0x44, b);
        SetPos(object, Pos(object) + 5u);
        return value;
    }
    case 0xC5:
        if (p[1] & 0x80) Sprite_Current[0] |= 0x20;
        else Sprite_Current[0] &= 0xDF;
        Sprite_Current[0x5C] = operand(1) & 3;
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xC6:
        Sprite_Current[0x5D] = static_cast<unsigned char>(Sprite_Current[0x5D] + p[1]);
        Sprite_Current[0x5E] = static_cast<unsigned char>(Sprite_Current[0x5E] + operand(2));
        Sprite_Current[0x5F] = static_cast<unsigned char>(Sprite_Current[0x5F] + operand(3));
        SetPos(object, Pos(object) + 3u);
        return value;
    case 0xC7: {
        Sprite_Current[0x48] = 2;
        static const unsigned kFields[] = {0x40, 0x44, 0x34, 0x38, 0x3C};
        for (unsigned i = 0; i < 5; ++i) AddLong(Sprite_Current + kFields[i], Fixed88(script + Pos(object) + 1 + 2 * i));
        SetPos(object, Pos(object) + 10u);
        return value;
    }
    case 0xC8: {   // C8 d s: turn toward direction d, one step a frame, s & 0xF to context +1
        unsigned char* const sprite = Sprite_Current;
        if (sprite[8] == p[1]) {
            SetPos(object, pos + 2u);
            return Sprite_Current[8];
        }
        const unsigned char s = p[2];
        return TurnStep(object, static_cast<unsigned char>((sprite[8] + ((s & 0x80) ? 1 : -1)) & 7), s & 0xF);
    }
    case 0xC9:
        Sprite_Current[0x2B] = p[1];
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xCA:
        ActiveMember()[0xA0] = p[1];
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xCB:
        g.animation_bank(static_cast<unsigned short>(p[1] << 8 | p[2]));
        g.set_animation(operand(3));
        Sprite_Current[0x2A] = operand(4) & 1;
        SetPos(object, Pos(object) + 4u);
        return value;
    case 0xCC:
        if (!(object[0] & 0x20) && (object[0] & 0x10)) SetPos(object, pos - 1u);
        return value;
    case 0xCD:
        g.tint_release(Sprite_Current);
        return value;
    case 0xCE: {   // turn toward ObjTrio 0's direction, reversed
        unsigned char* const sprite = Sprite_Current;
        const auto target = static_cast<unsigned char>(ObjTrio[8] ^ 4);
        const auto now = static_cast<unsigned char>(sprite[8] & 7);
        if (now == target) return sprite[8];
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
        const long* const at = g.handle_position(buffer, p[1], p[2]);
        const long x = at[0], y = at[1], z = at[2];
        SetLong(Sprite_Current + 0x34, x);
        SetLong(Sprite_Current + 0x38, y);
        SetLong(Sprite_Current + 0x3C, z);
        SetPos(object, Pos(object) + 2u);
        return value;
    }
    case 0x85: {   // the kind-2 object toward ObjTrio 0, repeating until it is there
        const long x = Field_Kind2X, to_x = Long(ObjTrio + 0x34), to_z = Long(ObjTrio + 0x38), z = Field_Kind2Z;
        if (x == to_x && z == to_z) {
            if (Field_Kind2Hold == 0) {
                g.set_elevation(g.elevation(x, z));
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
        g.elevation(x, z);
        const long nx = Long(ObjTrio + 0x34), nz = Long(ObjTrio + 0x38);
        Field_Kind2X = nx;
        Field_Kind2Z = nz;
        const long h = g.elevation(nx, nz);
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
        if (g.kind() == 1) {
            const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
            unsigned char* const* scripts;
            std::memcpy(&scripts, descriptor + 0x18, sizeof scripts);
            unsigned char* const next = scripts[script[Pos(object) + 1u]];
            std::memcpy(Field_State + 0x130, &next, sizeof next);
        } else {
            object[3] = script[Pos(object) + 1u];
        }
        SetPos(object, 0xFFFF);
        return value;
    }
    case 0x87: g.op_87(); return value;
    case 0x88: g.op_88(); return value;
    case 0x89:
        g.load_stream(static_cast<unsigned short>(p[1] << 8 | p[2]));
        SetPos(object, Pos(object) + 2u);
        return value;
    case 0x8A:
        SetPos(object, pos + 2u);
        return value;
    case 0x8B:
        if (g.stream_done() == 0) SetPos(object, Pos(object) - 1u);
        return value;
    case 0x8C: {
        unsigned char* const sprite = Sprite_Current;
        const auto arg = static_cast<signed char>(MoveScript_EffectArg[MoveScript_EffectState[Field_State[0x89]]]);
        const unsigned char n = g.effect_spawn_at(static_cast<unsigned char>(p[1] - 0x90), p[1] == 0x94 ? 10 : 0, arg,
                                                  Long(sprite + 0x34), Long(sprite + 0x38), Long(sprite + 0x3C));
        Sprite_Current[0xB] = n;
        SetPos(object, Pos(object) + 1u);
        return value;
    }
    default:
        return value;
    }
}

namespace {

// --- BOF3X_SHADOW=move_groups: a differential fuzz, once at start-up --------
// Seven byte-copies, one per handler, their jump tables moved into the copy
// and every call re-aimed at the recorders below; the area's handler table
// and +0x18 array are swapped for recorders and a table of the fuzz's. One
// round: a group, an op in (or just past) its range with random operands, and
// random state - the context, a sprite, the field state, a party object, the
// tint and party records, ObjTrio 0, Sprite_Kind2 and the globals the groups
// touch - with the boundaries each op tests seeded; theirs, then from the same
// state ours; all of it, the result and the recorders' log compared.

constexpr unsigned kBuf = 0x100, kScript = 0x10010, kLog = 128;
unsigned char g_object[kBuf], g_sprite[kBuf], g_field[0x200], g_script[kScript];
unsigned char g_descriptor[0x44];
unsigned char* g_scripts18[256];
long g_handle_buffer[4];
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {   // a pointer as the log keeps it
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    if (p == g_sprite) return 1;
    if (p == g_object) return 2;
    if (a == Address(Sprite_Kind2)) return 3;
    return static_cast<std::uint32_t>(a);
}
// A callee may move the position or turn the sprite: the handler must read them again.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) SetPos(g_object, Pos(g_object) + (h >> 8) % 5);
    if (h % 5 == 0) Sprite_Current[8] = static_cast<unsigned char>(h >> 16);
}
unsigned char Byte() { return static_cast<unsigned char>(Hash() >> 7); }

int __cdecl StubKind() { Record(1); return static_cast<int>(Hash() % 4); }
void __cdecl StubFace(unsigned char d) { Record(2, d); Disturb(); }
unsigned char __cdecl StubTurnTarget(unsigned char* o) { Record(3, Id(o)); Disturb(); return static_cast<unsigned char>(Hash()); }
void __cdecl StubOpE7(unsigned char n) { Record(4, n); }
unsigned char __cdecl StubOpE9(unsigned char* o, signed char a, signed char b, unsigned short c, unsigned short d,
                               unsigned char e, unsigned char f) {
    Record(5, Id(o), static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b) << 8 | e << 16 | static_cast<std::uint32_t>(f) << 24, c, d);
    Disturb();
    return Hash() % 2 ? Byte() | 1 : 0;
}
unsigned char __cdecl StubTint(unsigned char* s, unsigned char r, unsigned char gg, unsigned char b, unsigned char a) {
    Record(6, Id(s), r | gg << 8 | b << 16 | static_cast<std::uint32_t>(a) << 24);
    Disturb();
    return Byte();
}
void __cdecl StubTintRelease(unsigned char* s) { Record(7, Id(s)); }
void __cdecl StubSetAnimation(unsigned char a) { Record(8, a); Disturb(); }
unsigned char __cdecl StubAnimationBank(unsigned short b) { Record(9, b); Disturb(); return 0; }   // op CB ignores the result
long __cdecl StubElevation(long x, long z) { Record(10, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z)); return static_cast<long>(Hash()); }
void __cdecl StubSetElevation(int v) { Record(11, static_cast<std::uint32_t>(v)); }
unsigned short __cdecl StubPeek() {
    Record(12);
    const std::uint32_t h = Hash();
    const unsigned char operand = g_script[Pos(g_object) + 3u];
    switch (h % 4) {
    case 0: return Word(Sprite_Current + 0x58);
    case 1: return operand;
    case 2: SetWord(Sprite_Current + 0x58, operand); return operand;
    default: return static_cast<unsigned short>(h >> 8);
    }
}
void __cdecl StubPlayById(unsigned short id) { Record(13, id); }
void __cdecl StubSound(unsigned short id) { Record(14, id); }
void __cdecl StubMusicPlay(unsigned int t, int u) { Record(15, t & 0xFF, static_cast<std::uint32_t>(u)); }
void __cdecl StubFadeOutStop(int n) { Record(16, static_cast<std::uint32_t>(n)); }
void __cdecl StubFadeIn(int n) { Record(17, static_cast<std::uint32_t>(n) & 0xFF); }
void __cdecl StubFadeOut(int n) { Record(18, static_cast<std::uint32_t>(n) & 0xFF); }
void __cdecl StubOpB8(unsigned char b) { Record(19, b); }
void __cdecl StubOpDB() { Record(20); }
unsigned char __cdecl StubTestFB(short x, short z) { Record(21, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z)); return Byte(); }
unsigned char __cdecl StubTestFC(short x, short z) { Record(22, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z)); return Byte(); }
unsigned char __cdecl StubEffectSpawn(unsigned char k, signed char a, signed char b, short x, short z) {
    Record(23, k | static_cast<std::uint8_t>(a) << 8 | static_cast<std::uint8_t>(b) << 16, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z));
    Disturb();
    return Byte();
}
unsigned char __cdecl StubEffectSpawnAt(unsigned char k, signed char a, signed char b, long x, long y, long z) {
    Record(24, k | static_cast<std::uint8_t>(a) << 8 | static_cast<std::uint8_t>(b) << 16, static_cast<std::uint32_t>(x),
           static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(z));
    Disturb();
    return Byte();
}
// Fills the caller's buffer, or returns a buffer of its own: the handler must use what it returns.
long* __cdecl StubHandlePosition(long* out, unsigned char h, unsigned char b) {
    Record(25, h, b);
    long* const to = Hash() % 2 ? out : g_handle_buffer;
    for (unsigned i = 0; i < 4; ++i) to[i] = static_cast<long>(Hash() * (i + 1));
    return to;
}
void __cdecl StubOp87() { Record(26); }
void __cdecl StubOp88() { Record(27); }
void __cdecl StubLoadStream(unsigned id) { Record(28, id); }
int __cdecl StubStreamDone() { Record(29); return Hash() % 2 ? static_cast<int>(Hash() | 0x100u) : 0; }
template <unsigned I>
void __cdecl StubArea() {
    Record(40, I);
    Sprite_Current[8] = Byte();
}
template <std::size_t... I>
constexpr std::array<void*, sizeof...(I)> AreaStubs(std::index_sequence<I...>) {
    return {{reinterpret_cast<void*>(&StubArea<I>)...}};
}
const auto kAreaStubs = AreaStubs(std::make_index_sequence<256>());

const Callees kStubs = {
    StubKind, StubFace, StubTurnTarget, StubOpE7, StubOpE9, StubTint, StubTintRelease, StubSetAnimation,
    StubAnimationBank, StubElevation, StubSetElevation, StubPeek, StubPlayById, StubSound, StubMusicPlay,
    StubFadeOutStop, StubFadeIn, StubFadeOut, StubOpB8, StubOpDB, StubTestFB, StubTestFC, StubEffectSpawn,
    StubEffectSpawnAt, StubHandlePosition, StubOp87, StubOp88, StubLoadStream, StubStreamDone,
};

// Where each copy's calls went, by original address, and what they go to now.
const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x57C840: return f(&StubKind);
    case 0x57C4C0: return f(&StubFace);
    case 0x517E90: return f(&StubTurnTarget);
    case 0x518B20: return f(&StubOpE7);
    case 0x57C8E0: return f(&StubOpE9);
    case 0x454CC0: return f(&StubTint);
    case 0x454DC0: return f(&StubTintRelease);
    case 0x5891F0: return f(&StubSetAnimation);
    case 0x589590: return f(&StubAnimationBank);
    case 0x5720C0: return f(&StubElevation);
    case 0x5725F0: return f(&StubSetElevation);
    case 0x57CDC0: return f(&StubPeek);
    case 0x587900: return f(&StubPlayById);
    case 0x587740: return f(&StubSound);
    case 0x587AE0: return f(&StubMusicPlay);
    case 0x587B40: return f(&StubFadeOutStop);
    case 0x587BA0: return f(&StubFadeIn);
    case 0x587BE0: return f(&StubFadeOut);
    case 0x4DF820: return f(&StubOpB8);
    case 0x57CD40: return f(&StubOpDB);
    case 0x572650: return f(&StubTestFB);
    case 0x572790: return f(&StubTestFC);
    case 0x57CE10: return f(&StubEffectSpawn);
    case 0x57CE80: return f(&StubEffectSpawnAt);
    case 0x578DC0: return f(&StubHandlePosition);
    case 0x5795F0: return f(&StubOp87);
    case 0x5794D0: return f(&StubOp88);
    case 0x587910: return f(&StubLoadStream);
    case 0x587A00: return f(&StubStreamDone);
    default: bof3::Fatal("move_groups: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The seven copies: range, jump table, and every relative call out of the
// range as (E8 offset, original target) - by capstone, 2026-09-22.
struct Call { std::uint32_t offset, target; };
struct Group {
    const char* name;
    std::uint32_t base, size;
    Table table;
    const Call* calls;
    int n_calls;
    unsigned char first_op, last_op;
};
constexpr Call kCallsB[] = {{0x31, 0x587740}, {0x4a, 0x587740}, {0x64, 0x587740}, {0x7e, 0x587740}, {0x94, 0x587ae0}, {0xa4, 0x587b40},
                            {0xb8, 0x4df820}, {0xc9, 0x587b40}, {0xde, 0x587ba0}, {0xf3, 0x587be0}, {0x117, 0x587900}, {0x135, 0x587900},
                            {0x14c, 0x587ae0}};
constexpr Call kCallsE[] = {{0x42, 0x57c4c0}, {0x58, 0x517e90}, {0xa8, 0x57c4c0}, {0xd8, 0x517e90}, {0x164, 0x518b20}, {0x199, 0x57c8e0},
                            {0x1a9, 0x57c840}, {0x234, 0x57c840}, {0x2a3, 0x454cc0}};
constexpr Call kCallsD[] = {{0x4b, 0x5720c0}, {0x6a, 0x57cdc0}, {0xa2, 0x587900}, {0x350, 0x57cd40}, {0x362, 0x5720c0}, {0x383, 0x572650},
                            {0x3a2, 0x572790}};
constexpr Call kCallsC[] = {{0x32, 0x57c840}, {0x63, 0x454cc0}, {0xa8, 0x454cc0}, {0xc9, 0x57c840}, {0x1af, 0x57c840}, {0x4fd, 0x57c840},
                            {0x511, 0x57c4c0}, {0x52d, 0x5891f0}, {0x57c, 0x589590}, {0x58c, 0x5891f0}, {0x5dc, 0x454dc0}, {0x61d, 0x57c840},
                            {0x631, 0x57c4c0}, {0x642, 0x5891f0}};
constexpr Call kCalls8[] = {{0x8c, 0x578dc0}, {0x10c, 0x5720c0}, {0x112, 0x5725f0}, {0x150, 0x5720c0}, {0x16d, 0x5720c0}, {0x1cc, 0x57c840},
                            {0x235, 0x5795f0}, {0x246, 0x5794d0}, {0x260, 0x587910}, {0x28c, 0x587a00}, {0x2e4, 0x57ce80}};
constexpr Call kCalls9[] = {{0x5b, 0x57ce10}, {0x96, 0x57ce80}};
constexpr Call kCalls6[] = {{0xdf, 0x5891f0}};
#define BOF3_CALLS(a) a, static_cast<int>(sizeof a / sizeof a[0])
const Group kGroups[] = {
    {"MoveScript_GroupB", 0x577280, 0x198, {0x27, 0x15c, 15}, BOF3_CALLS(kCallsB), 0xB0, 0xBF},
    {"MoveScript_GroupE", 0x577420, 0x334, {0x2e, 0x2f4, 16}, BOF3_CALLS(kCallsE), 0xE0, 0xEF},
    {"MoveScript_GroupD", 0x577BD0, 0x438, {0x2f, 0x3f8, 16}, BOF3_CALLS(kCallsD), 0xD0, 0xDF},
    {"MoveScript_GroupC", 0x578010, 0x6a4, {0x2e, 0x668, 15}, BOF3_CALLS(kCallsC), 0xC0, 0xCF},
    {"MoveScript_Group8", 0x5786C0, 0x33c, {0x2f, 0x308, 13}, BOF3_CALLS(kCalls8), 0x80, 0x8F},
    {"MoveScript_Group9", 0x578A00, 0xf8, {0x3c, 0xd8, 4}, BOF3_CALLS(kCalls9), 0x90, 0x9F},
    {"MoveScript_Group6", 0x578B00, 0x110, {0x25, 0xf4, 7}, BOF3_CALLS(kCalls6), 0x60, 0x6F},
};
#undef BOF3_CALLS
constexpr unsigned kGroupCount = sizeof kGroups / sizeof kGroups[0];

using GroupFn = unsigned char (__cdecl*)(unsigned char*, const unsigned char*, unsigned char);
using GroupBFn = void (__cdecl*)(unsigned char*, const unsigned char*);
const void* const kOurs[kGroupCount] = {
    reinterpret_cast<const void*>(&MoveScript_GroupB), reinterpret_cast<const void*>(&MoveScript_GroupE),
    reinterpret_cast<const void*>(&MoveScript_GroupD), reinterpret_cast<const void*>(&MoveScript_GroupC),
    reinterpret_cast<const void*>(&MoveScript_Group8), reinterpret_cast<const void*>(&MoveScript_Group9),
    reinterpret_cast<const void*>(&MoveScript_Group6),
};

// Game memory both sides may touch, saved before the fuzz and put back after;
// randomised every round and compared after both passes.
struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(Sprite_Kind2), Sprite_Kind2_count},
    {Address(ObjTrio), 0x40},
    {Address(Sprite_ObjectsExtra), 8 * 0xA4},   // the party objects and what a slot up to 7 reaches
    {Address(MoveScript_TintRecords), MoveScript_TintRecords_count},
    {Address(MoveScript_PartyRecords), MoveScript_PartyRecords_count},
    {bof3::addr::DamageScratch - 8, 12},
    {Address(&Field_Kind2Z), 8},
    {Address(&Field_ScriptFlags), 2},
    {Address(&MoveScript_F3Divisor), 2},
    {Address(&MoveScript_FAWord), 2},
    {Address(&Field_Request), 1},
    {Address(&MoveScript_WaitWordDA), 2},
    {Address(&Field_Kind2Hold), 1},
    {Address(&MapView_Elevation), 4},
};
constexpr unsigned kRegionBytes = Sprite_Kind2_count + 0x40 + 8 * 0xA4 + MoveScript_TintRecords_count +
                                  MoveScript_PartyRecords_count + 12 + 8 + 2 + 2 + 2 + 1 + 2 + 1 + 4;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char object[kBuf], sprite[kBuf], field[0x200];
    unsigned char* current;
    unsigned char* active;
    std::uint16_t area;
    Entry log[kLog];
    unsigned log_n;
    unsigned char result;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.object, g_object, kBuf);
    std::memcpy(s.sprite, g_sprite, kBuf);
    std::memcpy(s.field, g_field, sizeof s.field);
    s.current = Sprite_Current;
    s.active = Field_ActiveMember;
    s.area = Game_AreaNumber;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_object, s.object, kBuf);
    std::memcpy(g_sprite, s.sprite, kBuf);
    std::memcpy(g_field, s.field, sizeof s.field);
    Sprite_Current = s.current;
    Field_ActiveMember = s.active;
    Game_AreaNumber = s.area;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x2545F491u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }

// The boundaries each op tests, seeded a third of the time.
void Seed(unsigned char op, unsigned char* p) {
    unsigned char* const sprite = g_sprite;
    switch (op) {
    case 0xC8: if (Often()) p[1] = sprite[8]; break;
    case 0xCE: if (Often()) sprite[8] = static_cast<unsigned char>((sprite[8] & 0xF8) | ((ObjTrio[8] ^ 4) & 7)); break;
    case 0xD4: if (Often()) p[7] = 0; break;
    case 0xD5: case 0xD6:
        if (Often()) {
            unsigned char* const r = PartyRecord(PartySlot() & 7);
            r[1] = static_cast<unsigned char>(0 - r[2]);
        }
        break;
    case 0xD9: if (Often()) Field_Request = 2; break;
    case 0xD1: case 0xDF: if (Often()) Field_Request = 3; break;
    case 0xDA: if (Often()) MoveScript_WaitWordDA = 0; break;
    case 0xEA: if (Often()) sprite[6] = 0x0A; break;
    case 0x9F: if (Often()) sprite[0xB] = static_cast<unsigned char>(Next() % 2); break;
    case 0x8C: if (Often()) p[1] = 0x94; break;
    case 0x85:
        if (Next() % 2) { Field_Kind2X = Long(ObjTrio + 0x34); Field_Kind2Z = Long(ObjTrio + 0x38); }
        else if (Often()) {
            Field_Kind2X = Long(ObjTrio + 0x34) + static_cast<long>(Next() % 0x10000) - 0x8000;
            Field_Kind2Z = Long(ObjTrio + 0x38) + static_cast<long>(Next() % 0x10000) - 0x8000;
        }
        if (Often()) Field_Kind2Hold = 0;
        break;
    default: break;
    }
}

void SelfTest(void* const (&theirs)[kGroupCount]) {
    constexpr unsigned kRounds = 30000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("move_groups: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    Capture(saved);
    unsigned char* const saved_field = Field_State;
    Field_State = g_field;
    const void* const area_table = kAreaStubs.data();
    std::memcpy(g_descriptor + 0x3C, &area_table, sizeof area_table);
    unsigned char** const scripts = g_scripts18;
    std::memcpy(g_descriptor + 0x18, &scripts, sizeof scripts);
    g = kStubs;

    unsigned bad = 0, calls = 0, per_group[kGroupCount] = {}, ops[256] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kGroupCount;
        const Group& grp = kGroups[k];
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, log); ++i) bytes[i] = static_cast<unsigned char>(Next());
        for (auto& s : g_scripts18) s = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(Next()));
        const unsigned start = Next() % 0xFF00;
        for (unsigned i = 0; i < 16; ++i) g_script[start + i] = static_cast<unsigned char>(Next());
        const unsigned span = grp.last_op - grp.first_op + 1u;
        const auto op = static_cast<unsigned char>(Next() % 16 ? grp.first_op + Next() % span : Next());
        g_script[start] = op;
        ++ops[op];
        ++per_group[k];
        SetPos(input.object, start);
        input.current = Next() % 5 ? g_sprite : Sprite_Kind2;
        // The party slot D4..D6 compute: every one of 0..7, so that D4's clamp
        // (above 4) and D5's bound (4 and up) are met; a few bytes into the
        // object sometimes, since the original divides, truncating. A uniform
        // pointer across the four objects met slot 4 once in ~650 rounds and
        // never above it - a control moving D4's clamp went unseen.
        {
            const unsigned slot = Next() % 8;
            input.active = At(Address(Sprite_ObjectsExtra) + slot * 0xA4 + (slot < 7 ? Next() % 3 : 0));
        }
        input.area = static_cast<std::uint16_t>(Next() % Area_Descriptors_count);
        const auto value = static_cast<unsigned char>(Next());
        g_seed = Next();
        // Seeding writes through the globals: apply, seed, and take that as the input.
        Apply(input);
        Seed(op, g_script + start);
        Capture(input);
        unsigned char* const saved_descriptor = Area_Descriptors[input.area];
        Area_Descriptors[input.area] = g_descriptor;

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const void* const fn = pass ? kOurs[k] : theirs[k];
            unsigned char r = 0;
            if (k == 0) reinterpret_cast<GroupBFn>(const_cast<void*>(fn))(g_object, g_script);
            else r = reinterpret_cast<GroupFn>(const_cast<void*>(fn))(g_object, g_script, value);
            State& out = pass ? our_out : their_out;
            Capture(out);
            out.result = r;
        }
        Area_Descriptors[input.area] = saved_descriptor;
        calls += their_out.log_n;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      move_groups self-test MISMATCH: round %u, %s op %02X, result %02X / %02X, position %04X / %04X, "
                      "log %u / %u", round, grp.name, op, their_out.result, our_out.result, Pos(their_out.object),
                      Pos(our_out.object), their_out.log_n, our_out.log_n);
    }
    g = kOriginals;
    Field_State = saved_field;
    Apply(saved);
    unsigned covered = 0, listed = 0;
    for (const Group& grp : kGroups)
        for (unsigned op = grp.first_op; op <= grp.last_op; ++op, ++listed) covered += ops[op] != 0;
    bof3::Log("shadow      move_groups self-test: %u rounds (%u per group), %u of %u ops of the seven groups generated, "
              "%u calls to the stand-ins, %u MISMATCHES; the context, sprite, field state, party objects, tint and party "
              "records, ObjTrio 0, Sprite_Kind2, eight globals, the result and the stand-ins' log compared",
              kRounds, per_group[0], covered, listed, calls, bad);
    if (bad) bof3::Fatal("the movement script's group handlers differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void MoveGroups_Inject() {
    if (bof3::WantsShadow("move_groups")) {
        void* clones[kGroupCount];
        for (unsigned k = 0; k < kGroupCount; ++k) {
            const Group& grp = kGroups[k];
            bof3::CloneCall calls[16];
            if (grp.n_calls > 16) bof3::Fatal("move_groups: %s has %d calls", grp.name, grp.n_calls);
            for (int i = 0; i < grp.n_calls; ++i) calls[i] = {grp.calls[i].offset, StubFor(grp.calls[i].target), grp.calls[i].target};
            clones[k] = bof3::CloneOriginal(grp.name, grp.base, grp.size, calls, grp.n_calls);
            Relocate(clones[k], grp.base, grp.size, grp.table);
        }
        SelfTest(clones);
    }
    BOF3_INJECT(MoveScript_Group6);
    BOF3_INJECT(MoveScript_Group8);
    BOF3_INJECT(MoveScript_Group9);
    BOF3_INJECT(MoveScript_GroupB);
    BOF3_INJECT(MoveScript_GroupC);
    BOF3_INJECT(MoveScript_GroupD);
    BOF3_INJECT(MoveScript_GroupE);
}
