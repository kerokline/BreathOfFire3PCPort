// Group DE of the eighth round (docs/field_hidden.md): fifteen field-side
// functions, each read to its last instruction with capstone against
// bof3/BOF3.exe. Faithful: no divergence. The start-up fuzz is
// field_hidden_fuzz.cpp.
//
//   Member_ResumeUnlessHeld     0x51BA60..0x51BA73 (0x14)   Member_States entry 3
//   Member_EffectState          0x51BBD0..0x51BCE4 (0x115)  Member_States entry 7
//   PartyAction_Finish          0x51DA30..0x51DA95 (0x66)   entry 2 of every form-0 state table
//   PartyAction5_Form0          0x51E910..0x51E921 (0x12)   PartyAction5_Forms entry 0
//   PartyAction5_Form0Begin     0x51E930..0x51EAE0 (0x1B1)  PartyAction5_Form0States entry 0
//   PartyAction5_Form0Resolve   0x51EAF0..0x51EBCA (0xDB)   PartyAction5_Form0States entry 1
//   Field_CellPickup            0x51EBD0..0x51ECEE (0x11F)
//   PartyAction5_ByForm         0x51F1B0..0x51F1C2 (0x13)   entry 5 of 0x6609D0 (DC's 0x52FB60)
//   Sprite_PoseFromSet          0x589110..0x589154 (0x45)
//   Sprite_CopyFrames           0x589160..0x5891B8 (0x59)
//   Sprite_AnimFromSet          0x5891C0..0x5891EC (0x2D)
//   EventOp_Ex                  0x5898D0..0x58996E (0x9F)
//   Encounter_Place             0x591F30..0x5920DA (0x1AB)
//   Encounter_PickRow           0x592570..0x59259A (0x2B)
//   Encounter_FillSlots         0x5925A0..0x5925FC (0x5D)
//
// Every call goes through field_hidden::g (field_hidden_callees.h), so that
// the start-up fuzz can stand recorders in for them.
#include "game/field_hidden.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/field_hidden_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_hidden {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Fn<Byte0>(::bof3::Code(bof3::addr::Sprite_ScriptTick)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Sprite_ScriptTickOnce)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Effect_FindFree)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_SetAnimation)),
    Fn<std::uint32_t (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_EnsureAnimation)),
    Fn<Byte0>(::bof3::Code(kTargetAhead.v)),
    Fn<long (__cdecl*)(long, long, unsigned long)>(::bof3::Code(bof3::addr::MapView_SlopeAt)),
    Fn<long (__cdecl*)(long, long)>(::bof3::Code(bof3::addr::MapView_GroundAt)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sound_PlayEffect)),
    Fn<std::uint32_t (__cdecl*)(long, long, unsigned)>(::bof3::Code(bof3::addr::Sprite_ObjectAt)),
    Fn<std::uint32_t (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Field_CellPickup)),
    Fn<std::uint32_t (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::AreaMap_ByteAt)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(kSpawnAtCell.v)),
    Rand,   // the CRT's rand, ours since crt_rest
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kFoundZenny.v)),
    Fn<unsigned char* (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Item_NamePtr)),
    Fn<std::uint32_t (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::Inventory_Add)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Msg_OpenSystem)),
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kClearCell.v)),
    Fn<unsigned char* (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_SetFrameQueueUpload)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_ScriptStart)),
    Fn<void (__cdecl*)(unsigned, unsigned char*, unsigned)>(::bof3::Code(bof3::addr::Sprite_CopyFrames)),
    Fn<long (__cdecl*)(long, long)>(::bof3::Code(bof3::addr::AreaMap_Elevation)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Encounter_PickRow)),
    Fn<std::uint32_t (__cdecl*)(const unsigned char*)>(::bof3::Code(bof3::addr::Encounter_FillSlots)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Encounter_PlaceParty)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Encounter_PlaceEnemies)),
    Fn<Byte0>(::bof3::Code(bof3::addr::Encounter_EnemiesReachable)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Gte_PushMatrix)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Gte_PopMatrix)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Encounter_AimCamera)),
    Fn<std::uint32_t (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Party_Count)),
    Fn<std::uint32_t (__cdecl*)(long, long, unsigned, unsigned)>(::bof3::Code(bof3::addr::Encounter_OnScreen)),
};
Callees g = kOriginals;

}  // namespace field_hidden

