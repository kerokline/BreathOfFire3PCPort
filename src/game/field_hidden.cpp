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
using move_script::SetLong;
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
// A .data dispatch table's entry, read as the original reads it: in place,
// the index unchecked.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + 4 * index)))));
}
// Field_DirectionSteps' row for a direction (two longs, x then z), the
// direction unchecked: the table has 8 rows.
long StepX(unsigned d) { return Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8)); }
long StepZ(unsigned d) { return Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8 + 4)); }
long Add(long a, long b) { return static_cast<long>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }
// An effect object by its index: Effect_Objects + index * 0x80, unchecked.
unsigned char* Effect(unsigned index) { return rv::Bytes(Effect_Objects) + (index << 7); }

// The animation offset of a direction as the original computes it: (d - 1)
// / 2 as a signed division (cdq, sub, sar), so direction 0 gives 0.
unsigned char HalfTurn(unsigned char d) { return static_cast<unsigned char>((static_cast<int>(d) - 1) / 2); }

}  // namespace

#define FH_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Two party-member states (Member_States 0x65F960, Field_MemberFrame's)
// ===========================================================================

// original 0x51BA60, state 3: back to state 1 (Member_Control) unless bit 10
// of Field_ScriptFlags2 holds the member.
FH_EXPORT void __cdecl Member_ResumeUnlessHeld(void) {
    if ((Field_ScriptFlags2 & 0x400) == 0) rv::Current<records::PartyMember>()->state = 1;
}

// original 0x51BBD0, state 7, by its sub-state +2:
//   0 - once the leader's (ObjTrio's) +0x137 is not 8: +1 = 1, +2 = 0;
//   1 - an effect object from Effect_FindFree into +0xB; if there is none,
//       +2 = 0; else the object gets +0 = 1, kind +5 = 6, +6 = 1, +0xC = 0,
//       +0x10 = MoveScript_EffectArg[Field_State +0x89] (as a byte), +0x2E /
//       +0x30 the member's own words there, and +2 = 2;
//   2 - once that object's +0 is 0, +2 = 0;
// and Sprite_ScriptTick in every case (the original's tail).
//
// As the original has it: the sub-state and the effect index are not
// checked; in sub-state 1 the index is re-read from +0xB for every store.
FH_EXPORT void __cdecl Member_EffectState(void) {
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    switch (s->step) {
    case 0:
        if (rv::Bytes(ObjTrio)[0x137] != 8) {
            s->state = 1;
            rv::Current<records::PartyMember>()->step = 0;
        }
        break;
    case 1: {
        const unsigned char i = Al(g.effect_free());
        Sprite_Current[0xB] = i;
        records::PartyMember* const c = rv::Current<records::PartyMember>();
        if (rv::Bytes(c)[0xB] == 0xFF) {
            c->step = 0;
            break;
        }
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->flags = 1;
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->kind = 6;
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->param = 1;
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->step_x = 0;
        const unsigned arg = static_cast<unsigned char>(MoveScript_EffectArg[rv::As<records::PartyMember>(Field_State)->char_id]);
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->step_z = static_cast<std::int32_t>(arg);
        rv::Store<unsigned>(rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->screen_x, static_cast<std::uint16_t>(c->screen_x));
        rv::Store<unsigned>(rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->screen_y, static_cast<std::uint16_t>(c->screen_y));
        c->step = 2;
        break;
    }
    case 2:
        if (rv::As<records::EffectObject>(Effect(rv::Bytes(s)[0xB]))->flags == 0) s->step = 0;
        break;
    default:
        break;
    }
    g.script_tick();
}

// ===========================================================================
// The party actions (0x6609D0 by party set -> a forms table by u16 +0x2C ->
// a states table by +2)
// ===========================================================================

// original 0x51DA30: entry 2 of the party sets' state tables - 18 .data
// dwords hold it, PartyAction0/1/2/5_Form0States among them - by +0xB:
//   0 - Sprite_ScriptTickOnce; when it answers non-zero, the end;
//   1 - Sprite_ScriptTickOnce; when it answers non-zero,
//       Sprite_SetAnimation(+8) and +0xB = 2;
//   2.. - Sprite_ScriptTick; unless Field_Request is set, the end.
// The end: +0x2B = 0 and Field_State +0x137 = 0.
FH_EXPORT void __cdecl PartyAction_Finish(void) {
    const unsigned char b = Sprite_Current[0xB];
    if (b == 0) {
        if (Al(g.script_tick_once()) == 0) return;
    } else if (b == 1) {
        if (Al(g.script_tick_once()) == 0) return;
        g.set_animation(rv::Current<records::PartyMember>()->direction);
        Sprite_Current[0xB] = 2;
        return;
    } else {
        g.script_tick();
        if (Field_Request != 0) return;
    }
    Sprite_Current[0x2B] = 0;
    Field_State[0x137] = 0;
}

