// Moved here by tools/move_funcs.py from field_hidden.cpp, inventory_ops.cpp (docs/source-layout-round.md).
#include "game/field/encounter.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_hidden_callees.h"
#include "game/inventory_ops.h"
#include "game/inventory_ops_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace field_hidden;

using namespace inventory_ops;

namespace field_hidden {

using move_script::At;

using move_script::Long;

}  // namespace field_hidden

namespace {

unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }

unsigned char Byte(std::uint32_t address) { return At(address)[0]; }

}  // namespace

#define FH_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
// move_script::Long is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::SetLong;

using i32 = std::int32_t;

using u32 = std::uint32_t;

unsigned char* PosX(u32 m) { return move_script::At(::bof3::Img(inventory_ops::at::kMemberPos.v) + 8 * m); }

unsigned char* PosZ(u32 m) { return move_script::At(::bof3::Img(inventory_ops::at::kMemberPos.v) + 8 * m + 4); }

unsigned char* Member(u32 m) { return move_script::At(::bof3::Img(inventory_ops::at::kMembers.v) + 4 * m); }   // +0 placed, +1 wide

unsigned char* Enemy(u32 k) { return move_script::At(::bof3::Img(Encounter_Slots_addr) + inventory_ops::at::kEnemyStride * k); }

unsigned char* Kind(u32 kind) { return move_script::At(::bof3::Img(inventory_ops::at::kKinds.v) + inventory_ops::at::kKindStride * kind); }

i32 CentreX() { return static_cast<std::int32_t>(BossSi_CentreX); }

i32 CentreZ() { return static_cast<std::int32_t>(BossSi_CentreZ); }

// A 16.16 coordinate's cell: `sar reg, 16`.
u32 Cell(i32 v) { return static_cast<u32>(v >> 16); }

// A shift of a value the original holds as a 32-bit register.
i32 Shl(i32 v, unsigned n) { return static_cast<i32>(static_cast<u32>(v) << n); }

i32 Neg(i32 v) { return static_cast<i32>(0u - static_cast<u32>(v)); }

i32 AddInventoryOps(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }

i32 Sub(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) - static_cast<u32>(b)); }

}  // namespace

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
    Sprite_Current = At(::bof3::Img(field_hidden::at::kScratchSprite));
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
    At(::bof3::Img(field_hidden::at::kBattleFacing))[0] = FieldHidden_Facing;
    if (Al(field_hidden::g.place_party()) == 0 || Al(field_hidden::g.place_enemies()) == 0 || Al(field_hidden::g.reachable()) == 0) answer = 0;
    field_hidden::g.push_matrix();
    field_hidden::g.aim_camera(FieldHidden_Facing);
    for (unsigned char m = 0; m < Al(field_hidden::g.party_count(1)); ++m) {
        const long x = Long(At(::bof3::Img(field_hidden::at::kMemberPos) + m * 8u));
        const long z = Long(At(::bof3::Img(field_hidden::at::kMemberPos) + m * 8u + 4));
        const unsigned char id = Byte(::bof3::Img(field_hidden::at::kPartyList2) + m);
        const unsigned wide = (id == 2 || id == 6) ? 1u : 0u;
        if (Al(field_hidden::g.on_screen(x, z, (facing & 0xFFFFFF00u) | wide, 0x20)) == 0) {
            field_hidden::g.pop_matrix();
            return 0;
        }
    }
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const slot = At(::bof3::Img(Encounter_Slots_addr) + k * field_hidden::at::kEnemyStride);
        const unsigned char* const kind = At(::bof3::Img(field_hidden::at::kKinds) + slot[1] * field_hidden::at::kKindStride);
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