using namespace field_hidden;

namespace {

unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }
unsigned char Byte(std::uint32_t address) { return At(address)[0]; }
// An effect object by its index: Effect_Objects + index * 0x80, unchecked.
unsigned char* Effect(unsigned index) { return rv::Bytes(Effect_Objects) + (index << 7); }

// The record views (game/record_views.h): the member states and party actions
// run on an ObjTrio record, Field_State is one; the Sprite_* helpers run on
// any object whose first 0x80 bytes are a sprite object's.
// A pointer the original keeps in a dword, read afresh.
unsigned char* PtrIn(const unsigned char* at) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(field_hidden::Long(at))));
}

}  // namespace

#define FH_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Two party-member states (Member_States 0x65F960, Field_MemberFrame's)
// ===========================================================================

// ===========================================================================
// The party actions (0x6609D0 by party set -> a forms table by u16 +0x2C ->
// a states table by +2)
// ===========================================================================

// ===========================================================================
// A pose from a set of frames
// ===========================================================================

// original 0x589160: Sprite_SetFrameQueueUpload(frame), then (size & 0xFFFF)
// bytes from the sprite's +0x50 copied to buffer + its +5 * that size, one by
// one, and +0x50 pointed at the copy.
//
// As the original has it: Sprite_Current and its +0x50 are re-read for every
// byte, so a copy over either is seen; +5 is unchecked.
FH_EXPORT void __cdecl Sprite_CopyFrames(unsigned frame, unsigned char* buffer, unsigned size) {
    field_hidden::g.frame_upload(frame);
    const unsigned n = size & 0xFFFF;
    unsigned char* const to = buffer + static_cast<std::uint32_t>(Sprite_Current[5]) * n;
    for (unsigned i = 0; i < n; ++i) to[i] = PtrIn(Sprite_Current + 0x50)[i];
    rv::Current<records::SpriteObject>()->script = static_cast<std::int32_t>(Low32(to));
}

// original 0x589110: Sprite_CopyFrames(animation, set, size); when +0x4B is
// already the animation (its low byte), Sprite_ScriptStart(u16 +0x58 - 2),
// the script restarted where it was; else +0x4B = the animation and
// Sprite_ScriptStart(0).
FH_EXPORT void __cdecl Sprite_PoseFromSet(unsigned animation, unsigned char* set, unsigned size) {
    field_hidden::g.copy_frames(animation, set, size);
    records::SpriteObject* const s = rv::Current<records::SpriteObject>();
    if (s->animation == static_cast<unsigned char>(animation)) {
        field_hidden::g.script_start(static_cast<std::uint16_t>(field_hidden::Word(rv::Bytes(s) + 0x58) - 2));
        return;
    }
    s->animation = static_cast<unsigned char>(animation);
    field_hidden::g.script_start(0);
}

// original 0x5891C0 (PSX 0x8014D558): Sprite_CopyFrames(animation, buffer, size), +0x4B = the
// animation, Sprite_ScriptStart(position) - the dword as given.
FH_EXPORT void __cdecl Sprite_AnimFromSet(unsigned animation, unsigned position, unsigned char* buffer, unsigned size) {
    field_hidden::g.copy_frames(animation, buffer, size);
    rv::Current<records::SpriteObject>()->animation = static_cast<unsigned char>(animation);
    field_hidden::g.script_start(position);
}

// ===========================================================================
// The event op Ex
// ===========================================================================

