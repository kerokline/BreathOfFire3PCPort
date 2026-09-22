// The movement commands and the party's moves - fifteen functions the movement
// script's interpreter (move_script.cpp, move_groups.cpp) and the field object
// update (field_objects.cpp) call, and the demo's party reset. Each is the PSX
// twin's where one was read (docs/move-cmds.md section 1 has the pairs). The
// start-up fuzz is move_cmds_fuzz.cpp.
#include "game/move_cmds.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_cmds_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"

namespace move_cmds {

const Callees kOriginals = {
    AreaMap_Elevation, Math_Ratan2, ScriptContext_Reset, MapView_SetElevation, MoveCmd_AttachOffset,
    MoveScript_ObjectKind, MoveCmd_Detach, MoveCmd_AttachMove, MoveCmd_HandlePosition, PartySet_Load, Party_Join,
};
Callees g = kOriginals;

}  // namespace move_cmds

namespace {

using namespace move_script;
using namespace move_cmds;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
unsigned char* Kind2() { return Sprite_Kind2; }

// The angle a party member turns to for a direction: Sprite_DirectionAngles,
// read at any index as the original reads it (the table has 8).
std::uint16_t DirectionAngle(unsigned char direction) { return Word(At(Address(Sprite_DirectionAngles) + direction * 2u)); }

// Math_Ratan2's doubles, bit for bit the image's (the fuzz checks them).
constexpr double kScale = 2048.0;
constexpr std::uint64_t kInversePiBits = 0x3FD461D59AE78A99ull;   // 0.3184713375796178 = 1 / 3.14
double InversePi() {
    double v;
    std::memcpy(&v, &kInversePiBits, sizeof v);
    return v;
}
const double kInversePi = InversePi();

}  // namespace

// original 0x5A7A70: an angle in 4096ths of a turn from a slope - libgte's
// ratan2 with pi taken as 3.14 (PSX 0x8017AAB0 is the real one, in integers).
// x87 throughout, as the original: fpatan of y over x at the full 64 bits
// (precision control does not touch it), then two fmul by doubles, each
// rounded to whatever the control word's precision is at the call - 53 bits
// in game - and the CRT's _ftol 0x5B9550: round-toward-zero set in a copy of
// the control word for one fistp to 64 bits, then the word put back. The
// caller gets the low dword; _ftol also leaves the high one in edx, which no
// caller reads (int result; 70 call sites, rel32 scan).
extern "C" int __cdecl Math_Ratan2(float y, float x) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds %[y]\n\t"
        "flds %[x]\n\t"
        "fpatan\n\t"
        "fmull %[scale]\n\t"
        "fmull %[inverse_pi]\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [y] "m"(y), [x] "m"(x), [scale] "m"(kScale), [inverse_pi] "m"(kInversePi)
        : "eax", "st", "st(1)");
    return static_cast<int>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(result)));
}

