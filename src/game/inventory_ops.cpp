// Group BI of the seventh round: the random encounter's placement on the field
// map and the battle intro's party steps. docs/inventory_ops.md.
//
//   - The placement, 0x5920E0..0x592EFE (PSX GAME.EMI section 0,
//     0x801C73A8..0x801C881C): under 0x591F30 (not ours - nobody's this
//     round), which Field_EncounterDue asks whether a fight fits where the
//     party stands. The party is put round a centre by a formation table,
//     each member tried at 16 cells; the enemies round it by a triangle of
//     offsets and 49 jitters; every spot must fit the cell map, keep apart
//     from the others, be reachable in steps from the centre or a member,
//     and (0x5928F0) project on screen; then the camera is aimed (0x592A30).
//   - The intro's party steps, 0x532550..0x532C0E, 0x52F570 and 0x534880
//     (PSX 0x801BFBA8.., 0x801B3CF8, 0x801C3998): the initiative roll, the
//     turn to face the fight, the shade fade out and back, the walk to the
//     placed spots.
//
// Every call goes through inventory_ops::g (inventory_ops_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. "SC" is Sprite_Current, read afresh wherever the
// original reads [0x937F88] again, and held where it holds it in a register.
#include "game/inventory_ops.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/inventory_ops_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace inventory_ops {

namespace {
template <typename F>
F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Party_Count,
    AreaMap_Elevation,
    Sprite_ObjectAt,
    Gte_RotTransPers,
    Gte_RotMatrix,
    Gte_ApplyMatrix,
    Gte_SetRotMatrix,
    Gte_SetTransMatrix,
    Rand,
    MapView_GroundAt,
    Effect_FindFree,
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned char*, unsigned)>(::bof3::Code(kSetAnimFrom.v)),
    Sprite_ScriptTick,
    Sprite_ShadeFadeBegin,
    Sprite_ShadeRaise,
    Sprite_ScriptTickOnce,
    Sprite_SetAnimation,
    Encounter_PlaceMember,
    Encounter_MemberClear,
    Encounter_MemberStands,
    Encounter_EnemyClear,
    AreaMap_CellNibble,
    Encounter_Project,
    Encounter_Apart,
    Encounter_CellFits,
    Encounter_PathClear,
    Short_Abs,
    Short_Sign,
    Encounter_StepOpen,
    Sprite_TurnSense,
    Sprite_ShadeLower,
};
Callees g = kOriginals;

}  // namespace inventory_ops

using namespace inventory_ops;

namespace {

// move_script::SetWord is named in full here (step B0, analysis/tu_renames.tsv)
// move_script::Word is named in full here (step B0, analysis/tu_renames.tsv)

using i32 = std::int32_t;
using u32 = std::uint32_t;

unsigned char* PosX(u32 m) { return move_script::At(::bof3::Img(inventory_ops::at::kMemberPos.v) + 8 * m); }
unsigned char* PosZ(u32 m) { return move_script::At(::bof3::Img(inventory_ops::at::kMemberPos.v) + 8 * m + 4); }
unsigned char* Obj(u32 i) { return rv::Bytes(ObjTrio) + at::kObjStride * i; }

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does;
// rv::As<records::PartyMember>(p) is given party members (ObjTrio,
// Field_State), rv::As<records::EffectObject>(p) effect objects
// (Effect_Objects).
unsigned char& ByteInventoryOps(u32 address) { return *move_script::At(address); }

// A shift of a value the original holds as a 32-bit register.
i32 Shl(i32 v, unsigned n) { return static_cast<i32>(static_cast<u32>(v) << n); }
i32 AddInventoryOps(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }

}  // namespace

// ===========================================================================
// The placement: the cell map and the geometry under it

