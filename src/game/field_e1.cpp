// Round twelve group FE1: the field engine's resident code 0x52D080..0x533BA0,
// taken with the scenario harness in field mode (scenario_harness.h, docs/
// scenario_harness.md section 7). docs/field_e1.md has each function, every
// caller, the tables and the fuzz.
//
//   FieldPanel_DrawHeader        0x52D080  two panel sprites (0, 1) at (x + 8, y) and (x + 0x108, y)
//   FieldPanel_DrawKindIcon      0x52D0C0  panel sprite 2 at (x, y), its CLUT and UV by a kind byte
//   FieldPanel_DrawKindRow       0x52D140  a kind's row: three sprites, its name, count and points
//   FieldPanel_DrawTotal         0x52D320  the total of the points, its rank (0x9045F4) and the rank's sprites
//   FieldPanel_DrawMessage       0x52D560  sprite 0xC and a script-pool message at (x, y)
//   FieldPanel_DrawShade         0x52D5C0  a half-transparent tile over the 320 x 240 screen
//   FieldPanel_DrawBox3          0x52D610  a window, its sprite frame and three script-pool lines
//   FieldPanel_DrawBox2          0x52D750  a wider window and two lines
//   Inventory_Holds38To4DAt99    0x52D880  al: 22 or more slots of item 0x38..0x4D held at 99
//   FieldPanel_DrawBlink         0x52D8C0  sprite 0x47 every other 8 frames while 0x939A28
//   Field_PathClear              0x52EC20  al: the way from the leader to (x, z) walkable, x then z
//   Field_FormActionState        0x52F4F0  leader state 6: the party set's handler (0x660A44) until done
//   PartyAction_ScriptEnd        0x52F5C0  a party action's step: the script once, done when it ends
//   Field_PassageTrigger         0x52F8F0  passage step 3: the object trigger on the passage's event
//   Field_JumpState              0x52F950  leader state 8: jmp Field_JumpSteps[+2]
//   Field_JumpBegin .. JumpIn3   0x52F970..0x52FB50  the jump's steps and sub-steps (FE2's helpers)
//   Field_ContentState           0x52FBB0  leader state 11: the content step (0x660A1C), the script tick
//   Field_ContentTake            0x52FBD0  a field object's content taken: zenny or an item, its flag
//   Field_ContentEnd             0x52FD90  the object after: closed back, removed or its cell marked
//   Field_AreaRunState           0x52FE90  leader state 12: area 104's or 121's run
//   Field_GiveZenny              0x5307C0  the zenny found: sound, "n" printed, message 5, Zenny_Add
//   Field_CellAroundLarge        0x531120  al: a cell of `code` around a sprite whose +0x34 / +0x38 are not whole
//   Field_CellAroundSide         0x531540  al: the two cells of `code` on one side, the facing turned to it
//   Field_GatewayExit            0x531820  al: the area's gateway exit, set pending
//   Party_PlaceAtSlots           0x532C10  the members to the slot positions, the camera to the leader
//   Party_ScriptTicks            0x532D10  each member's movement script, one tick
//   Party_PlacesByList           0x532D50  the members' places by the second list, then poses or a drop-in
//   Field_LeaderPlaceOffset      0x533690  the leader placed at (x, z) set off by the facing
//   Field_PendingJumpTurn        0x5338B0  pending jump 1: the member released by its facing (kind 2 / 3)
//   Field_PendingJumpKind4       0x5338F0  pending jump 2: released with kind 4
//   Field_PendingDrop            0x533900  pending jump 3: the next member dropped in from above
//   Field_PendingNext            0x5339A0  al: the next member of the pending jump made current
//   Field_PendingRelease         0x533A50  al: the member at 0x904EF2 released into a kind's state
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Sprite_Current and Field_State are read
// afresh at every use, as the original reads [0x937F88] / [0x905D98] (a callee
// may move either). Where the original would jump through a table entry that
// is not code, or walk its stack past a table, ours aborts with a message
// (docs/field_e1.md section 7).
#include "game/field_e1.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_e1_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_e1::at;
// move_script::SetWord: named in full below (source-layout B0, a clash on SetWord)
// move_script::Word: named in full below (source-layout B0, a clash on Word)

