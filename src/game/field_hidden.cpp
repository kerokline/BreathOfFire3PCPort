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
    Fn<Byte0>(bof3::addr::Sprite_ScriptTick),
    Fn<Byte0>(bof3::addr::Sprite_ScriptTickOnce),
    Fn<Byte0>(bof3::addr::Effect_FindFree),
    Fn<void (__cdecl*)(unsigned)>(bof3::addr::Sprite_SetAnimation),
    Fn<std::uint32_t (__cdecl*)(unsigned)>(bof3::addr::Sprite_EnsureAnimation),
    Fn<Byte0>(kBlockedAhead),
    Fn<long (__cdecl*)(long, long, unsigned long)>(bof3::addr::MapView_SlopeAt),
    Fn<long (__cdecl*)(long, long)>(bof3::addr::MapView_GroundAt),
    Fn<void (__cdecl*)(unsigned)>(bof3::addr::Sound_PlayEffect),
    Fn<std::uint32_t (__cdecl*)(long, long, unsigned)>(bof3::addr::Sprite_ObjectAt),
    Fn<std::uint32_t (__cdecl*)(unsigned, unsigned)>(bof3::addr::Field_CellPickup),
    Fn<std::uint32_t (__cdecl*)(unsigned, unsigned)>(bof3::addr::AreaMap_ByteAt),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned)>(kSpawnAtCell),
    Fn<int (__cdecl*)()>(0x5B93D2),   // Rand (Capcom's CRT, not ours)
    Fn<void (__cdecl*)(unsigned)>(kFoundZenny),
    Fn<unsigned char* (__cdecl*)(unsigned, unsigned)>(bof3::addr::Item_NamePtr),
    Fn<std::uint32_t (__cdecl*)(unsigned, unsigned, unsigned)>(bof3::addr::Inventory_Add),
    Fn<void (__cdecl*)(unsigned)>(bof3::addr::Msg_OpenSystem),
    Fn<void (__cdecl*)(unsigned, unsigned)>(kClearCell),
    Fn<unsigned char* (__cdecl*)(unsigned)>(bof3::addr::Sprite_SetFrameQueueUpload),
    Fn<void (__cdecl*)(unsigned)>(bof3::addr::Sprite_ScriptStart),
    Fn<void (__cdecl*)(unsigned, unsigned char*, unsigned)>(bof3::addr::Sprite_CopyFrames),
    Fn<long (__cdecl*)(long, long)>(bof3::addr::AreaMap_Elevation),
    Fn<Byte0>(bof3::addr::Encounter_PickRow),
    Fn<std::uint32_t (__cdecl*)(const unsigned char*)>(bof3::addr::Encounter_FillSlots),
    Fn<Byte0>(bof3::addr::Encounter_PlaceParty),
    Fn<Byte0>(bof3::addr::Encounter_PlaceEnemies),
    Fn<Byte0>(bof3::addr::Encounter_EnemiesReachable),
    Fn<void (__cdecl*)()>(bof3::addr::Gte_PushMatrix),
    Fn<void (__cdecl*)()>(bof3::addr::Gte_PopMatrix),
    Fn<void (__cdecl*)(unsigned)>(bof3::addr::Encounter_AimCamera),
    Fn<std::uint32_t (__cdecl*)(unsigned)>(bof3::addr::Party_Count),
    Fn<std::uint32_t (__cdecl*)(long, long, unsigned, unsigned)>(bof3::addr::Encounter_OnScreen),
};
Callees g = kOriginals;

}  // namespace field_hidden

using namespace field_hidden;