// original 0x5928F0 (PSX 0x801C805C): whether a thing of size class `size`
// at (x, z) is on screen. Its four corners, (size + 1) * 0x4000 out, each on
// the ground (AreaMap_Elevation) and projected: refused if any screen x is
// 320 or more or any y 240 or more. Then the first corner again: yes if its
// screen y less `margin` (a byte) is 8 or more. The PSX compares unsigned
// shorts, so a corner left of or above the screen fails there too; the PC's
// floats accept one (docs/inventory_ops.md section 5). A NaN passes the
// corners and fails the last test, as the x87 compares have it.
//
// DIVERGENCE DIV-0116 (D56): the PSX's lower bounds restored. Its twin,
// GAME.EMI 0x801C805C, loads each corner's screen x and y with lhu and tests
// sltiu 0x140 / sltiu 0xF0 (0x801C80D4, 0x801C80E8), so a corner left of or
// above the screen - a negative short - is refused as well; the port's float
// compares kept only the upper bounds. Once the flag is on, a corner whose
// screen x or y is below 0 is refused too (a NaN still passes, as the upper
// tests let it). InventoryOps_Inject sets it after the self-test, whose fuzz
// compares the original's bounds; BOF3X_ORIGINAL=EncounterOnScreenLowerBounds
// keeps ours with the port's bounds (docs/d56-encounter-bounds.md). Once the
// self-test is past, a corner below 0 is logged either way ("D56 ... refused"
// or "... passed, the port's bounds"), so a run shows where the two part; the
// log changes nothing the game reads.
unsigned char g_on_screen_lower_bounds = 0;
bool g_on_screen_log = false;

// ===========================================================================
// The placement: the party

// ===========================================================================
// The placement: the enemies

// ===========================================================================
// The battle intro's party steps

// original 0x52F570 (PSX 0x801B3CF8): which way Sprite_Current turns to face
// direction `target` (its byte): its direction +8 (plus 8, as a byte, when
// below the target) 1..4 past the target: 0xFF (anticlockwise, by the
// numbers), else 1. Ten call sites; each stores al in the sprite's +0xB.
extern "C" unsigned char __cdecl Sprite_TurnSense(unsigned target) {
    auto d = Sprite_Current[8];
    const auto a = static_cast<unsigned char>(target);
    if (d < a) d = static_cast<unsigned char>(d + 8);
    const u32 v = d, t = a;
    if (v >= t + 1 && v <= t + 4) return 0xFF;
    return 1;
}

// original 0x534880 (PSX 0x801C3998): Sprite_ShadeRaise's mirror - each of
// Sprite_Current's shade bytes +0x5D..+0x5F that is not 0x80 less the step
// (both signed bytes, a 16-bit difference), 0x80 when that is below -128, else
// its low byte (so a difference above 127 wraps, and 0x80 can come of it).
// Answers whether all three are 0x80.
extern "C" unsigned char __cdecl Sprite_ShadeLower(unsigned step) {
    const auto s = static_cast<std::int16_t>(static_cast<signed char>(step));
    for (unsigned at = 0x5D; at <= 0x5F; ++at) {
        const unsigned char c = Sprite_Current[at];
        if (c == 0x80) continue;
        const auto v = static_cast<std::int16_t>(static_cast<std::int16_t>(static_cast<signed char>(c)) - s);
        Sprite_Current[at] = v < -128 ? 0x80 : static_cast<unsigned char>(v);
    }
    const unsigned char* const sc = Sprite_Current;
    return sc[0x5D] == 0x80 && sc[0x5E] == 0x80 && sc[0x5F] == 0x80;
}