unsigned char& BFieldE1(std::uint32_t a) { return *move_script::At(a); }
std::uint16_t W(std::uint32_t a) { return move_script::Word(move_script::At(a)); }
void SetW(std::uint32_t a, unsigned v) { move_script::SetWord(move_script::At(a), v); }
std::int32_t L(std::uint32_t a) { return move_script::Long(move_script::At(a)); }

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Fs() { return Field_State; }
unsigned char* ObjectFieldE1(unsigned i) { return rv::Bytes(&Sprite_Objects[i]); }

// The record views (game/record_views.h): in the leader states Sprite_Current
// is an ObjTrio record, and Field_State always is.
short S16(std::uint32_t v) { return static_cast<short>(v); }

// `cdq / xor / sub`: the absolute value with the original's wrap (the most
// negative stays negative).
std::int32_t Abs32(std::int32_t v) {
    const auto u = static_cast<std::uint32_t>(v);
    const std::uint32_t s = static_cast<std::uint32_t>(v >> 31);
    return static_cast<std::int32_t>((u ^ s) - s);
}

// --- the callees nobody owns, and FE2's, by address -----------------------------
using Void0 = void (__cdecl*)();
using Byte0 = unsigned char (__cdecl*)();
void Fe2(std::uint32_t address) { SH_WORD(Void0, address)(); }
unsigned char Fe2Al(std::uint32_t address) { return SH_WORD(Byte0, address)(); }

// A .data dispatch table read in place: the index is a whole byte and
// unchecked, as the original's; where the entry is not code (past the run of
// code-pointer tables the entry lies in) the original jumps into data - ours
// aborts. While the fuzz runs, the entries are its recorders (outside .text).
using Handler = void (__cdecl*)();
Handler CodeAt(std::uint32_t table, unsigned index, const char* who) {
    const std::uint32_t cell = table + 4u * index;
    const auto entry = static_cast<std::uint32_t>(L(cell));
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry));
}

bool ScriptFlag8() { return ((BFieldE1(::bof3::Img(field_e1::at::kFlags2Lo)) | BFieldE1(::bof3::Img(field_e1::at::kFlagsLo))) & 8) != 0; }

}  // namespace

// --- the panel draws (callers 0x466BE0..0x4670C0, 0x528A90..0x52A420: Capcom's) ----

// The sprite frame of a panel window: its top row (0x25, 0x26 x `across`,
// 0x27, 0x28) at y, the two sides (0x468950) from y + 0x18 over `side`, the
// bottom row (0x30, 0x31 x `bottom`, 0x32) at y + `low`.
// --- the way, the leader's states ---------------------------------------------------

// original 0x52EC20: whether the way from the leader to (x, z) is open - from
// the leader's x (read again each step) in steps of half a unit toward x,
// |x - leader| >> 15 + 1 times, at z; then from the leader's z toward z at x.
// Each step: the ground (MapView_GroundAt) within 0x40 of the leader's height
// +0x3E, and not Field_WayBlocked(.., raised, the ground). al 1 open, 0 not.
// Called by Field_SwapGather and Field_SwapExchange (event_leader.cpp).
extern "C" unsigned char __cdecl Field_PathClear(long x, long z, unsigned raised) {
    const std::int32_t leader_z = ObjTrio[0].z;
    const std::int32_t along_x = Abs32(static_cast<std::int32_t>(x) - ObjTrio[0].x) >> 15;
    const std::int32_t along_z = Abs32(static_cast<std::int32_t>(z) - leader_z) >> 15;
    const short xh = S16(static_cast<std::uint32_t>(x) >> 16), zh = S16(static_cast<std::uint32_t>(z) >> 16);
    const short lxh = static_cast<short>(ObjTrio[0].x_whole), lzh = static_cast<short>(ObjTrio[0].z_whole);
    const int sx = xh > lxh ? 1 : xh == lxh ? 0 : -1;
    const int sz = zh > lzh ? 1 : zh == lzh ? 0 : -1;
    std::int32_t offset = 0;
    for (std::int32_t i = 0; i <= along_x; ++i) {
        const auto px = static_cast<long>(static_cast<std::uint32_t>(ObjTrio[0].x) + static_cast<std::uint32_t>(offset));
        const long ground = SH_CALL(MapView_GroundAt)(px, z);
        if (Abs32(static_cast<short>(ObjTrio[0].height_whole) - S16(static_cast<std::uint32_t>(ground))) > 0x40) return 0;
        if (SH_CALL(Field_WayBlocked)(px, z, raised, ground)) return 0;
        offset = static_cast<std::int32_t>(static_cast<std::uint32_t>(offset) + (static_cast<std::uint32_t>(sx) << 15));
    }
    offset = 0;
    for (std::int32_t i = 0; i <= along_z; ++i) {
        const auto pz = static_cast<long>(static_cast<std::uint32_t>(ObjTrio[0].z) + static_cast<std::uint32_t>(offset));
        const long ground = SH_CALL(MapView_GroundAt)(x, pz);
        if (Abs32(static_cast<short>(ObjTrio[0].height_whole) - S16(static_cast<std::uint32_t>(ground))) > 0x40) return 0;
        if (SH_CALL(Field_WayBlocked)(x, pz, raised, ground)) return 0;
        offset = static_cast<std::int32_t>(static_cast<std::uint32_t>(offset) + (static_cast<std::uint32_t>(sz) << 15));
    }
    return 1;
}