// original 0x5920E0 (PSX 0x801C73A8): the party's placement. The formation
// byte 0x904060 read first; n = Party_Count(1)'s byte; the first three
// members' order 0, 1, 2, placed bytes 0 and wide bytes (their record, by the
// second list 0x904065, 2 or 6); a bubble sort of n passes puts the wide
// ones first - the order read from memory, so past three it runs on into the
// member bytes. If it moved any, the centre is retried at six offsets
// (0x669C80) until one fits (size 1) and is reached from the centre - whose
// cell is passed as (z, z), on the PSX too: a defect of Capcom's the port
// kept (docs/inventory_ops.md section 5) - and moves to the sixth when none
// does. Then each member of the order by Encounter_PlaceMember, the facing
// byte read afresh each time; 0 as soon as one fails.
extern "C" unsigned char __cdecl Encounter_PlaceParty(void) {
    const unsigned char formation = Cond_Flags[0xd0];
    const auto n = static_cast<unsigned char>(inventory_ops::g.party_count(1));
    unsigned char* const order = move_script::At(::bof3::Img(inventory_ops::at::kOrder.v));
    for (unsigned char i = 0; i < 3; ++i) {
        order[i] = i;
        Member(i)[0] = 0;
        const unsigned char record = Character_IndexById[Cond_Flags[0xd5 + i]];
        Member(i)[1] = (record == 2 || record == 6) ? 1 : 0;
    }
    bool swapped = false;
    if (n != 0) {
        int limit = n - 1;
        for (unsigned passes = n; passes != 0; --passes, --limit) {
            for (unsigned char j = 0; static_cast<int>(j) < limit; ++j) {
                const unsigned char a = order[j], b = order[j + 1];
                if (Member(a)[1] < Member(b)[1]) {
                    order[j] = b;
                    order[j + 1] = a;
                    swapped = true;
                }
            }
        }
        if (swapped) {
            i32 x = 0, z = 0;
            for (unsigned char k = 0; k < 6; ++k) {
                x = AddInventoryOps(move_script::Long(move_script::At(::bof3::Img(inventory_ops::at::kRecentre.v) + 8 * k)), CentreX());
                z = AddInventoryOps(move_script::Long(move_script::At(::bof3::Img(inventory_ops::at::kRecentre.v) + 8 * k + 4)), CentreZ());
                if (inventory_ops::g.cell_fits(x, z, 1) && inventory_ops::g.path_clear(Cell(CentreZ()), Cell(CentreZ()), Cell(x), Cell(z))) break;
            }
            BossSi_CentreX = x;
            BossSi_CentreZ = z;
        }
    }
    for (unsigned char i = 0; i < n; ++i)
        if (!inventory_ops::g.place_member(formation, order[i], static_cast<unsigned char>(n - 1), FieldHidden_Facing)) return 0;
    return 1;
}

// original 0x5922A0 (PSX 0x801C7650): member m's spot. The offset (x, z) from
// the table 0x6698B0 by ((last^2 + formation) * 3 + m), the four arguments'
// low bytes; the step (sx, sz) the offset's negated signs * 0x8000; both
// turned a quarter at a time until the facing (its byte, stepped + 1 & 3) is
// 2. One Rand bit picks which axis the first 8 retries walk; the other 8
// walk the other. Each try the spot at the centre plus the offset (the
// centre read afresh for each 8) plus the steps so far, stepped in memory,
// until Encounter_MemberClear and Encounter_MemberStands agree - then the
// member's placed byte is 1. Both are given the member argument whole, as the
// original pushes it. The original also uses three of its argument slots as
// counters, which its one caller does not read again.
extern "C" unsigned char __cdecl Encounter_PlaceMember(unsigned formation, unsigned member, unsigned last, unsigned facing) {
    const u32 f = formation & 0xFF, m = member & 0xFF, l = last & 0xFF;
    const u32 index = (l * l + f) * 3 + m;
    i32 dx = move_script::Long(move_script::At(::bof3::Img(inventory_ops::at::kPartyOffsets.v) + 8 * index)), dz = move_script::Long(move_script::At(::bof3::Img(inventory_ops::at::kPartyOffsets.v) + 8 * index + 4));
    const auto sign = [](i32 v) { return v > 0 ? 1 : v < 0 ? -1 : 0; };
    i32 sx = Shl(sign(Neg(dx)), 15), sz = Shl(sign(Neg(dz)), 15);
    for (auto c = static_cast<unsigned char>(facing); c != 2; c = static_cast<unsigned char>((c + 1) & 3)) {
        const i32 t = dx;
        dx = dz;
        dz = Neg(t);
        const i32 u = sx;
        sx = sz;
        sz = Neg(u);
    }
    const auto r = static_cast<unsigned char>(inventory_ops::g.rand() & 1);
    for (unsigned char pass = 0; pass < 2; ++pass) {
        SetLong(PosX(m), AddInventoryOps(CentreX(), dx));
        SetLong(PosZ(m), AddInventoryOps(CentreZ(), dz));
        for (unsigned char t = 0; t < 8; ++t) {
            if (inventory_ops::g.member_clear(member) && inventory_ops::g.member_stands(member)) {
                Member(m)[0] = 1;
                return 1;
            }
            if ((r ^ pass) != 0) SetLong(PosX(m), AddInventoryOps(move_script::Long(PosX(m)), sx));
            else SetLong(PosZ(m), AddInventoryOps(move_script::Long(PosZ(m)), sz));
        }
    }
    return 0;
}