// original 0x532550 (PSX 0x801BFBA8): the intro's initiative roll. ObjTrio's
// first +0xB is 0xFF; for five slots (ObjTrio holds three: slots at or past
// Field_MemberCount, read per slot, never read theirs) r = Rand & 0x7F,
// redrawn above 100. A member notices when its record's byte +0x38 (by the
// first list and 0x66972C) is r or more: an effect (Effect_FindFree; none
// free counts as noticed) of kind 6 at the member's words +0x2E / +0x30, the
// two bytes 0x660B24 of the member id +0x89 at +0xC / +0x10, its index in
// ObjTrio's first +0xB. A slot past the party counts when r <= 50. All five:
// 0x904AE4 = 1; none: 2; else 0.
extern "C" void __cdecl Encounter_RollInitiative(void) {
    unsigned char count = 0;
    Obj(0)[0xB] = 0xFF;
    for (int i = 0; i < 5; ++i) {
        unsigned char r;
        do r = static_cast<unsigned char>(inventory_ops::g.rand() & 0x7F);
        while (r > 0x64);
        unsigned char* const obj = Obj(static_cast<u32>(i));
        if (i >= static_cast<int>(Field_MemberCount)) {
            if (r <= 0x32) ++count;
            continue;
        }
        const u32 record = Character_IndexById[Cond_Flags[0xd2 + i]];
        if (move_script::At(::bof3::Img(CharacterRecords_addr) + record * at::kRecordSize)[0x38] < r) continue;
        const unsigned char e = inventory_ops::g.effect_free();
        if (e != 0xFF) {
            unsigned char* const fx = rv::Bytes(Effect_Objects) + (static_cast<u32>(e) << 7);
            fx[0] = 1;
            fx[5] = 6;
            fx[6] = 1;
            rv::As<records::EffectObject>(fx)->screen_x = static_cast<std::uint16_t>(rv::As<records::PartyMember>(obj)->screen_x);
            rv::As<records::EffectObject>(fx)->screen_y = static_cast<std::uint16_t>(rv::As<records::PartyMember>(obj)->screen_y);
            const u32 id = obj[0x89];
            Obj(0)[0xB] = e;
            rv::As<records::EffectObject>(fx)->step_x = move_script::At(::bof3::Img(at::kAlertOffsets.v))[2 * id];
            rv::As<records::EffectObject>(fx)->step_z = move_script::At(::bof3::Img(at::kAlertOffsets.v))[2 * id + 1];
        }
        ++count;
    }
    BattlePhases_Initiative = count == 5 ? 1 : count != 0 ? 0 : 2;
}

// original 0x532660 (PSX 0x801BFDC8): each member's +0xB the way it turns
// (Sprite_TurnSense) to the direction 0x660B3C gives the battle's facing;
// the count and the facing read per member, the sprite after the call.
extern "C" void __cdecl Encounter_PartyTurnSense(void) {
    for (u32 i = 0; i < Field_MemberCount; ++i) {
        Sprite_Current = Obj(i);
        const unsigned char s = inventory_ops::g.turn_sense(move_script::At(::bof3::Img(at::kFaceTable.v))[ByteInventoryOps(::bof3::Img(at::kBattleFacing.v))]);
        Sprite_Current[0xB] = s;
    }
}

// original 0x5326B0 (PSX 0x801BFE54): one frame of the party's turn. While
// any member's +0xB is set (the count held for the scan): each member
// (Field_State and Sprite_Current it) - one whose record's state word has
// bit 14 is given its animation +0x4B through 0x5891C0 and +0xB 0, with no
// script tick; else, facing the direction already, +0xB non-zero gives the
// animation of that direction and +0xB 0; not facing it, the direction
// steps by +0xB (& 7) and the new one's animation is set; then the script
// ticks. Answers 0. When none is set: bit 2 of 0x904AE5 answers 1 at once;
// else each member's 32 CLUT words (row +5) saved three CLUTs on,
// Sprite_ShadeFadeBegin, +0xB 0, shades +0x5D..+0x5F 0xC0, +0x5C 1; 1.
extern "C" unsigned char __cdecl Encounter_PartyTurn(void) {
    u32 n = Field_MemberCount;
    u32 i = 0;
    if (static_cast<i32>(n) > 0)
        while (i < n && Obj(i)[0xB] == 0) ++i;
    if (i < n) {
        for (u32 j = 0; static_cast<i32>(j) < static_cast<i32>(n); ++j, n = Field_MemberCount) {
            unsigned char* const obj = Obj(j);
            Field_State = obj;
            Sprite_Current = obj;
            const u32 record = obj[0x148];
            if (move_script::Word(move_script::At(::bof3::Img(CharacterRecords_addr) + record * at::kRecordSize + 0x10)) & 0x4000) {
                inventory_ops::g.set_anim_from(obj[0x4B], 0, move_script::At(::bof3::Img(at::kAnimBuffer.v)), 0xA00);
                Sprite_Current[0xB] = 0;
                continue;
            }
            const unsigned char target = move_script::At(::bof3::Img(at::kFaceTable.v))[ByteInventoryOps(::bof3::Img(at::kBattleFacing.v))];
            const unsigned char d = obj[8], turn = obj[0xB];
            if (d == target) {
                if (turn != 0) {
                    inventory_ops::g.set_anim_from(d, 0, move_script::At(::bof3::Img(at::kAnimBuffer.v)), 0xA00);
                    Sprite_Current[0xB] = 0;
                }
            } else {
                obj[8] = static_cast<unsigned char>((d + turn) & 7);
                inventory_ops::g.set_anim_from(Sprite_Current[8], 0, move_script::At(::bof3::Img(at::kAnimBuffer.v)), 0xA00);
            }
            inventory_ops::g.script_tick();
        }
        return 0;
    }
    if (EventBattle_Flags & 4) return 1;
    for (u32 j = 0; static_cast<i32>(j) < static_cast<i32>(n); ++j, n = Field_MemberCount) {
        unsigned char* const obj = Obj(j);
        Field_State = obj;
        Sprite_Current = obj;
        for (u32 w = 0; w < 32; ++w) {
            const u32 o = (static_cast<u32>(obj[5]) << 5) + w;
            move_script::SetWord(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1ec0 + 2 * o), move_script::Word(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + 2 * o)));
        }
        inventory_ops::g.shade_begin();
        Sprite_Current[0xB] = 0;
        Sprite_Current[0x5D] = 0xC0;
        Sprite_Current[0x5E] = 0xC0;
        Sprite_Current[0x5F] = 0xC0;
        Sprite_Current[0x5C] = 1;
    }
    return 1;
}