// original 0x52F4F0 (PSX 0x801B3C30): leader state 6 - with Field_InputHeld not 0, Field_State
// +0x137 = 0; else the handler of 0x660A44 for the loaded party set (0x90412C
// & 0x7F; 19 entries, unchecked). Then, once +0x137 is 0: the pose +8, +9 =
// 0, member 0 cleared, state 1, +0xB = 0, Field_LeaderStand (a tail jump).
extern "C" void __cdecl Field_FormActionState(void) {
    if (Field_InputHeld != 0) {
        Fs()[0x137] = 0;
    } else {
        const unsigned set = AreaW0b_PartySet & 0x7Fu;
        CodeAt(::bof3::Img(Field_FormActions_addr), set, "Field_FormActionState")();
    }
    if (Fs()[0x137] != 0) return;
    SH_CALL(Sprite_EnsureAnimation)(rv::As<records::PartyMember>(Sc())->direction);
    rv::As<records::PartyMember>(Sc())->step_frames = 0;
    SH_CALL(Member_ClearState)(0);
    rv::As<records::PartyMember>(Sc())->state = 1;
    Sc()[0xB] = 0;
    SH_CALL(Field_LeaderStand)();
}

// original 0x52F5C0 (PSX 0x801B3D4C): a party action's step (48 cells of the PartyAction run
// tables 0x65F998..): Sprite_ScriptTickOnce, and when it answers not 0,
// Field_State +0x137 = 0 (the action done).
extern "C" void __cdecl PartyAction_ScriptEnd(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0) Fs()[0x137] = 0;
}

// original 0x52F8F0 (PSX 0x801B41E8): passage step 3 (Field_PassageSteps[3]) - unless
// Field_Request is 2: an object record on the stack (0xA4 bytes, only +0x86
// and +0x88 written: Field_State +0x12A and the word +0x12C), handed to
// Field_ObjectTrigger unless +0x86 is 0xFF; step 2.
// As the original has it: the rest of the record is whatever the frame held
// (the trigger reads +0x89, the word's high byte, and hands the record on).
extern "C" void __cdecl Field_PassageTrigger(void) {
    if (Field_Request == 2) return;
    alignas(4) unsigned char object[0xA4];
    std::memset(object, 0, sizeof object);
    const unsigned char* const fs = Fs();
    object[0x86] = fs[0x12A];
    move_script::SetWord(object + 0x88, move_script::Word(fs + 0x12C));
    if (object[0x86] != 0xFF) SH_AT(void (__cdecl*)(unsigned char*), field_e1::at::kObjectTrigger.v)(object);
    rv::As<records::PartyMember>(Sc())->step = 2;
}

// original 0x52F950 (PSX 0x801B4258): leader state 8, the jump - jmp Field_JumpSteps[+2].
extern "C" void __cdecl Field_JumpState(void) { CodeAt(::bof3::Img(Field_JumpSteps_addr), rv::As<records::PartyMember>(Sc())->step, "Field_JumpState")(); }

// original 0x52F970 (PSX 0x801B429C): jump step 0 - 0x535FC0 (the first pose), step 2.
extern "C" void __cdecl Field_JumpBegin(void) {
    Fe2(field_e1::at::kJumpPose.v);
    rv::As<records::PartyMember>(Sc())->step = 2;
}