// original 0x5190A0: a party member's move toward `direction` (PSX
// FUN_801A4088): the velocity op D4's record of its party slot filled for the
// frames Sprite_Current +0xA says - the two tilts and the turn, each a step
// per frame. The slot is (object - Sprite_ObjectsExtra) / 0xA4, its low byte
// taken signed and 4 when negative, as Party_ApplyRecord's is; a record with
// bit 0 of +0 set is left alone. With a velocity (+0xC + +0x10 non-zero) the
// ground's rise one step ahead, `Field_DirectionSteps[direction]`, becomes a
// tilt through Math_Ratan2(rise, 128): directions with bit 1 (2, 3, 6, 7) tilt
// +0x68's axis and zero +0x64, the others the reverse; the rise's sign is
// flipped except for direction 3 and direction 1 respectively.
//
// As the original has it: the object pointer is only arithmetic; the slot
// byte can index up to record 127; the tilts' targets are the sprite's +0x64 /
// +0x68 as found on entry unless replaced, the one zeroed included - so the
// zeroed axis steps from its old value to its old value, a count of 0; the
// divisions are by n * 2 and n * 4 with n the byte +0xA, never 0 there (the
// record's counts are n << 1 and n << 2 as bytes, which wrap); directions 5
// and 6 turn the long way when the sprite's angle & 0xFFF is past direction
// 3's; any direction byte indexes both tables (callers pass 0..7).
extern "C" void __cdecl Party_MoveMember(unsigned char* object, unsigned char direction) {
    unsigned char* sprite = Sprite_Current;
    std::int32_t tilt_x = Long(sprite + 0x64);
    std::int32_t tilt_y = Long(sprite + 0x68);
    const std::int32_t index = static_cast<std::int32_t>(Address(object) - Address(Sprite_ObjectsExtra)) / 0xA4;
    signed char slot = static_cast<signed char>(index & 0xFF);
    if (slot < 0) slot = 4;
    unsigned char* const record = MoveScript_PartyRecords + slot * 16;
    if (record[0] & 1) return;
    if (Add(Long(sprite + 0x10), Long(sprite + 0xC)) != 0) {
        const unsigned char* const step = At(Address(Field_DirectionSteps) + direction * 8u);
        const long ground = g.elevation(Add(Long(step), Long(sprite + 0x34)), Add(Long(step + 4), Long(sprite + 0x38)));
        sprite = Sprite_Current;
        const auto rise = static_cast<std::uint16_t>(static_cast<std::uint32_t>(ground) - Word(sprite + 0x3E));
        if (direction & 2) {
            SetLong(sprite + 0x64, 0);
            const auto slope = static_cast<short>(direction == 3 ? rise : 0u - rise);
            tilt_y = g.ratan2(static_cast<float>(slope), 128.0f);
        } else {
            const auto slope = static_cast<short>(direction == 1 ? rise : 0u - rise);
            tilt_x = g.ratan2(static_cast<float>(slope), 128.0f);
            SetLong(Sprite_Current + 0x68, 0);
        }
        sprite = Sprite_Current;
    }
    const unsigned char frames = sprite[0xA];
    if (frames == 0) {
        SetLong(record + 0xC, 0);
        SetLong(record + 8, 0);
        SetLong(record + 4, 0);
        SetLong(sprite + 0x6C, DirectionAngle(direction));
        return;
    }
    record[1] = static_cast<unsigned char>(frames << 1);
    record[2] = static_cast<unsigned char>(sprite[0xA] << 2);
    SetLong(record + 4, Sub(tilt_x, Long(sprite + 0x64)) / static_cast<std::int32_t>(sprite[0xA] * 2u));
    SetLong(record + 8, Sub(tilt_y, Long(sprite + 0x68)) / static_cast<std::int32_t>(sprite[0xA] * 2u));
    std::uint16_t turn_to = DirectionAngle(direction);
    if ((direction == 5 || direction == 6) &&
        static_cast<std::int32_t>(DirectionAngle(3)) < (Long(sprite + 0x6C) & 0xFFF))
        turn_to = static_cast<std::uint16_t>(turn_to + 0x1000);
    SetLong(record + 0xC, (static_cast<short>(turn_to) - (Long(sprite + 0x6C) & 0xFFF)) /
                              static_cast<std::int32_t>(sprite[0xA] * 4u));
}

// original 0x518B00: a pose held while the context's +1 counts down (PSX
// FUN_801A3764): Sprite_Current[3] = [1], [1] = 5. The argument is unused.
extern "C" void __cdecl Sprite_SetPoseWait(unsigned char* context) {
    (void)context;
    Sprite_Current[3] = Sprite_Current[1];
    Sprite_Current[1] = 5;
}