// original 0x592400 (PSX 0x801C7880): whether member m's spot keeps apart
// from every other placed member of the first three (their wide bytes the
// sizes; k is compared with the argument's low byte) and is reached from the
// centre (Encounter_PathClear, cells). The member's spot is held for the
// separations; the centre and the spot are read again for the path.
extern "C" unsigned char __cdecl Encounter_MemberClear(unsigned member) {
    const u32 m = member & 0xFF;
    const unsigned char wide = Member(m)[1];
    const i32 x = move_script::Long(PosX(m)), z = move_script::Long(PosZ(m));
    for (unsigned char k = 0; k < 3; ++k) {
        if (k == static_cast<unsigned char>(member)) continue;
        if (Member(k)[0] == 0) continue;
        if (!inventory_ops::g.apart(x, z, move_script::Long(PosX(k)), move_script::Long(PosZ(k)), wide, Member(k)[1])) return 0;
    }
    return inventory_ops::g.path_clear(Cell(CentreX()), Cell(CentreZ()), Cell(move_script::Long(PosX(m))), Cell(move_script::Long(PosZ(m)))) != 0;
}

// original 0x5924E0 (PSX 0x801C79C0): member m's spot on the ground -
// Sprite_Current +0x3C its elevation << 16, the sprite read after the call -
// then whether it fits (Encounter_CellFits, the wide byte the size) and no
// object stands there (Sprite_ObjectAt 0xFF). The spot is read at each use.
extern "C" unsigned char __cdecl Encounter_MemberStands(unsigned member) {
    const u32 m = member & 0xFF;
    const unsigned wide = Member(m)[1];
    const long e = inventory_ops::g.elevation(move_script::Long(PosX(m)), move_script::Long(PosZ(m)));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->height, Shl(static_cast<std::int16_t>(e), 16));
    if (!inventory_ops::g.cell_fits(move_script::Long(PosX(m)), move_script::Long(PosZ(m)), wide)) return 0;
    return inventory_ops::g.object_at(move_script::Long(PosX(m)), move_script::Long(PosZ(m)), wide) == 0xFF;
}

// original 0x592570 (PSX 0x801C7A74): an encounter row by weight - (Rand & 0xF) + 1 against
// the running byte sum of the eight rows' weights (Encounter_Rows +8, 9 bytes
// a row): the first row whose sum reaches it, else 0.
//
// As the original has it: the sum is a byte and wraps.
FH_EXPORT unsigned char __cdecl Encounter_PickRow(void) {
    const unsigned char roll = static_cast<unsigned char>((Al(static_cast<std::uint32_t>(field_hidden::g.rand())) & 0xF) + 1);
    unsigned char sum = 0;
    for (unsigned char row = 0; row < 8; ++row) {
        sum = static_cast<unsigned char>(sum + Byte(::bof3::Img(field_hidden::at::kRows) + 8 + row * 9u));
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
        unsigned char* const slot = At(::bof3::Img(Encounter_Slots_addr) + k * field_hidden::at::kEnemyStride);
        const unsigned char kind = row[k];
        slot[1] = kind;
        if (kind == 0xFF) {
            slot[0] = 0;
            continue;
        }
        const int roll = (field_hidden::g.rand() & 0xF) + 1;
        if (roll > static_cast<int>(Byte(::bof3::Img(field_hidden::at::kSlotChance) + k))) {
            slot[0] = 0;
        } else {
            slot[0] = 1;
            ++n;
        }
    }
    return n;
}