// original 0x52F980 (PSX 0x801B42C8): jump step 1 - jmp Field_JumpOutSteps[+3].
extern "C" void __cdecl Field_JumpOut(void) { CodeAt(::bof3::Img(Field_JumpOutSteps_addr), rv::As<records::PartyMember>(Sc())->state3, "Field_JumpOut")(); }

// original 0x52F9A0 (PSX 0x801B430C): jump-out 0 - 0x535FE0 (Field_JumpSetUp's caller); with
// +5 0 and neither script flag's bit 3, Field_JumpCamera; +3 = 1.
extern "C" void __cdecl Field_JumpOut0(void) {
    Fe2(field_e1::at::kJumpSetUp.v);
    if (rv::As<records::PartyMember>(Sc())->actor == 0 && !ScriptFlag8()) SH_CALL(Field_JumpCamera)();
    rv::As<records::PartyMember>(Sc())->state3 = 1;
}

// original 0x52F9E0 (PSX 0x801B4380) / 0x52FA00 / 0x52FA20: jump-out 1..3 - FE2's step, and
// when it answers al not 0, +3 one on.
extern "C" void __cdecl Field_JumpOut1(void) {
    if (Fe2Al(field_e1::at::kJumpOut1.v)) rv::As<records::PartyMember>(Sc())->state3 = 2;
}
extern "C" void __cdecl Field_JumpOut2(void) {
    if (Fe2Al(field_e1::at::kJumpOut2.v)) rv::As<records::PartyMember>(Sc())->state3 = 3;
}
extern "C" void __cdecl Field_JumpOut3(void) {
    if (Fe2Al(field_e1::at::kJumpOut3.v)) rv::As<records::PartyMember>(Sc())->state3 = 4;
}

// original 0x52FA40 (PSX 0x801B4434): jump-out 4 - 0x536170; when al: step 2, +3 = 0.
extern "C" void __cdecl Field_JumpOut4(void) {
    if (!Fe2Al(field_e1::at::kJumpOut4.v)) return;
    rv::As<records::PartyMember>(Sc())->step = 2;
    rv::As<records::PartyMember>(Sc())->state3 = 0;
}

// original 0x52FA60 (PSX 0x801B4480): jump step 2 - by Field_InputHeld: bit 12 0x5364D0, else
// bit 14 0x536550, else any bit of the button map word 0x903580 0x5365D0; then
// unless a script flag's bit 3, MapView_SetElevation(+0x3E) and
// Field_JumpCamera (a tail jump).
extern "C" void __cdecl Field_JumpAir(void) {
    const std::uint16_t held = Field_InputHeld;
    if (held & 0x1000) Fe2(field_e1::at::kJumpAirA.v);
    else if (held & 0x4000) Fe2(field_e1::at::kJumpAirB.v);
    else if (W(::bof3::Img(field_e1::at::kButtonMap)) & held) Fe2(field_e1::at::kJumpAirC.v);
    if (ScriptFlag8()) return;
    SH_CALL(MapView_SetElevation)(static_cast<short>(rv::As<records::PartyMember>(Sc())->height_whole));
    SH_CALL(Field_JumpCamera)();
}

// original 0x52FAC0 (PSX 0x801B4538): jump step 3 - jmp Field_JumpInSteps[+3].
extern "C" void __cdecl Field_JumpIn(void) { CodeAt(::bof3::Img(Field_JumpInSteps_addr), rv::As<records::PartyMember>(Sc())->state3, "Field_JumpIn")(); }

// original 0x52FAE0 (PSX 0x801B457C): jump-in 0 - 0x536290, +3 = 1.
extern "C" void __cdecl Field_JumpIn0(void) {
    Fe2(field_e1::at::kJumpIn0.v);
    rv::As<records::PartyMember>(Sc())->state3 = 1;
}

// original 0x52FAF0 (PSX 0x801B45A8): jump-in 1 - 0x5362D0; when al, +3 = 2.
extern "C" void __cdecl Field_JumpIn1(void) {
    if (Fe2Al(field_e1::at::kJumpIn1.v)) rv::As<records::PartyMember>(Sc())->state3 = 2;
}