// original 0x573400: the kind-2 object's move (PSX FUN_801C6DF4), the twin of
// MoveCmd_Move: the divisor speed * (4 for directions 2 and 6, else 8) to
// MoveScript_F3Divisor; the position moved by kKind2Steps[direction] * steps *
// (0x80 / divisor) * speed, kept in Field_Kind2X / Z as well; then the rise to
// the ground there spread over (0x80 / divisor) * steps frames into +0x14, and
// MoveScript_FAWord cleared.
//
// As the original has it: every product wraps in 32 bits; the divisor is 16
// bits; the rise is (ground - height) as 16-bit values, shifted up 16 and
// wrapping; the steps byte and the divisor are read again after the ground;
// a speed of 0, a divisor above 0x80 or 0 steps divide by zero - as the
// original faults (Field_MoveSpeeds is 0, 1, 2, 4, 8, 16, so only entry 0 or
// a speed index past the table can); any direction byte indexes the steps.
extern "C" void __cdecl MoveCmd_MoveKind2(unsigned char direction) {
    unsigned char* const k2 = Kind2();
    const unsigned multiplier = (direction == 2 || direction == 6) ? 4 : 8;
    const unsigned char speed = At(Address(Field_MoveSpeeds))[k2[kK2Speed]];
    const auto divisor = static_cast<short>(static_cast<std::uint16_t>(speed * multiplier));
    MoveScript_F3Divisor = divisor;
    const std::int32_t per_frame = 0x80 / static_cast<std::int32_t>(divisor);
    const unsigned char* const step = At(kKind2Steps + direction * 8u);
    const std::int32_t steps = k2[kK2Steps];
    const std::int32_t dx = Mul(Mul(Mul(Long(step), steps), per_frame), speed);
    const std::int32_t dz = Mul(Mul(Mul(Long(step + 4), steps), per_frame), speed);
    const std::int32_t x = Add(Long(k2 + kK2X), dx);
    const std::int32_t z = Add(Long(k2 + kK2Z), dz);
    SetLong(k2 + kK2X, x);
    SetLong(k2 + kK2Z, z);
    Field_Kind2X = x;
    Field_Kind2Z = z;
    const long ground = g.elevation(x, z);
    const std::int32_t frames = Mul(0x80 / static_cast<std::int32_t>(MoveScript_F3Divisor), k2[kK2Steps]);
    MoveScript_FAWord = 0;
    const std::int32_t rise = static_cast<short>(static_cast<std::uint16_t>(ground)) - static_cast<short>(Word(k2 + kK2Height));
    SetLong(k2 + kK2Rise, static_cast<std::int32_t>(static_cast<std::uint32_t>(rise) << 16) / frames);
}

// original 0x5734F0: the kind-2 object placed (PSX FUN_801C7014): its script
// context reset, +0x85 bit 7, script `script` at +0x83, speed index 3, at
// (Field_Kind2X, Field_Kind2Z) on the ground, and +1 = 1, +2 = +3 = 0.
// As the original has it: +0x85 is read after the reset (which clears it);
// MapView_SetElevation gets the ground's whole dword where the PSX passes it
// as a short - AreaMap_Elevation's high half is not always 0.
extern "C" void __cdecl Kind2_Place(unsigned char script) {
    unsigned char* const k2 = Kind2();
    g.context_reset(k2 + kK2Context);
    k2[kK2Flags] |= 0x80;
    const long z = Field_Kind2Z;
    k2[kK2Script] = script;
    const long x = Field_Kind2X;
    k2[kK2Speed] = 3;
    SetLong(k2 + kK2X, x);
    SetLong(k2 + kK2Z, z);
    const long ground = g.elevation(x, z);
    SetWord(k2 + kK2Height, static_cast<std::uint32_t>(ground));
    g.set_elevation(ground);
    k2[1] = 1;
    k2[2] = 0;
    k2[3] = 0;
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
    const unsigned char* const sprite = Sprite_Current;
    const std::int32_t dz = Sub(z, Long(sprite + 0x38));
    const std::int32_t dx = Sub(x, Long(sprite + 0x34));
    const long ground = g.elevation(x, z);
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<std::uint32_t>(ground) - Word(Sprite_Current + 0x3E)));
    if (context[4] == 0) return;
    const unsigned char n = frames ? frames : 1;
    Sprite_Current[9] = n;
    Sprite_Current[0xA] = static_cast<unsigned char>(n + 1);
    SetLong(Sprite_Current + 0xC, dx / n);
    SetLong(Sprite_Current + 0x10, dz / n);
    SetLong(Sprite_Current + 0x14, rise / n);
    Sprite_Current[0] |= 0x10;
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
    std::memcpy(At(kSavedSprite), &Sprite_Current, 4);
    unsigned char* object;
    if (handle & 0x80) {
        unsigned char count = 0;
        for (unsigned char n = 0; n < 0x1E; ++n) {
            if (Sprite_Objects[n * 0xA4u + 6] != 0x0A) continue;
            if (count == (handle & 0x3F)) break;
            ++count;
        }
        object = Sprite_Objects + count * 0xA4u;
    } else {
        object = Sprite_ObjectsExtra + handle * 0xA4u;
    }
    Sprite_Current = object;
    long buffer[4] = {};
    g.attach_offset(buffer, offset);
    const unsigned char* const at = Sprite_Current;
    const std::int32_t x = Add(buffer[0], Long(at + 0x34));
    const std::int32_t z = Add(buffer[1], Long(at + 0x38));
    const std::int32_t y = Add(buffer[2], static_cast<short>(Word(at + 0x3E)));
    std::memcpy(&Sprite_Current, At(kSavedSprite), 4);
    out[0] = x;
    out[1] = z;
    out[2] = y;
    out[3] = buffer[3];
    return out;
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
    if (g.object_kind() == 1) {
        if (Sprite_Current[2] == 1) g.detach();
        if (mode != 7) {
            Sprite_Current[2] = 0;
            return;
        }
        Sprite_Current[2] = 1;
        SetLong(Sprite_Current + 0x18, handle);
        SetLong(Sprite_Current + 0x1C, s);
        if (context[4] != 0) {
            g.attach_move(t);
            context[7] = 1;
        }
        SetPos(context, Pos(context) + 3u);
        return;
    }
    if (Sprite_Current[1] == 7) g.detach();
    Sprite_Current[1] = mode;
    if (Sprite_Current[1] != 7) return;
    SetLong(Sprite_Current + 0x18, handle);
    SetLong(Sprite_Current + 0x1C, s);
    if (context[4] != 0) g.attach_move(t);
    SetPos(context, Pos(context) + 3u);
}