// original 0x592600 (PSX 0x801C7BAC): the enemies' placement. For each of the
// eight active slots: the offset from 0x669B60 by (c - 1) * c / 2 + placed
// (c the chosen count 0x6BE084, placed the count so far - both read per
// slot; a 16-bit index), x + 5 cells, turned a quarter per facing step until
// the facing byte, stepped + 1 & 3, is 0; then up to 49 jitters (0x669AF8,
// half cells) round the centre (read per try), written into the slot, until
// the spot fits (the kind's size) and keeps apart from the slots before it
// (Encounter_EnemyClear, given the spot read back from the slot); placed + 1
// in memory. A slot that never fits is made inactive. Answers placed != 0.
extern "C" unsigned char __cdecl Encounter_PlaceEnemies(void) {
    FieldHidden_Placed = 0;
    for (std::uint16_t i = 0; static_cast<std::int16_t>(i) < 8; ++i) {
        unsigned char* const e = Enemy(i);
        if (e[0] == 0) continue;
        unsigned char* const kind = Kind(e[1]);
        const u32 c = FieldHidden_EnemyCount;
        const i32 triangle = static_cast<i32>((c - 1) * c) >> 1;   // lea eax, [ecx - 1]; imul eax, ecx; sar eax, 1
        const auto index = static_cast<std::int16_t>(AddInventoryOps(triangle, FieldHidden_Placed));
        i32 ox = AddInventoryOps(move_script::Long(move_script::At(::bof3::Img(inventory_ops::at::kEnemyOffsets.v) + 8 * index)), 0x50000);
        i32 oz = move_script::Long(move_script::At(::bof3::Img(inventory_ops::at::kEnemyOffsets.v) + 8 * index + 4));
        for (auto f = FieldHidden_Facing; f != 0; f = static_cast<unsigned char>((f + 1) & 3)) {
            const i32 t = ox;
            ox = oz;
            oz = Neg(t);
        }
        std::uint16_t t = 0;
        for (;;) {
            const auto* const j = reinterpret_cast<const signed char*>(move_script::At(::bof3::Img(inventory_ops::at::kJitter.v) + 2 * static_cast<std::int16_t>(t)));
            const i32 x = AddInventoryOps(AddInventoryOps(Shl(j[0], 15), CentreX()), ox);
            rv::Store<std::int32_t>(rv::As<records::EncounterSlot>(e)->x, x);
            const i32 z = AddInventoryOps(AddInventoryOps(Shl(j[1], 15), CentreZ()), oz);
            rv::Store<std::int32_t>(rv::As<records::EncounterSlot>(e)->z, z);
            if (inventory_ops::g.cell_fits(x, z, kind[0x86]) && inventory_ops::g.enemy_clear(i, rv::As<records::EncounterSlot>(e)->x, rv::As<records::EncounterSlot>(e)->z, kind[0x86])) {
                ++FieldHidden_Placed;
                break;
            }
            ++t;
            if (static_cast<std::int16_t>(t) >= 0x31) break;
        }
        if (t == 0x31) e[0] = 0;
    }
    return FieldHidden_Placed != 0;
}

// original 0x592760 (PSX 0x801C7DC4): each active enemy slot stays active if
// some member of Party_Count(1) - asked again after each member - reaches it
// (Encounter_PathClear from the member's cell to the enemy's, its words +6
// and +0xA), counting them in 0x6BE071; answers that count != 0.
extern "C" unsigned char __cdecl Encounter_EnemiesReachable(void) {
    FieldHidden_Placed = 0;
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const e = Enemy(k);
        if (e[0] == 0) continue;
        unsigned char reached = 0;
        if (static_cast<unsigned char>(inventory_ops::g.party_count(1)) != 0) {
            std::uint16_t m = 0;
            for (;;) {
                if (inventory_ops::g.path_clear(Cell(move_script::Long(PosX(static_cast<std::int16_t>(m)))), Cell(move_script::Long(PosZ(static_cast<std::int16_t>(m)))),
                                 move_script::Word(e + 6), move_script::Word(e + 0xA))) {
                    ++FieldHidden_Placed;
                    reached = 1;
                    break;
                }
                ++m;
                if (!(static_cast<std::int16_t>(m) < static_cast<std::int16_t>(static_cast<unsigned char>(inventory_ops::g.party_count(1))))) break;
            }
        }
        e[0] = reached;
    }
    return FieldHidden_Placed != 0;
}