// original 0x52FB10 (PSX 0x801B45E4): jump-in 2 - 0x5363C0; when al: with +5 0 and neither
// script flag's bit 3, Field_JumpCamera; +3 = 3.
extern "C" void __cdecl Field_JumpIn2(void) {
    if (!Fe2Al(field_e1::at::kJumpIn2.v)) return;
    if (rv::As<records::PartyMember>(Sc())->actor == 0 && !ScriptFlag8()) SH_CALL(Field_JumpCamera)();
    rv::As<records::PartyMember>(Sc())->state3 = 3;
}

// original 0x52FB50 (PSX 0x801B4668): jump-in 3 - a tail jump to 0x536440.
extern "C" void __cdecl Field_JumpIn3(void) { Fe2(field_e1::at::kJumpIn3.v); }

// original 0x52FBB0 (PSX 0x801B4714): leader state 11 - the step of Field_ContentSteps[+2],
// then Sprite_ScriptTick (a tail jump).
extern "C" void __cdecl Field_ContentState(void) {
    CodeAt(::bof3::Img(Field_ContentSteps_addr), rv::As<records::PartyMember>(Sc())->step, "Field_ContentState")();
    SH_CALL(Sprite_ScriptTick)();
}

// The field object Field_State +0x139 names (Sprite_Objects, the byte whole
// and unchecked), read again at every use as the original reads it.
namespace {
unsigned char* ContentObject() { return ObjectFieldE1(Fs()[0x139]); }
}  // namespace

// original 0x52FBD0: content step 0 - the object's flag (+5, in the bank
// 0x9040CC) set: system message 1. Else by +0x18: 0xFF the zenny +0x19 * 40
// (Field_GiveZenny), the flag (unless 0xFF) and the count 0x904140; an item
// (category +0x18, id +0x19): its 16-byte name to Text_Records and
// Inventory_Add(.., 1): taken - the flag (unless 0xFF), sound 0x106, message
// 2, the count; no room - message 3. Then, except after message 1, unless the
// object's +0xB bit 0 its animation 1 (Sprite_Current lent to it). Field_Request
// 2, +2 one on.
// As the original has it: the category and id reach Item_NamePtr and
// Inventory_Add as dwords whose upper bytes are the frame's (both read the low
// byte only).
extern "C" void __cdecl Field_ContentTake(void) {
    if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(field_e1::at::kObjectFlags)), ContentObject()[5])) {
        SH_CALL(Msg_OpenSystem)(1);
    } else {
        const unsigned char* const o = ContentObject();
        const unsigned char item = o[0x19];
        const unsigned char category = o[0x18];
        if (category == 0xFF) {
            SH_CALL(Field_GiveZenny)(item * 40u);
            const unsigned char flag = ContentObject()[5];
            if (flag != 0xFF) SH_CALL(Flags_Set)(move_script::At(::bof3::Img(field_e1::at::kObjectFlags)), flag);
            FieldE1_ContentCount = static_cast<std::int32_t>(FieldE1_ContentCount) + 1;
        } else {
            const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
            std::memcpy(move_script::At(::bof3::Img(bof3::addr::Text_Records)), name, 16);
            if (SH_CALL(Inventory_Add)(category, item, 1)) {
                const unsigned char flag = ContentObject()[5];
                if (flag != 0xFF) SH_CALL(Flags_Set)(move_script::At(::bof3::Img(field_e1::at::kObjectFlags)), flag);
                SH_CALL(Sound_PlayEffect)(0x106);
                SH_CALL(Msg_OpenSystem)(2);
                FieldE1_ContentCount = static_cast<std::int32_t>(FieldE1_ContentCount) + 1;
            } else {
                SH_CALL(Msg_OpenSystem)(3);
            }
        }
        unsigned char* const object = ContentObject();
        if (!(object[0xB] & 1)) {
            unsigned char* const kept = Sprite_Current;
            Sprite_Current = object;
            SH_CALL(Sprite_SetAnimation)(1);
            Sprite_Current = kept;
        }
    }
    records::PartyMember* const sc = rv::As<records::PartyMember>(Sc());
    Field_Request = 2;
    sc->step = static_cast<unsigned char>(sc->step + 1);
}