// original 0x5898D0 (EventScript_Op's handler for Ex, 7 bytes): an effect
// object from Effect_FindFree, none if there is none free: +0 = 1, kind +5 =
// op[1], the position +0x34 / +0x38 as 16.16 - the cell words op[3] / op[5]
// and the halves 0x8000 when op[2] / op[4] are non-zero - its ground +0x3E =
// AreaMap_Elevation(+0x34, +0x38) and +0xB = op[6].
//
// As the original has it: the words are stored before the dwords are read
// back for the call; the op is read after each store.
FH_EXPORT void __cdecl EventOp_Ex(const unsigned char* op) {
    const unsigned char i = Al(field_hidden::g.effect_free());
    if (i == 0xFF) return;
    records::EffectObject* const e = rv::As<records::EffectObject>(Effect(i));
    e->flags = 1;
    e->kind = op[1];
    e->x_whole = op[3];
    e->x_frac = op[2] != 0 ? 0x8000 : 0;
    e->z_whole = op[5];
    e->z_frac = op[4] != 0 ? 0x8000 : 0;
    const long h = field_hidden::g.elevation(e->x, e->z);
    e->height_whole = static_cast<std::uint16_t>(h);
    rv::Bytes(e)[0xB] = op[6];
}

// ===========================================================================
// The encounter placement's driver
// ===========================================================================

// original 0x592570 (PSX 0x801C7A74): an encounter row by weight - (Rand & 0xF) + 1 against
// the running byte sum of the eight rows' weights (Encounter_Rows +8, 9 bytes
// a row): the first row whose sum reaches it, else 0.
//
// As the original has it: the sum is a byte and wraps.
FH_EXPORT unsigned char __cdecl Encounter_PickRow(void) {
    const unsigned char roll = static_cast<unsigned char>((Al(static_cast<std::uint32_t>(field_hidden::g.rand())) & 0xF) + 1);
    unsigned char sum = 0;
    for (unsigned char row = 0; row < 8; ++row) {
        sum = static_cast<unsigned char>(sum + Byte(::bof3::Img(at::kRows) + 8 + row * 9u));
        if (roll <= sum) return row;
    }
    return 0;
}

// original 0x5925A0 (PSX 0x801C7AE4): the eight enemy slots from a row. Slot k's kind +1 =
// row[k]; it is active (+0 = 1) when that is not 0xFF and (Rand & 0xF) + 1
// is at most Encounter_SlotChance[k] (read after the draw), else +0 = 0.
// Answers the slots made active.
FH_EXPORT unsigned char __cdecl Encounter_FillSlots(const unsigned char* row) {
    unsigned char n = 0;
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const slot = At(::bof3::Img(Encounter_Slots_addr) + k * at::kEnemyStride);
        const unsigned char kind = row[k];
        slot[1] = kind;
        if (kind == 0xFF) {
            slot[0] = 0;
            continue;
        }
        const int roll = (field_hidden::g.rand() & 0xF) + 1;
        if (roll > static_cast<int>(Byte(::bof3::Img(at::kSlotChance) + k))) {
            slot[0] = 0;
        } else {
            slot[0] = 1;
            ++n;
        }
    }
    return n;
}