namespace {

unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }
unsigned char Byte(std::uint32_t address) { return At(address)[0]; }
// A .data dispatch table's entry, read as the original reads it: in place,
// the index unchecked.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + 4 * index)))));
}
// Field_DirectionSteps' row for a direction (two longs, x then z), the
// direction unchecked: the table has 8 rows.
long StepX(unsigned d) { return Long(At(at::kDirSteps + d * 8)); }
long StepZ(unsigned d) { return Long(At(at::kDirSteps + d * 8 + 4)); }
long Add(long a, long b) { return static_cast<long>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }
// An effect object by its index: Effect_Objects + index * 0x80, unchecked.
unsigned char* Effect(unsigned index) { return Effect_Objects + (index << 7); }
// A pointer the original keeps in a dword, read afresh.
unsigned char* PtrIn(const unsigned char* at) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(at))));
}
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
    if ((Field_ScriptFlags2 & 0x400) == 0) Sprite_Current[1] = 1;
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
    unsigned char* const s = Sprite_Current;
    switch (s[2]) {
    case 0:
        if (Byte(at::kLeader137) != 8) {
            s[1] = 1;
            Sprite_Current[2] = 0;
        }
        break;
    case 1: {
        const unsigned char i = Al(g.effect_free());
        Sprite_Current[0xB] = i;
        unsigned char* const c = Sprite_Current;
        if (c[0xB] == 0xFF) {
            c[2] = 0;
            break;
        }
        Effect(c[0xB])[0] = 1;
        Effect(c[0xB])[5] = 6;
        Effect(c[0xB])[6] = 1;
        SetLong(Effect(c[0xB]) + 0xC, 0);
        const unsigned arg = static_cast<unsigned char>(MoveScript_EffectArg[Field_State[0x89]]);
        SetLong(Effect(c[0xB]) + 0x10, static_cast<std::int32_t>(arg));
        SetWord(Effect(c[0xB]) + 0x2E, Word(c + 0x2E));
        SetWord(Effect(c[0xB]) + 0x30, Word(c + 0x30));
        c[2] = 2;
        break;
    }
    case 2:
        if (Effect(s[0xB])[0] == 0) s[2] = 0;
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
        g.set_animation(Sprite_Current[8]);
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
// 0x65FC18 that the u16 +0x2C picks (0x51E910, 0x51ED10, 0x5226D0), jumped
// to, the index unchecked.
FH_EXPORT void __cdecl PartyAction5_ByForm(void) { Entry(at::kForms, Word(Sprite_Current + 0x2C))(); }

// original 0x51E910: party set 5's form 0 - the entry of
// PartyAction5_Form0States 0x65FBC0 that +2 picks (0x51E930, 0x51EAF0,
// 0x51DA30), jumped to, the index unchecked.
FH_EXPORT void __cdecl PartyAction5_Form0(void) { Entry(at::kForm0States, Sprite_Current[2])(); }

namespace {
// PartyAction5_Form0Begin's side probe: the point one step in direction `d`
// (3 or 5) from the sprite. When the slope there (MapView_SlopeAt, the
// scratch flag set and more than 0x40 as a short) is steep and the ground
// there (MapView_GroundAt) is above the sprite's +0x3E, +0x2B = 0.
void ProbeSide(unsigned d) {
    const unsigned char* s = Sprite_Current;
    const long x = Add(Long(s + 0x34), StepX(d));
    const long z = Add(Long(s + 0x38), StepZ(d));
    const long slope = g.slope_at(x, z, d);
    if (Byte(at::kScratch) == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = g.ground_at(x, z);
    s = Sprite_Current;
    if (static_cast<short>(Word(s + 0x3E)) < static_cast<short>(ground)) Sprite_Current[0x2B] = 0;
}
}  // namespace

// original 0x51E930, form 0's state 0. An even direction +8 is turned one
// eighth back (-1, & 7); when 0x51C390 finds that blocked, two on (+2), and
// when that is blocked too, back to the first turn (-2). Then the slope one
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
    const unsigned char* s = Sprite_Current;
    if ((s[8] & 1) == 0) {
        Sprite_Current[8] = static_cast<unsigned char>((s[8] - 1) & 7);
        if (Al(g.blocked_ahead()) == 0) {
            Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + 2) & 7);
            if (Al(g.blocked_ahead()) == 0) Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] - 2) & 7);
        }
        s = Sprite_Current;
    }
    const unsigned d = s[8];
    const long x = Add(Long(s + 0x34), StepX(d));
    const long z = Add(Long(s + 0x38), StepZ(d));
    const long slope = g.slope_at(x, z, d);
    if (Byte(at::kScratch) != 0 && static_cast<short>(slope) > 0x40) {
        g.ensure_animation(static_cast<unsigned char>(HalfTurn(Sprite_Current[8]) + 0x46));
        ++Sprite_Current[2];
    } else {
        Sprite_Current[0x2B] = 1;
        ProbeSide(3);
        ProbeSide(5);
        g.play_effect(static_cast<std::uint16_t>(Word(Sprite_Current + 0x2C) + 0x100));
        g.ensure_animation(static_cast<unsigned char>(HalfTurn(Sprite_Current[8]) + 0x42));
        Sprite_Current[0xA] = 5;
    }
    Sprite_Current[0xB] = 0;
    ++Sprite_Current[2];
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
        const unsigned char* const s = Sprite_Current;
        const unsigned d = s[8];
        const std::uint32_t x = static_cast<std::uint32_t>(Long(s + 0x34)) + 2u * static_cast<std::uint32_t>(StepX(d));
        const std::uint32_t z = static_cast<std::uint32_t>(Long(s + 0x38)) + 2u * static_cast<std::uint32_t>(StepZ(d));
        const unsigned char found = Al(g.object_at(static_cast<long>(x), static_cast<long>(z), 0));
        if (found != 0xFF) {
            const int i = static_cast<signed char>(found);
            if (i < 0x1E)
                At(at::kObjectFlag + static_cast<std::uint32_t>(i * static_cast<int>(at::kObjectStride)))[0] |= 1;
            else
                At(at::kExtraFlag + static_cast<std::uint32_t>((i - 0x1E) * static_cast<int>(at::kObjectStride)))[0] |= 1;
        }
        const unsigned cx = x >> 16, cz = z >> 16;
        if (Al(g.cell_pickup(cx, cz)) == 0) {
            bool done = false;
            if ((x & 0xFFFF) != 0) done = Al(g.cell_pickup(cx + 1, cz)) != 0;
            if (!done && (z & 0xFFFF) != 0) g.cell_pickup(cx, cz + 1);
        }
        ++Sprite_Current[2];
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
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(bof3::addr::Text_Records + k), Long(name + k));
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

