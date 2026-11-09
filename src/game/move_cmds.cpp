// The movement commands and the party's moves - sixteen functions the movement
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
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace move_cmds {

const Callees kOriginals = {
    AreaMap_Elevation, Math_Ratan2, ScriptContext_Reset, MapView_SetElevation, MoveCmd_AttachOffset,
    MoveScript_ObjectKind, MoveCmd_Detach, MoveCmd_AttachMove, MoveCmd_HandlePosition, PartySet_Load, Party_Join,
    AreaMap_Slope,
};
Callees g = kOriginals;

}  // namespace move_cmds

namespace {

using namespace move_script;
using namespace move_cmds;

std::uint32_t AddressMoveCmds(const volatile void* p) { return Low32(p); }
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

// Sprite_Current may be a Sprite_Objects record or another family whose
// first 0x80 bytes are a sprite object's (an ObjTrio member, the kind-2
// object), so only that part is named: rv::Current<records::SpriteHead>()
// (C2 audit frs F3; typed-data-round.md 3, "C2 audit as run").

// The angle a party member turns to for a direction: Sprite_DirectionAngles,
// read at any index as the original reads it (the table has 8).
std::uint16_t DirectionAngle(unsigned char direction) { return move_script::Word(move_script::At(AddressMoveCmds(Sprite_DirectionAngles) + direction * 2u)); }

}  // namespace

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
    records::SpriteHead* sprite = rv::Current<records::SpriteHead>();
    std::int32_t tilt_x = static_cast<std::int32_t>(sprite->angle[0]);
    std::int32_t tilt_y = static_cast<std::int32_t>(sprite->angle[1]);
    const std::int32_t index = static_cast<std::int32_t>(AddressMoveCmds(object) - AddressMoveCmds(rv::Bytes(Sprite_ObjectsExtra))) / 0xA4;
    signed char slot = static_cast<signed char>(index & 0xFF);
    if (slot < 0) slot = 4;
    unsigned char* const record = MoveScript_PartyRecords + slot * 16;
    if (record[0] & 1) return;
    if (Add(sprite->step_z, sprite->step_x) != 0) {
        const unsigned char* const step = move_script::At(AddressMoveCmds(Field_DirectionSteps) + direction * 8u);
        const long ground = move_cmds::g.elevation(Add(move_script::Long(step), sprite->x), Add(move_script::Long(step + 4), sprite->z));
        sprite = rv::Current<records::SpriteHead>();
        const auto rise = static_cast<std::uint16_t>(static_cast<std::uint32_t>(ground) - sprite->height_whole);
        if (direction & 2) {
            sprite->angle[0] = 0;
            const auto slope = static_cast<short>(direction == 3 ? rise : 0u - rise);
            tilt_y = move_cmds::g.ratan2(static_cast<float>(slope), 128.0f);
        } else {
            const auto slope = static_cast<short>(direction == 1 ? rise : 0u - rise);
            tilt_x = move_cmds::g.ratan2(static_cast<float>(slope), 128.0f);
            rv::Current<records::SpriteHead>()->angle[1] = 0;
        }
        sprite = rv::Current<records::SpriteHead>();
    }
    const unsigned char frames = rv::Bytes(sprite)[0xA];
    if (frames == 0) {
        move_script::SetLong(record + 0xC, 0);
        move_script::SetLong(record + 8, 0);
        move_script::SetLong(record + 4, 0);
        sprite->angle[2] = DirectionAngle(direction);
        return;
    }
    record[1] = static_cast<unsigned char>(frames << 1);
    record[2] = static_cast<unsigned char>(rv::Bytes(sprite)[0xA] << 2);
    move_script::SetLong(record + 4, Sub(tilt_x, static_cast<std::int32_t>(sprite->angle[0])) / static_cast<std::int32_t>(rv::Bytes(sprite)[0xA] * 2u));
    move_script::SetLong(record + 8, Sub(tilt_y, static_cast<std::int32_t>(sprite->angle[1])) / static_cast<std::int32_t>(rv::Bytes(sprite)[0xA] * 2u));
    std::uint16_t turn_to = DirectionAngle(direction);
    if ((direction == 5 || direction == 6) &&
        static_cast<std::int32_t>(DirectionAngle(3)) < (static_cast<std::int32_t>(sprite->angle[2]) & 0xFFF))
        turn_to = static_cast<std::uint16_t>(turn_to + 0x1000);
    move_script::SetLong(record + 0xC, (static_cast<short>(turn_to) - (static_cast<std::int32_t>(sprite->angle[2]) & 0xFFF)) /
                              static_cast<std::int32_t>(rv::Bytes(sprite)[0xA] * 4u));
}

// original 0x518B00: a pose held while the context's +1 counts down (PSX
// FUN_801A3764): Sprite_Current[3] = [1], [1] = 5. The argument is unused.
extern "C" void __cdecl Sprite_SetPoseWait(unsigned char* context) {
    (void)context;
    Sprite_Current[3] = rv::Current<records::SpriteHead>()->state;
    rv::Current<records::SpriteHead>()->state = 5;
}

// original 0x519890: chapter 16's call-table entry 0 (Scenario_CallA(0) from
// Scena16_Start): PartySet_Load(0xA, 0xFF, 0xFF, 0), both party lists emptied
// (0xFF), bits 0..2 of the flag byte before them set, Field_MemberCount 0,
// then Party_Join(0xA). No PSX twin read.
// As the original has it: the flag byte is read after PartySet_Load and
// written after the lists. The original returns Party_Join's al; Scenario_CallA's
// callers never read eax (symbols.toml there).
extern "C" void __cdecl Scena16_PartyReset(void) {
    move_cmds::g.partyset_load(0xA, 0xFF, 0xFF, 0);
    const auto flags = static_cast<unsigned char>(Cond_Flags[0xd1] | 7);
    for (unsigned i = 0; i < 6; ++i) Cond_Flags[0xd2 + i] = 0xFF;
    Cond_Flags[0xd1] = flags;
    Field_MemberCount = 0;
    move_cmds::g.party_join(0xA);
}

void MoveCmds_Inject() {
    if (bof3::WantsShadow("move_cmds")) move_cmds::SelfTest();
    BOF3_INJECT(Party_MoveMember);
    BOF3_INJECT(Sprite_SetPoseWait);
    BOF3_INJECT(Scena16_PartyReset);
}