// original 0x52FD90 (PSX 0x801B4A34): content step 1 - once Field_Request is 0: after system
// message 3 (the message word 0x7DEE48), the object back to animation 0
// unless its +0xB bit 0; else bit 0 removes the object (+0 = 0), or with its
// +0x34 and +0x38 fractions 0 its cell (+0x36, +0x3A) becomes 0x10
// (AreaMap_SetByte). Then with Field_InputFlags bit 5 state 0xC, else the pose
// +8 and state 1; step 0.
extern "C" void __cdecl Field_ContentEnd(void) {
    if (Field_Request != 0) return;
    if (W(::bof3::Img(field_e1::at::kMessageWord)) == 3) {
        unsigned char* const object = ContentObject();
        if (!(object[0xB] & 1)) {
            unsigned char* const kept = Sprite_Current;
            Sprite_Current = object;
            SH_CALL(Sprite_SetAnimation)(0);
            Sprite_Current = kept;
        }
    } else {
        records::SpriteObject* const object = rv::As<records::SpriteObject>(ContentObject());
        if (rv::Bytes(object)[0xB] & 1) {
            object->flags = 0;
        } else if (object->x_frac == 0 && object->z_frac == 0) {
            SH_CALL(AreaMap_SetByte)(object->x_whole, object->z_whole, 0x10);
        }
    }
    if (Field_InputFlags & 0x20) {
        rv::As<records::PartyMember>(Sc())->state = 0xC;
        rv::As<records::PartyMember>(Sc())->step = 0;
        return;
    }
    SH_CALL(Sprite_EnsureAnimation)(rv::As<records::PartyMember>(Sc())->direction);
    rv::As<records::PartyMember>(Sc())->state = 1;
    rv::As<records::PartyMember>(Sc())->step = 0;
}

// original 0x52FE90 (PSX 0x801B4BF4): leader state 12 - in area 0x68 Area104_LeaderRun, else
// Area121_LeaderRun (tail jumps).
extern "C" void __cdecl Field_AreaRunState(void) {
    if (Game_AreaNumber == 0x68) SH_CALL(Area104_LeaderRun)();
    else SH_CALL(Area121_LeaderRun)();
}

// original 0x5307C0 (PSX 0x801B6E50, the sibling's Field_GiveZenny): sound
// 0x106; the amount printed into Text_Records by Area08_MessageFormat; system
// message 5; Field_Request 2; Zenny_Add(amount, 0).
extern "C" void __cdecl Field_GiveZenny(unsigned amount) {
    SH_CALL(Sound_PlayEffect)(0x106);
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(move_script::At(::bof3::Img(bof3::addr::Text_Records))), reinterpret_cast<const char*>(move_script::At(::bof3::Img(field_e1::at::kZennyFormat))),
                         amount);
    SH_CALL(Msg_OpenSystem)(5);
    Field_Request = 2;
    SH_CALL(Zenny_Add)(amount, 0);
}

// --- the cells around a sprite ------------------------------------------------------

// --- the party's placements -----------------------------------------------------------

// --- the pending jump's members (Field_PendingJumps 1..3) ---------------------------

void FieldE1_Inject() {
    if (bof3::WantsShadow("field_e1")) field_e1::SelfTest();
    BOF3_INJECT(Field_PathClear);
    BOF3_INJECT(Field_FormActionState);
    BOF3_INJECT(PartyAction_ScriptEnd);
    BOF3_INJECT(Field_PassageTrigger);
    BOF3_INJECT(Field_JumpState);
    BOF3_INJECT(Field_JumpBegin);
    BOF3_INJECT(Field_JumpOut);
    BOF3_INJECT(Field_JumpOut0);
    BOF3_INJECT(Field_JumpOut1);
    BOF3_INJECT(Field_JumpOut2);
    BOF3_INJECT(Field_JumpOut3);
    BOF3_INJECT(Field_JumpOut4);
    BOF3_INJECT(Field_JumpAir);
    BOF3_INJECT(Field_JumpIn);
    BOF3_INJECT(Field_JumpIn0);
    BOF3_INJECT(Field_JumpIn1);
    BOF3_INJECT(Field_JumpIn2);
    BOF3_INJECT(Field_JumpIn3);
    BOF3_INJECT(Field_ContentState);
    BOF3_INJECT(Field_ContentTake);
    BOF3_INJECT(Field_ContentEnd);
    BOF3_INJECT(Field_AreaRunState);
    BOF3_INJECT(Field_GiveZenny);
}