// original 0x592800 (PSX 0x801C7ED8): whether a thing at (x, z) of size
// class `size` keeps apart from each active enemy of the first `count` (its
// byte) - each enemy's size its kind's byte +0x86.
extern "C" unsigned char __cdecl Encounter_EnemyClear(unsigned count, long x, long z, unsigned size) {
    const auto n = static_cast<unsigned char>(count);
    for (unsigned char k = 0; k < n; ++k) {
        unsigned char* const e = Enemy(k);
        if (e[0] == 0) continue;
        if (!inventory_ops::g.apart(x, z, rv::As<records::EncounterSlot>(e)->x, rv::As<records::EncounterSlot>(e)->z, size, Kind(e[1])[0x86])) return 0;
    }
    return 1;
}

// original 0x592890 (PSX 0x801C7FCC): the nibble of cell (x, z) of the map at
// 0x8C3D80 - x and z their low words, 0 outside AreaMap_Header's bytes 0 / 1
// (width, depth); cell i = width * z + x, a 16-bit sum; the high nibble of
// byte i / 2 for an even i, the low for an odd one. What the nibble means
// is read only by its callers: 0x592C30 wants a size class, 0x592E30 a
// passage class (docs/inventory_ops.md section 3).
extern "C" unsigned char __cdecl AreaMap_CellNibble(unsigned x, unsigned z) {
    const std::uint16_t w = AreaMap_Header[0], d = AreaMap_Header[1];
    if (static_cast<std::uint16_t>(x) >= w) return 0;
    if (static_cast<std::uint16_t>(z) >= d) return 0;
    const auto i = static_cast<std::uint16_t>(w * z + x);
    const unsigned char b = move_script::At(::bof3::Img(inventory_ops::at::kCellMap.v))[i >> 1];
    return (i & 1) ? static_cast<unsigned char>(b & 0xF) : static_cast<unsigned char>(b >> 4);
}

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

extern "C" unsigned char __cdecl Encounter_OnScreen(long x, long z, unsigned size, unsigned margin) {
    i32 a = Shl(static_cast<i32>((size & 0xFF) + 1), 14);
    i32 b = a;
    float sxy[2];
    for (unsigned char t = 0; t < 4; ++t) {
        const i32 pz = AddInventoryOps(b, z), px = AddInventoryOps(a, x);
        const i32 y = Shl(static_cast<std::int16_t>(inventory_ops::g.elevation(px, pz)), 16);
        inventory_ops::g.project(px, pz, y, reinterpret_cast<unsigned long*>(sxy));
        if (sxy[0] >= 320.0f) return 0;
        if (sxy[1] >= 240.0f) return 0;
        if (sxy[0] < 0.0f || sxy[1] < 0.0f) {
            if (g_on_screen_log)
                bof3::Log("D56         Encounter_OnScreen(%ld, %ld, size %u, margin %u) corner %u at (%.1f, %.1f): %s",
                          x, z, size & 0xFF, margin & 0xFF, static_cast<unsigned>(t), static_cast<double>(sxy[0]),
                          static_cast<double>(sxy[1]),
                          g_on_screen_lower_bounds ? "refused (DIV-0116)" : "passed, the port's bounds");
            if (g_on_screen_lower_bounds) return 0;
        }
        const i32 na = Neg(a);
        a = b;
        b = na;
    }
    const i32 pz = AddInventoryOps(b, z), px = AddInventoryOps(a, x);
    const i32 y = Shl(static_cast<std::int16_t>(inventory_ops::g.elevation(px, pz)), 16);
    inventory_ops::g.project(px, pz, y, reinterpret_cast<unsigned long*>(sxy));
    return static_cast<double>(sxy[1]) - static_cast<double>(margin & 0xFF) >= 8.0;
}

// original 0x5929D0 (PSX 0x801C81BC): the screen point of a ground point,
// Gte_RotTransPers of the vertex (x >> 9 - 0x4000, z >> 9 - 0x4000, -(y's
// high word / 2)), the two floats to `sxy`; the answer is Gte_RotTransPers's.
// The original lets the depth land in its own second argument slot, which no
// caller reads again.
extern "C" long __cdecl Encounter_Project(long x, long z, long y, unsigned long* sxy) {
    short v[3];
    v[0] = static_cast<short>(Sub(x >> 9, 0x4000));
    v[1] = static_cast<short>(Sub(z >> 9, 0x4000));
    const i32 yh = static_cast<std::int16_t>(static_cast<u32>(y) >> 16);
    v[2] = static_cast<short>(Neg(yh / 2));
    long depth;
    return inventory_ops::g.rot_trans_pers(v, sxy, &depth);
}