// original 0x589160: Sprite_SetFrameQueueUpload(frame), then (size & 0xFFFF)
// bytes from the sprite's +0x50 copied to buffer + its +5 * that size, one by
// one, and +0x50 pointed at the copy.
//
// As the original has it: Sprite_Current and its +0x50 are re-read for every
// byte, so a copy over either is seen; +5 is unchecked.
FH_EXPORT void __cdecl Sprite_CopyFrames(unsigned frame, unsigned char* buffer, unsigned size) {
    g.frame_upload(frame);
    const unsigned n = size & 0xFFFF;
    unsigned char* const to = buffer + static_cast<std::uint32_t>(Sprite_Current[5]) * n;
    for (unsigned i = 0; i < n; ++i) to[i] = PtrIn(Sprite_Current + 0x50)[i];
    SetLong(Sprite_Current + 0x50, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(to)));
}

// original 0x589110: Sprite_CopyFrames(animation, set, size); when +0x4B is
// already the animation (its low byte), Sprite_ScriptStart(u16 +0x58 - 2),
// the script restarted where it was; else +0x4B = the animation and
// Sprite_ScriptStart(0).
FH_EXPORT void __cdecl Sprite_PoseFromSet(unsigned animation, unsigned char* set, unsigned size) {
    g.copy_frames(animation, set, size);
    unsigned char* const s = Sprite_Current;
    if (s[0x4B] == static_cast<unsigned char>(animation)) {
        g.script_start(static_cast<std::uint16_t>(Word(s + 0x58) - 2));
        return;
    }
    s[0x4B] = static_cast<unsigned char>(animation);
    g.script_start(0);
}

// original 0x5891C0: Sprite_CopyFrames(animation, buffer, size), +0x4B = the
// animation, Sprite_ScriptStart(position) - the dword as given.
FH_EXPORT void __cdecl Sprite_AnimFromSet(unsigned animation, unsigned position, unsigned char* buffer, unsigned size) {
    g.copy_frames(animation, buffer, size);
    Sprite_Current[0x4B] = static_cast<unsigned char>(animation);
    g.script_start(position);
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
    const unsigned char i = Al(g.effect_free());
    if (i == 0xFF) return;
    unsigned char* const e = Effect(i);
    e[0] = 1;
    e[5] = op[1];
    SetWord(e + 0x36, op[3]);
    SetWord(e + 0x34, op[2] != 0 ? 0x8000 : 0);
    SetWord(e + 0x3A, op[5]);
    SetWord(e + 0x38, op[4] != 0 ? 0x8000 : 0);
    const long h = g.elevation(Long(e + 0x34), Long(e + 0x38));
    SetWord(e + 0x3E, static_cast<std::uint16_t>(h));
    e[0xB] = op[6];
}

// ===========================================================================
// The encounter placement's driver
// ===========================================================================