// original 0x591F30 (PSX 0x801C70D8; one call site, Field_EncounterDue's 0x53013C, as
// (4, 0)): an encounter placed on the field map. Sprite_Current = the scratch sprite 0x6BDFF0; the facing byte
// 0x6BE070 = the first argument's low byte, or at 4 and above the facing one
// on (& 3). Unless the second argument's low byte is set, the fight's centre
// 0x903780 / 0x903784 = Field_Kind2X / Field_Kind2Z to the half cell. A row
// (Encounter_PickRow, 0x8C5580 + 9 * it, kept in 0x6BE088) fills the slots
// (Encounter_FillSlots, the count in 0x6BE084); with none, the answer is 0.
// Else 0x904AAC = the facing; the answer is 1 unless Encounter_PlaceParty,
// Encounter_PlaceEnemies or Encounter_EnemiesReachable (each only after the
// one before answered yes) says no. Then, between Gte_PushMatrix and
// Gte_PopMatrix: Encounter_AimCamera(facing); every member (m below
// Party_Count(1), asked again after each) must be on screen
// (Encounter_OnScreen(its 0x7E06E0 spot, wide when its id at 0x904065 is 2
// or 6, 0x20)) or the answer is 0; an active enemy slot not on screen (its
// kind's +0x86 / +0x87) goes inactive and 0x6BE071 one down; with 0x6BE071
// then 0, the answer is 0.
//
// As the original has it: the member's "wide" flag is written into the low
// byte of the first argument's own stack slot and that dword is pushed, so
// its upper bytes are the caller's; the member counter lives in the second
// argument's slot. Neither slot is read by a caller again.
FH_EXPORT unsigned char __cdecl Encounter_Place(unsigned facing, unsigned keep_centre) {
    unsigned char answer = 1;
    Sprite_Current = At(::bof3::Img(at::kScratchSprite));
    unsigned char f = static_cast<unsigned char>(facing);
    if (f >= 4) f = static_cast<unsigned char>((FieldHidden_Facing + 1) & 3);
    FieldHidden_Facing = f;
    if (static_cast<unsigned char>(keep_centre) == 0) {
        BossSi_CentreX = static_cast<std::int32_t>(static_cast<std::uint32_t>(Field_Kind2X) & 0xFFFF8000u);
        BossSi_CentreZ = static_cast<std::int32_t>(static_cast<std::uint32_t>(Field_Kind2Z) & 0xFFFF8000u);
    }
    const unsigned row = Al(field_hidden::g.pick_row());
    const unsigned char* const picked = At(::bof3::Img(Encounter_Rows_addr) + row * 9);
    FieldHidden_RowUsed = static_cast<std::int32_t>(Low32(picked));
    const unsigned char count = Al(field_hidden::g.fill_slots(picked));
    FieldHidden_EnemyCount = count;
    if (count == 0) return 0;
    At(::bof3::Img(at::kBattleFacing))[0] = FieldHidden_Facing;
    if (Al(field_hidden::g.place_party()) == 0 || Al(field_hidden::g.place_enemies()) == 0 || Al(field_hidden::g.reachable()) == 0) answer = 0;
    field_hidden::g.push_matrix();
    field_hidden::g.aim_camera(FieldHidden_Facing);
    for (unsigned char m = 0; m < Al(field_hidden::g.party_count(1)); ++m) {
        const long x = Long(At(::bof3::Img(at::kMemberPos) + m * 8u));
        const long z = Long(At(::bof3::Img(at::kMemberPos) + m * 8u + 4));
        const unsigned char id = Byte(::bof3::Img(at::kPartyList2) + m);
        const unsigned wide = (id == 2 || id == 6) ? 1u : 0u;
        if (Al(field_hidden::g.on_screen(x, z, (facing & 0xFFFFFF00u) | wide, 0x20)) == 0) {
            field_hidden::g.pop_matrix();
            return 0;
        }
    }
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const slot = At(::bof3::Img(Encounter_Slots_addr) + k * at::kEnemyStride);
        const unsigned char* const kind = At(::bof3::Img(at::kKinds) + slot[1] * at::kKindStride);
        if (slot[0] == 0) continue;
        if (Al(field_hidden::g.on_screen(Long(slot + 4), Long(slot + 8), kind[0x86], kind[0x87])) == 0) {
            slot[0] = 0;
            FieldHidden_Placed = static_cast<unsigned char>(FieldHidden_Placed - 1);
        }
    }
    const unsigned char placed = FieldHidden_Placed;
    field_hidden::g.pop_matrix();
    return placed != 0 ? answer : 0;
}

// ===========================================================================

void FieldHidden_Inject() {
    if (bof3::WantsShadow("field_hidden")) field_hidden::SelfTest();
    BOF3_INJECT(Sprite_PoseFromSet);
    BOF3_INJECT(Sprite_CopyFrames);
    BOF3_INJECT(Sprite_AnimFromSet);
    BOF3_INJECT(EventOp_Ex);
    BOF3_INJECT(Encounter_Place);
    BOF3_INJECT(Encounter_PickRow);
    BOF3_INJECT(Encounter_FillSlots);
}