// original 0x579390: detach (PSX FUN_801ACEE0): +0x24 bit 5 cleared, and for
// the object kind 1 +2 too.
extern "C" void __cdecl MoveCmd_Detach(void) {
    Sprite_Current[0x24] &= 0xDF;
    if (g.object_kind() == 1) Sprite_Current[2] = 0;
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
    const long* const at = g.handle_position(buffer, handle, offset);
    const std::int32_t x = at[0], z = at[1], y = at[2];
    const unsigned char count = t ? t : 1;
    Sprite_Current[9] = count;
    const std::int32_t n = static_cast<signed char>(count);
    SetLong(Sprite_Current + 0xC, Sub(x, Long(Sprite_Current + 0x34)) / n);
    SetLong(Sprite_Current + 0x10, Sub(z, Long(Sprite_Current + 0x38)) / n);
    SetLong(Sprite_Current + 0x14, Sub(y, static_cast<short>(Word(Sprite_Current + 0x3E))) / n);
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

// original 0x57C310: the movement script's variables (PSX 0x8015C260), read
// by flow ops 05, 07 and 09 - a jump table at 0x57C3A4 of one-line reads.
// As the original has it: 14 is 0 again (the table's last entry is case 0's);
// 13 is the constant 1; past 14, 0xFF.
extern "C" int __cdecl MoveScript_Variable(unsigned char index) {
    switch (index) {
    case 0: case 14: return Cond_ByteFA;
    case 1: return Game_AreaNumber;
    case 2: {
        const unsigned char* flags;
        std::memcpy(&flags, At(kFlagsPtr), sizeof flags);
        return Long(flags);
    }
    case 3: case 4: case 5: case 6: return At(kCounters)[index - 3];
    case 7: return MoveScript_Var7;
    case 8: return Cond_ByteFD;
    case 9: return Field_StatusBits & 1;
    case 10: return Word(Sprite_Current + 0x58);
    case 11: return At(kNameIndex)[0];
    case 12: return Long(At(kStoryFlags));
    case 13: return 1;
    default: return 0xFF;
    }
}

// original 0x57C840: what Sprite_Current is (PSX 0x8015CB38): 2 Sprite_Kind2,
// 1 one of the three ObjTrio objects, 0 one of the 30 Sprite_Objects, 3 one of
// the four Sprite_ObjectsExtra.
// As the original has it: anything else returns the pointer itself.
extern "C" int __cdecl MoveScript_ObjectKind(void) {
    const std::uint32_t sprite = Address(Sprite_Current);
    if (sprite == Address(Sprite_Kind2)) return 2;
    const std::uint32_t trio = Address(ObjTrio);
    if (sprite == trio || sprite == trio + 0x14C || sprite == trio + 0x298) return 1;
    if (sprite >= Address(Sprite_Objects) && sprite < Address(Sprite_Objects) + Sprite_Objects_count) return 0;
    if (sprite >= Address(Sprite_ObjectsExtra) && sprite < Address(Sprite_ObjectsExtra) + Sprite_ObjectsExtra_count) return 3;
    return static_cast<int>(sprite);
}

// original 0x57CDC0: the frame script's position after its next step (PSX
// 0x8015D48C), for op FE: unless the tick byte +0x4A is 1, the position +0x58
// as it is; with it, 2 when position / 2 is the length byte +0x49, else past
// the entry at +0x50 + position - 2 bytes, or 2 + 2 * its second byte when
// its first is 0x80 or more.
// As the original has it: position / 2 is compared in 16 bits; the sums wrap
// at 16 bits.
extern "C" unsigned short __cdecl Sprite_ScriptPeek(void) {
    const unsigned char* const sprite = Sprite_Current;
    if (sprite[0x4A] != 1) return Word(sprite + 0x58);
    const std::uint16_t position = Word(sprite + 0x58);
    if (static_cast<std::uint16_t>(position >> 1) == sprite[0x49]) return 2;
    const unsigned char* data;
    std::memcpy(&data, sprite + 0x50, sizeof data);
    const unsigned char* const entry = data + position;
    if (entry[0] >= 0x80) return static_cast<unsigned short>(entry[1] * 2u + 2u);
    return static_cast<unsigned short>(position + 2u);
}

// original 0x519890: chapter 16's call-table entry 0 (Scenario_CallA(0) from
// Scena16_Start): PartySet_Load(0xA, 0xFF, 0xFF, 0), both party lists emptied
// (0xFF), bits 0..2 of the flag byte before them set, Field_MemberCount 0,
// then Party_Join(0xA). No PSX twin read.
// As the original has it: the flag byte is read after PartySet_Load and
// written after the lists. The original returns Party_Join's al; Scenario_CallA's
// callers never read eax (symbols.toml there).
extern "C" void __cdecl Scena16_PartyReset(void) {
    g.partyset_load(0xA, 0xFF, 0xFF, 0);
    const auto flags = static_cast<unsigned char>(At(kPartyFlags)[0] | 7);
    for (unsigned i = 0; i < 6; ++i) At(kPartyLists)[i] = 0xFF;
    At(kPartyFlags)[0] = flags;
    Field_MemberCount = 0;
    g.party_join(0xA);
}

void MoveCmds_Inject() {
    if (bof3::WantsShadow("move_cmds")) move_cmds::SelfTest();
    BOF3_INJECT(Math_Ratan2);
    BOF3_INJECT(Party_MoveMember);
    BOF3_INJECT(Sprite_SetPoseWait);
    BOF3_INJECT(MoveCmd_MoveKind2);
    BOF3_INJECT(Kind2_Place);
    BOF3_INJECT(MoveCmd_OpF7);
    BOF3_INJECT(MoveCmd_HandlePosition);
    BOF3_INJECT(MoveCmd_Attach);
    BOF3_INJECT(MoveCmd_Detach);
    BOF3_INJECT(MoveCmd_AttachMove);
    BOF3_INJECT(MoveScript_FindLabel);
    BOF3_INJECT(MoveScript_Variable);
    BOF3_INJECT(MoveScript_ObjectKind);
    BOF3_INJECT(Sprite_ScriptPeek);
    BOF3_INJECT(Scena16_PartyReset);
}