// original 0x592A30 (PSX 0x801C823C): the camera for the fight. The mean
// elevation of the active enemies and of the Party_Count(1) members (the
// count asked again after each), summed in 0x939860 and read back after each
// call, divided (signed; no fight has nobody) and its low five bits cleared;
// then Gte_RotMatrix(Camera_Angles) into a local MATRIX, the vector (0x4000 -
// the centre plus the facing's offset (0x669CB8) >> 9, x then z, the mean
// / 2) through it (Gte_ApplyMatrix) plus Camera_ShiftX / ShiftY / Distance +
// 0x1194 as the translation, the rotation made again over it, and both set
// (Gte_SetRotMatrix, Gte_SetTransMatrix). The centre and the mean are read
// after the first Gte_RotMatrix, the camera words after Gte_ApplyMatrix.
extern "C" void __cdecl Encounter_AimCamera(unsigned facing) {
    ModeRest_ElevationBase = 0;
    i32 n = 0;
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const e = Enemy(k);
        if (e[0] == 0) continue;
        const long h = inventory_ops::g.elevation(rv::As<records::EncounterSlot>(e)->x, rv::As<records::EncounterSlot>(e)->z);
        ModeRest_ElevationBase = AddInventoryOps(static_cast<std::int32_t>(ModeRest_ElevationBase), static_cast<std::int16_t>(h));
        ++n;
    }
    for (unsigned char m = 0; m < static_cast<unsigned char>(inventory_ops::g.party_count(1)); ++m) {
        const long h = inventory_ops::g.elevation(move_script::Long(PosX(m)), move_script::Long(PosZ(m)));
        ModeRest_ElevationBase = AddInventoryOps(static_cast<std::int32_t>(ModeRest_ElevationBase), static_cast<std::int16_t>(h));
        ++n;
    }
    ModeRest_ElevationBase = static_cast<i32>(static_cast<u32>(static_cast<std::int32_t>(ModeRest_ElevationBase) / n) & 0xFFFFFFE0u);
    struct {
        short m[9];
        short pad;
        long t[3];
    } matrix;
    inventory_ops::g.rot_matrix(Camera_Angles, matrix.m);
    const u32 f = facing & 0xFF;
    const auto* const offset = reinterpret_cast<const std::int16_t*>(move_script::At(::bof3::Img(inventory_ops::at::kCameraOffsets.v) + 4 * f));
    short v[3];
    v[2] = static_cast<short>(static_cast<std::int32_t>(ModeRest_ElevationBase) >> 1);
    v[0] = static_cast<short>(Sub(0x4000, AddInventoryOps(Shl(offset[0], 16), CentreX()) >> 9));
    v[1] = static_cast<short>(Sub(0x4000, AddInventoryOps(Shl(offset[1], 16), CentreZ()) >> 9));
    long out[3];
    inventory_ops::g.apply_matrix(matrix.m, v, out);
    matrix.t[0] = AddInventoryOps(Camera_ShiftX, out[0]);
    matrix.t[1] = AddInventoryOps(Camera_ShiftY, out[1]);
    matrix.t[2] = AddInventoryOps(AddInventoryOps(Camera_Distance, out[2]), 0x1194);
    inventory_ops::g.rot_matrix(Camera_Angles, matrix.m);
    inventory_ops::g.set_rot_matrix(reinterpret_cast<const unsigned long*>(&matrix));
    inventory_ops::g.set_trans_matrix(reinterpret_cast<const unsigned long*>(&matrix));
}

// original 0x592BD0 (PSX 0x801C8490): whether two things at (x0, z0) and
// (x1, z1), 16.16, are apart: |dx| or |dz| (32-bit differences) at least
// ((size0 + 1) + (size1 + 1)) * 0x8000, the sizes' low bytes.
extern "C" unsigned char __cdecl Encounter_Apart(long x0, long z0, long x1, long z1, unsigned size0, unsigned size1) {
    const i32 sum = AddInventoryOps(Shl(static_cast<i32>((size1 & 0xFF) + 1), 15), Shl(static_cast<i32>((size0 & 0xFF) + 1), 15));
    i32 dx = Sub(x0, x1);
    if (dx <= 0) dx = Sub(x1, x0);
    if (dx >= sum) return 1;
    i32 dz = Sub(z0, z1);
    if (dz <= 0) dz = Sub(z1, z0);
    if (dz >= sum) return 1;
    return 0;
}