// original 0x51F1B0: party set 5's action - the entry of PartyAction5_Forms
// 0x65FC18 that the u16 +0x2C picks (0x51E910, PartyAction5_Form1, PartyAction_NoAction), jumped
// to, the index unchecked.
FH_EXPORT void __cdecl PartyAction5_ByForm(void) { Entry(::bof3::Img(PartyAction5_Forms_addr), rv::Current<records::PartyMember>()->column)(); }

// original 0x51E910: party set 5's form 0 - the entry of
// PartyAction5_Form0States 0x65FBC0 that +2 picks (0x51E930, 0x51EAF0,
// 0x51DA30), jumped to, the index unchecked.
FH_EXPORT void __cdecl PartyAction5_Form0(void) { Entry(::bof3::Img(PartyAction5_Form0States_addr), rv::Current<records::PartyMember>()->step)(); }

namespace {
// PartyAction5_Form0Begin's side probe: the point one step in direction `d`
// (3 or 5) from the sprite. When the slope there (MapView_SlopeAt, the
// scratch flag set and more than 0x40 as a short) is steep and the ground
// there (MapView_GroundAt) is above the sprite's +0x3E, +0x2B = 0.
void ProbeSide(unsigned d) {
    const records::PartyMember* s = rv::Current<records::PartyMember>();
    const long x = Add(s->x, StepX(d));
    const long z = Add(s->z, StepZ(d));
    const long slope = g.slope_at(x, z, d);
    if (at::kScratch == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = g.ground_at(x, z);
    s = rv::Current<records::PartyMember>();
    if (static_cast<short>(s->height_whole) < static_cast<short>(ground)) Sprite_Current[0x2B] = 0;
}
}  // namespace

// original 0x51E930, form 0's state 0. An even direction +8 is turned one
// eighth back (-1, & 7); when 0x51C390 finds nothing ahead that way (no
// object two steps on, no 0xF2 cell), two on (+2), and when it finds nothing
// there either, back to the first turn (-2). Then the slope one
// step ahead in +8 (MapView_SlopeAt with the whole direction byte): steep
// (the scratch flag and more than 0x40 as a short) - Sprite_EnsureAnimation(
// (+8 - 1) / 2 + 0x46) and +2 one on; otherwise +0x2B = 1, the side probes
// in directions 3 and 5 (which clear it again), Sound_PlayEffect(u16 +0x2C +
// 0x100), Sprite_EnsureAnimation((+8 - 1) / 2 + 0x42) and +0xA = 5. Then +0xB
// = 0 and +2 one on - so a steep slope moves +2 by two, to PartyAction_Finish.
//
// As the original has it: an odd +8 is kept and not masked, and the steps
// table is read with it unchecked; Sprite_Current is re-read after each call.
FH_EXPORT void __cdecl PartyAction5_Form0Begin(void) {
    const records::PartyMember* s = rv::Current<records::PartyMember>();
    if ((s->direction & 1) == 0) {
        rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>((s->direction - 1) & 7);
        if (Al(g.target_ahead()) == 0) {
            rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>((rv::Current<records::PartyMember>()->direction + 2) & 7);
            if (Al(g.target_ahead()) == 0) rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>((rv::Current<records::PartyMember>()->direction - 2) & 7);
        }
        s = rv::Current<records::PartyMember>();
    }
    const unsigned d = s->direction;
    const long x = Add(s->x, StepX(d));
    const long z = Add(s->z, StepZ(d));
    const long slope = g.slope_at(x, z, d);
    if (at::kScratch != 0 && static_cast<short>(slope) > 0x40) {
        g.ensure_animation(static_cast<unsigned char>(HalfTurn(rv::Current<records::PartyMember>()->direction) + 0x46));
        ++rv::Current<records::PartyMember>()->step;
    } else {
        Sprite_Current[0x2B] = 1;
        ProbeSide(3);
        ProbeSide(5);
        g.play_effect(static_cast<std::uint16_t>(Word(Sprite_Current + 0x2C) + 0x100));
        g.ensure_animation(static_cast<unsigned char>(HalfTurn(rv::Current<records::PartyMember>()->direction) + 0x42));
        Sprite_Current[0xA] = 5;
    }
    Sprite_Current[0xB] = 0;
    ++rv::Current<records::PartyMember>()->step;
}

// original 0x51EAF0, form 0's state 1. +0xA counted down; at 0: the point two
// steps ahead in +8 (unchecked). The object Sprite_ObjectAt(point, 0) finds
// there gets bit 0 of its +0x80 (0..0x1D Sprite_Objects, above that
// Sprite_ObjectsExtra, the index a signed byte); then Field_CellPickup on
// the point's cell, and when that finds nothing, on the cell one on in x
// (only when the point's x has a fraction), then one on in z (when z has
// one); then +2 one on. Sprite_ScriptTickOnce every time.
//
// As the original has it: the cells are the high words of the point, and
// the original passes them as dwords whose upper halves are its own
// uninitialised stack; every callee reads only the low word (docs section 3).
FH_EXPORT void __cdecl PartyAction5_Form0Resolve(void) {
    --Sprite_Current[0xA];
    if (Sprite_Current[0xA] == 0) {
        const records::PartyMember* const s = rv::Current<records::PartyMember>();
        const unsigned d = s->direction;
        const std::uint32_t x = static_cast<std::uint32_t>(s->x) + 2u * static_cast<std::uint32_t>(StepX(d));
        const std::uint32_t z = static_cast<std::uint32_t>(s->z) + 2u * static_cast<std::uint32_t>(StepZ(d));
        const unsigned char found = Al(g.object_at(static_cast<long>(x), static_cast<long>(z), 0));
        if (found != 0xFF) {
            const int i = static_cast<signed char>(found);
            if (i < 0x1E)
                At(::bof3::Img(Sprite_Objects_addr) + 0x80 + static_cast<std::uint32_t>(i * static_cast<int>(at::kObjectStride)))[0] |= 1;
            else
                At(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x80 + static_cast<std::uint32_t>((i - 0x1E) * static_cast<int>(at::kObjectStride)))[0] |= 1;
        }
        const unsigned cx = x >> 16, cz = z >> 16;
        if (Al(g.cell_pickup(cx, cz)) == 0) {
            bool done = false;
            if ((x & 0xFFFF) != 0) done = Al(g.cell_pickup(cx + 1, cz)) != 0;
            if (!done && (z & 0xFFFF) != 0) g.cell_pickup(cx, cz + 1);
        }
        ++rv::Current<records::PartyMember>()->step;
    }
    g.script_tick_once();
}

// original 0x51EBD0: what lies in the map cell (x, z) - AreaMap_ByteAt:
//   0xF2 - with an effect object free (Effect_FindFree): 0x524870(0, x, z);
//          three Rand draws in sixteen (& 0xF of 13, 14, 15) find zenny, 2 (5
//          on 15), ten times that when Field_InputFlags has bit 1 or 2 and a
//          second Rand & 3 is 0: 0x5307C0(the amount), 0x524870(1, x, z);
//          then +0xB = 1. With no object free, none of that.
//   0xF8 - 0x524870(0, x, z); the name of item 0x56 of category 0
//          (Item_NamePtr) copied, 16 bytes, into Text_Records; Inventory_Add(0,
//          0x56, 1): taken, Sound_PlayEffect(0x106) and Msg_OpenSystem(2),
//          else Msg_OpenSystem(3); Field_Request = 2, +0xB = 1.
// Both clear the cell (0x5728D0) and answer 1; anything else answers 0.
//
// As the original has it: x and z are passed on as the dwords it was given
// (every callee reads their low words); the original also keeps the amount
// in the low byte of its own z argument's slot, which no caller reads again;
// Inventory_Add is pushed a fourth dword, 0, which it does not read.
FH_EXPORT unsigned char __cdecl Field_CellPickup(unsigned x, unsigned z) {
    const unsigned char cell = Al(g.byte_at(x, z));
    if (cell == 0xF2) {
        if (Al(g.effect_free()) != 0xFF) {
            g.spawn_at_cell(0, x, z);
            const unsigned roll = Al(static_cast<std::uint32_t>(g.rand())) & 0xF;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (Al(static_cast<std::uint32_t>(g.rand())) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 10);
                g.found_zenny(amount);
                g.spawn_at_cell(1, x, z);
            }
            Sprite_Current[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        g.spawn_at_cell(0, x, z);
        const unsigned char* const name = g.item_name(0, 0x56);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (Al(g.inventory_add(0, 0x56, 1)) != 0) {
            g.play_effect(0x106);
            g.msg_open(2);
        } else {
            g.msg_open(3);
        }
        Field_Request = 2;
        Sprite_Current[0xB] = 1;
    } else {
        return 0;
    }
    g.clear_cell(x, z);
    return 1;
}

// ===========================================================================
// A pose from a set of frames
// ===========================================================================

// ===========================================================================
// The event op Ex
// ===========================================================================

// ===========================================================================
// The encounter placement's driver
// ===========================================================================

// ===========================================================================

void FieldHidden_Inject() {
    if (bof3::WantsShadow("field_hidden")) field_hidden::SelfTest();
    BOF3_INJECT(Member_ResumeUnlessHeld);
    BOF3_INJECT(Member_EffectState);
    BOF3_INJECT(PartyAction_Finish);
    BOF3_INJECT(PartyAction5_Form0);
    BOF3_INJECT(PartyAction5_Form0Begin);
    BOF3_INJECT(PartyAction5_Form0Resolve);
    BOF3_INJECT(Field_CellPickup);
    BOF3_INJECT(PartyAction5_ByForm);
}