// original 0x532860 (PSX 0x801C0114): one frame of the party's walk to its
// spots. Bit 2 of 0x904AE5: 1 at once. Else the centre is Field_Kind2X / Z
// plus the facing's cell step (0x660B1C); then each member (Field_State and
// Sprite_Current it): +0xB 0 - when Sprite_ShadeLower(8) is done, it is put
// at the spot of its place in the second list (0x904065, by Field_State's
// id +0x89; not found, the fourth spot), Field_State's +0x138 bit 0 cleared,
// +0x29 Draw_OtSlot, its height the ground's (MapView_GroundAt), +0x48 0,
// +0xB 1; +0xB 1 - when Sprite_ShadeRaise(8) is done, its 32 saved CLUT
// words back into Gfx_ClutStrip, Gfx_ClutStripDirty, bit 5 of +0 cleared,
// +0xB, the shades and +0x5C 0, then +0xB 2; then the script ticks. Answers
// whether every member's +0xB is 2 (1 for no members), the count the last
// one read.
extern "C" unsigned char __cdecl Encounter_PartyToPlaces(void) {
    if (EventBattle_Flags & 4) return 1;
    const u32 f = ByteInventoryOps(::bof3::Img(at::kBattleFacing.v));
    const auto* const step = reinterpret_cast<const signed char*>(move_script::At(::bof3::Img(at::kSideSteps.v) + 2 * f));
    BossSi_CentreX = AddInventoryOps(Shl(step[0], 16), Field_Kind2X);
    BossSi_CentreZ = AddInventoryOps(Shl(step[1], 16), Field_Kind2Z);
    u32 n = Field_MemberCount;
    if (n != 0) {
        for (u32 i = 0; i < n; ++i, n = Field_MemberCount) {
            unsigned char* const obj = Obj(i);
            Field_State = obj;
            Sprite_Current = obj;
            const unsigned char state = obj[0xB];
            if (state == 0) {
                if (inventory_ops::g.shade_lower(8)) {
                    unsigned char* const fs = Field_State;
                    const unsigned char id = fs[0x89];
                    u32 k = 0;
                    while (k < 3 && Cond_Flags[0xd5 + k] != id) ++k;
                    fs[0x138] = static_cast<unsigned char>(fs[0x138] & 0xFE);
                    Sprite_Current[0x29] = Draw_OtSlot;
                    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, move_script::Long(PosX(k)));
                    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, move_script::Long(PosZ(k)));
                    const long ground = inventory_ops::g.ground_at(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
                    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(ground));
                    Sprite_Current[0x48] = 0;
                    Sprite_Current[0xB] = 1;
                }
            } else if (state == 1) {
                if (inventory_ops::g.shade_raise(8)) {
                    unsigned char* const sc = Sprite_Current;
                    for (u32 w = 0; w < 32; ++w) {
                        const u32 o = (static_cast<u32>(sc[5]) << 5) + w;
                        move_script::SetWord(move_script::At(::bof3::Img(Gfx_ClutStrip_addr) + 0x1e00 + 2 * o), move_script::Word(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1ec0 + 2 * o)));
                    }
                    Gfx_ClutStripDirty = 1;
                    sc[0] = static_cast<unsigned char>(sc[0] & 0xDF);
                    Sprite_Current[0xB] = 0;
                    Sprite_Current[0x5D] = 0;
                    Sprite_Current[0x5E] = 0;
                    Sprite_Current[0x5F] = 0;
                    Sprite_Current[0x5C] = 0;
                    Sprite_Current[0xB] = 2;
                }
            }
            inventory_ops::g.script_tick();
        }
    }
    if (static_cast<i32>(n) <= 0) return 1;
    for (u32 i = 0; i < n; ++i)
        if (Obj(i)[0xB] != 2) return 0;
    return 1;
}