// original 0x592C30 (PSX 0x801C8510): whether a thing of size class `size`
// fits at (x, z), 16.16. Its corner is (x, z) less size * 0x8000; the nibble
// of that corner's cell against b = size * 4 + 1 (a byte): below b no; b + 4
// or more yes; between, by where the corner sits in its cell: on both edges
// the nibble must be b; off the z edge only, b + 2 or b + 3; off the x edge
// only, b + 1 or b + 3; off both, no. The original also leaves the nibble
// and b in its first and third argument slots, which no caller reads (three
// call sites, each pops them).
extern "C" unsigned char __cdecl Encounter_CellFits(long x, long z, unsigned size) {
    const i32 s = Shl(static_cast<i32>(size & 0xFF), 15);
    const i32 xs = Sub(x, s), zs = Sub(z, s);
    const unsigned char v = inventory_ops::g.cell_nibble(Cell(xs), Cell(zs));
    const auto b = static_cast<unsigned char>(static_cast<unsigned char>(size << 2) + 1);
    if (v < b) return 0;
    if (static_cast<int>(v) >= static_cast<int>(b) + 4) return 1;
    const auto xf = static_cast<std::uint16_t>(xs), zf = static_cast<std::uint16_t>(zs);
    if (xf == 0) {
        if (zf == 0) return v == b;
        return v == b + 2 || v == b + 3;
    }
    if (zf != 0) return 0;
    return v == b + 1 || v == b + 3;
}

// original 0x592CD0 (PSX 0x801C881C): whether cell (x1, z1) is reached from
// (x0, z0) by open steps (Encounter_StepOpen), a step along x then one along
// z per turn, towards the target by the signs of the differences, within a
// budget of |dx| + |dz| steps held in a byte: a budget of 0x80 or more (the
// byte negative) fails at once, and the walk ends when a turn moves nothing
// or the budget goes negative. Failing, it tries again from the other end
// (the original swaps its own argument slots). All positions are compared
// as their low words; the steps' sums keep the whole registers, as the
// original's do, since only the low words are ever read.
extern "C" unsigned char __cdecl Encounter_PathClear(unsigned x0, unsigned z0, unsigned x1, unsigned z1) {
    unsigned a0 = x0, a1 = z0, a2 = x1, a3 = z1;
    const auto w = [](unsigned v) { return static_cast<std::uint16_t>(v); };
    for (unsigned char pass = 0; pass < 2; ++pass) {
        const unsigned dx = a2 - a0, dz = a3 - a1;
        const auto sx = static_cast<unsigned>(static_cast<i32>(inventory_ops::g.short_sign(dx)));
        const auto sz = static_cast<unsigned>(static_cast<i32>(inventory_ops::g.short_sign(dz)));
        auto budget = static_cast<unsigned char>(inventory_ops::g.short_abs(dz));
        budget = static_cast<unsigned char>(budget + static_cast<unsigned char>(inventory_ops::g.short_abs(dx)));
        if ((budget & 0x80) == 0) {
            unsigned x = a0, z = a1;
            for (;;) {
                bool moved = false;
                bool z_step = true;
                if (w(x) != w(a2)) {
                    const unsigned nx = x + sx;
                    if (inventory_ops::g.step_open(x, z, nx, z)) {
                        x = nx;
                        --budget;
                        moved = true;
                    }
                }
                if (w(x) == w(a2)) {
                    if (w(z) == w(a3)) return 1;
                } else if (w(z) == w(a3)) {
                    z_step = false;
                }
                if (z_step) {
                    const unsigned nz = z + sz;
                    if (inventory_ops::g.step_open(x, z, x, nz)) {
                        z = nz;
                        --budget;
                        moved = true;
                    }
                }
                if (w(x) == w(a2) && w(z) == w(a3)) return 1;
                if (!moved) break;
                if (static_cast<signed char>(budget) < 0) break;
            }
        }
        const unsigned t0 = a0; a0 = a2; a2 = t0;
        const unsigned t1 = a1; a1 = a3; a3 = t1;
    }
    return 0;
}

// original 0x592E00 (PSX 0x801C8180): the argument, negated (32 bits) when
// its low word is negative. The whole eax: the original loads it whole.
extern "C" unsigned __cdecl Short_Abs(unsigned v) { return static_cast<std::int16_t>(v) < 0 ? 0u - v : v; }