// original 0x592570: an encounter row by weight - (Rand & 0xF) + 1 against
// the running byte sum of the eight rows' weights (Encounter_Rows +8, 9 bytes
// a row): the first row whose sum reaches it, else 0.
//
// As the original has it: the sum is a byte and wraps.
FH_EXPORT unsigned char __cdecl Encounter_PickRow(void) {
    const unsigned char roll = static_cast<unsigned char>((Al(static_cast<std::uint32_t>(g.rand())) & 0xF) + 1);
    unsigned char sum = 0;
    for (unsigned char row = 0; row < 8; ++row) {
        sum = static_cast<unsigned char>(sum + Byte(at::kRows + 8 + row * 9u));
        if (roll <= sum) return row;
    }
    return 0;
}

// original 0x5925A0: the eight enemy slots from a row. Slot k's kind +1 =
// row[k]; it is active (+0 = 1) when that is not 0xFF and (Rand & 0xF) + 1
// is at most Encounter_SlotChance[k] (read after the draw), else +0 = 0.
// Answers the slots made active.
FH_EXPORT unsigned char __cdecl Encounter_FillSlots(const unsigned char* row) {
    unsigned char n = 0;
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const slot = At(at::kEnemies + k * at::kEnemyStride);
        const unsigned char kind = row[k];
        slot[1] = kind;
        if (kind == 0xFF) {
            slot[0] = 0;
            continue;
        }
        const int roll = (g.rand() & 0xF) + 1;
        if (roll > static_cast<int>(Byte(at::kSlotChance + k))) {
            slot[0] = 0;
        } else {
            slot[0] = 1;
            ++n;
        }
    }
    return n;
}

// original 0x591F30 (one call site, Field_EncounterDue's 0x53013C, as
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
    Sprite_Current = At(at::kScratchSprite);
    unsigned char f = static_cast<unsigned char>(facing);
    if (f >= 4) f = static_cast<unsigned char>((Byte(at::kFacing) + 1) & 3);
    At(at::kFacing)[0] = f;
    if (static_cast<unsigned char>(keep_centre) == 0) {
        SetLong(At(at::kCentreX), static_cast<std::int32_t>(static_cast<std::uint32_t>(Field_Kind2X) & 0xFFFF8000u));
        SetLong(At(at::kCentreZ), static_cast<std::int32_t>(static_cast<std::uint32_t>(Field_Kind2Z) & 0xFFFF8000u));
    }
    const unsigned row = Al(g.pick_row());
    const unsigned char* const picked = At(at::kRows + row * 9);
    SetLong(At(at::kRowUsed), static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(picked)));
    const unsigned char count = Al(g.fill_slots(picked));
    At(at::kEnemyCount)[0] = count;
    if (count == 0) return 0;
    At(at::kBattleFacing)[0] = Byte(at::kFacing);
    if (Al(g.place_party()) == 0 || Al(g.place_enemies()) == 0 || Al(g.reachable()) == 0) answer = 0;
    g.push_matrix();
    g.aim_camera(Byte(at::kFacing));
    for (unsigned char m = 0; m < Al(g.party_count(1)); ++m) {
        const long x = Long(At(at::kMemberPos + m * 8u));
        const long z = Long(At(at::kMemberPos + m * 8u + 4));
        const unsigned char id = Byte(at::kPartyList2 + m);
        const unsigned wide = (id == 2 || id == 6) ? 1u : 0u;
        if (Al(g.on_screen(x, z, (facing & 0xFFFFFF00u) | wide, 0x20)) == 0) {
            g.pop_matrix();
            return 0;
        }
    }
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const slot = At(at::kEnemies + k * at::kEnemyStride);
        const unsigned char* const kind = At(at::kKinds + slot[1] * at::kKindStride);
        if (slot[0] == 0) continue;
        if (Al(g.on_screen(Long(slot + 4), Long(slot + 8), kind[0x86], kind[0x87])) == 0) {
            slot[0] = 0;
            At(at::kPlaced)[0] = static_cast<unsigned char>(Byte(at::kPlaced) - 1);
        }
    }
    const unsigned char placed = Byte(at::kPlaced);
    g.pop_matrix();
    return placed != 0 ? answer : 0;
}

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
    BOF3_INJECT(Sprite_PoseFromSet);
    BOF3_INJECT(Sprite_CopyFrames);
    BOF3_INJECT(Sprite_AnimFromSet);
    BOF3_INJECT(EventOp_Ex);
    BOF3_INJECT(Encounter_Place);
    BOF3_INJECT(Encounter_PickRow);
    BOF3_INJECT(Encounter_FillSlots);
}