// original 0x532A70 (the PSX twin not paired): each member (Field_State and
// Sprite_Current it, the count read per member) at its placed spot - x
// through the held object, z through the sprite - on the ground
// (MapView_GroundAt), +0xB 0, and the animation the battle's facing (+0x1C
// when bit 6 of its record's byte +0x11 is set, the record Field_State's
// +0x148). The original first searches the first party list for the
// member's id +0x89 and never uses what it finds: a read, not reproduced.
extern "C" void __cdecl Encounter_PartyAtPlaces(void) {
    for (u32 i = 0; i < Field_MemberCount; ++i) {
        unsigned char* const obj = Obj(i);
        Field_State = obj;
        Sprite_Current = obj;
        rv::As<records::PartyMember>(obj)->x = move_script::Long(PosX(i));
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, move_script::Long(PosZ(i)));
        const long ground = inventory_ops::g.ground_at(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
        rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(ground));
        Sprite_Current[0xB] = 0;
        const u32 record = Field_State[0x148];
        if (move_script::At(::bof3::Img(CharacterRecords_addr) + record * at::kRecordSize + 0x11)[0] & 0x40)
            inventory_ops::g.set_animation(static_cast<unsigned char>(ByteInventoryOps(::bof3::Img(at::kBattleFacing.v)) + 0x1C));
        else
            inventory_ops::g.set_animation(ByteInventoryOps(::bof3::Img(at::kBattleFacing.v)));
    }
}

// original 0x532B60 (the PSX twin not paired): bit 4 of 0x904AE5 answers 1
// at once; else each member (Field_State and Sprite_Current it) whose +0xB is
// 0 gets +0xB 1 when Sprite_ScriptTickOnce answers non-zero. Answers whether
// every member's +0xB is 1 (1 for no members), the count the last one read.
// Like 0x532A70 it first searches the first party list for nothing.
extern "C" unsigned char __cdecl Encounter_PartyScriptOnce(void) {
    if (EventBattle_Flags & 0x10) return 1;
    u32 n = Field_MemberCount;
    if (n != 0) {
        for (u32 i = 0; i < n; ++i, n = Field_MemberCount) {
            unsigned char* const obj = Obj(i);
            Field_State = obj;
            Sprite_Current = obj;
            if (obj[0xB] == 0 && inventory_ops::g.script_once()) Sprite_Current[0xB] = 1;
        }
    }
    if (static_cast<i32>(n) <= 0) return 1;
    for (u32 i = 0; i < n; ++i)
        if (Obj(i)[0xB] != 1) return 0;
    return 1;
}

// ===========================================================================

void InventoryOps_Inject() {
    if (bof3::WantsShadow("inventory_ops")) inventory_ops::SelfTest();
    // DIVERGENCE DIV-0116 (D56): after the self-test, which compares the original's bounds.
    {
        static const std::uint8_t was = 0, is = 1;
        if (bof3::PatchBytes("EncounterOnScreenLowerBounds",
                             Low32(&g_on_screen_lower_bounds),
                             &was, &is, 1))
            bof3::Log("DIV-0116    the encounter's on-screen test refuses a corner left of or above the screen (D56)");
        g_on_screen_log = true;
    }
    BOF3_INJECT(Sprite_TurnSense);
    BOF3_INJECT(Encounter_RollInitiative);
    BOF3_INJECT(Encounter_PartyTurnSense);
    BOF3_INJECT(Encounter_PartyTurn);
    BOF3_INJECT(Encounter_PartyToPlaces);
    BOF3_INJECT(Encounter_PartyAtPlaces);
    BOF3_INJECT(Encounter_PartyScriptOnce);
    BOF3_INJECT(Sprite_ShadeLower);
}