// original 0x592E10 (PSX 0x801C81A4): the sign of the low word, 1, 0 or -1,
// in ax - the original loads only ax, so eax's high word is the caller's.
extern "C" short __cdecl Short_Sign(unsigned v) {
    const auto s = static_cast<std::int16_t>(v);
    return s > 0 ? static_cast<short>(1) : s < 0 ? static_cast<short>(-1) : static_cast<short>(0);
}

// original 0x592E30 (PSX 0x801C8600): whether a step from cell (x0, z0) to
// (x1, z1) is open, all four their low words. The same cell: yes. The two
// ordered so that x0 < x1, or z0 < z1 on one x. Along x (z equal): the
// nibble of (x0, z0) is 2 or more and not 3. Along z (x equal): it is 3 or
// more. A diagonal whose steps have one sign: the nibble of (x0, z0) is 5 or
// more. The other diagonal: the nibble of (x0, z1) is 4 or more; else that of
// (x0, z0) is 2 or 4 or more (not 3) and that of (x0, z1), read again, 3 or
// more. The "x equal, z equal" path after the ordering is unreachable and
// kept as the original has it.
extern "C" unsigned char __cdecl Encounter_StepOpen(unsigned x0, unsigned z0, unsigned x1, unsigned z1) {
    unsigned a = x0, c = z0, b = x1, d = z1;   // esi, ecx, eax, edi
    const auto s16 = [](unsigned v) { return static_cast<std::int16_t>(v); };
    if (s16(a) == s16(b) && s16(c) == s16(d)) return 1;
    if (s16(a) > s16(b) || (s16(a) == s16(b) && s16(c) > s16(d))) {
        const unsigned t = a; a = b; b = t;
        const unsigned u = c; c = d; d = u;
    }
    if (s16(a) != s16(b)) {
        if (s16(c) == s16(d)) {
            const unsigned char v = inventory_ops::g.cell_nibble(a, c);
            if (v < 2) return 0;
            return v != 3;
        }
    } else if (s16(c) != s16(d)) {
        return inventory_ops::g.cell_nibble(a, c) >= 3;
    }
    const i32 product = static_cast<i32>(static_cast<std::int16_t>(b - a)) * static_cast<i32>(static_cast<std::int16_t>(d - c));
    if (product > 0) return inventory_ops::g.cell_nibble(a, c) >= 5;
    const unsigned char near = inventory_ops::g.cell_nibble(a, c);
    if (inventory_ops::g.cell_nibble(a, d) >= 4) return 1;
    if (near < 2) return 0;
    if (near == 3) return 0;
    return inventory_ops::g.cell_nibble(a, d) >= 3;
}

void Encounter_Inject() {
    BOF3_INJECT(Encounter_Place);
    BOF3_INJECT(Encounter_PlaceParty);
    BOF3_INJECT(Encounter_PlaceMember);
    BOF3_INJECT(Encounter_MemberClear);
    BOF3_INJECT(Encounter_MemberStands);
    BOF3_INJECT(Encounter_PickRow);
    BOF3_INJECT(Encounter_FillSlots);
    BOF3_INJECT(Encounter_PlaceEnemies);
    BOF3_INJECT(Encounter_EnemiesReachable);
    BOF3_INJECT(Encounter_EnemyClear);
    BOF3_INJECT(AreaMap_CellNibble);
    BOF3_INJECT(Encounter_OnScreen);
    BOF3_INJECT(Encounter_Project);
    BOF3_INJECT(Encounter_AimCamera);
    BOF3_INJECT(Encounter_Apart);
    BOF3_INJECT(Encounter_CellFits);
    BOF3_INJECT(Encounter_PathClear);
    BOF3_INJECT(Short_Abs);
    BOF3_INJECT(Short_Sign);
    BOF3_INJECT(Encounter_StepOpen);
}

void EncounterFromInventoryOps_Inject() {
    // DIVERGENCE DIV-0116 (D56): after the self-test, which compares the original's bounds.
    {
        static const std::uint8_t was = 0, is = 1;
        if (bof3::PatchBytes("EncounterOnScreenLowerBounds",
                             Low32(&g_on_screen_lower_bounds),
                             &was, &is, 1))
            bof3::Log("DIV-0116    the encounter's on-screen test refuses a corner left of or above the screen (D56)");
        g_on_screen_log = true;
    }
}
