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
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned char*, unsigned)>(kSetAnimFrom),
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

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using i32 = std::int32_t;
using u32 = std::uint32_t;

unsigned char* PosX(u32 m) { return At(at::kMemberPos + 8 * m); }
unsigned char* PosZ(u32 m) { return At(at::kMemberPos + 8 * m + 4); }
unsigned char* Member(u32 m) { return At(at::kMembers + 4 * m); }   // +0 placed, +1 wide
unsigned char* Enemy(u32 k) { return At(at::kEnemies + at::kEnemyStride * k); }
unsigned char* Kind(u32 kind) { return At(at::kKinds + at::kKindStride * kind); }
unsigned char* Obj(u32 i) { return ObjTrio + at::kObjStride * i; }
unsigned char& Byte(u32 address) { return *At(address); }

i32 CentreX() { return Long(At(at::kCentreX)); }
i32 CentreZ() { return Long(At(at::kCentreZ)); }
// A 16.16 coordinate's cell: `sar reg, 16`.
u32 Cell(i32 v) { return static_cast<u32>(v >> 16); }
// A shift of a value the original holds as a 32-bit register.
i32 Shl(i32 v, unsigned n) { return static_cast<i32>(static_cast<u32>(v) << n); }
i32 Neg(i32 v) { return static_cast<i32>(0u - static_cast<u32>(v)); }
i32 Add(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }
i32 Sub(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) - static_cast<u32>(b)); }

}  // namespace

// ===========================================================================
// The placement: the cell map and the geometry under it

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
    const unsigned char b = At(at::kCellMap)[i >> 1];
    return (i & 1) ? static_cast<unsigned char>(b & 0xF) : static_cast<unsigned char>(b >> 4);
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
            const unsigned char v = g.cell_nibble(a, c);
            if (v < 2) return 0;
            return v != 3;
        }
    } else if (s16(c) != s16(d)) {
        return g.cell_nibble(a, c) >= 3;
    }
    const i32 product = static_cast<i32>(static_cast<std::int16_t>(b - a)) * static_cast<i32>(static_cast<std::int16_t>(d - c));
    if (product > 0) return g.cell_nibble(a, c) >= 5;
    const unsigned char near = g.cell_nibble(a, c);
    if (g.cell_nibble(a, d) >= 4) return 1;
    if (near < 2) return 0;
    if (near == 3) return 0;
    return g.cell_nibble(a, d) >= 3;
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
        const auto sx = static_cast<unsigned>(static_cast<i32>(g.short_sign(dx)));
        const auto sz = static_cast<unsigned>(static_cast<i32>(g.short_sign(dz)));
        auto budget = static_cast<unsigned char>(g.short_abs(dz));
        budget = static_cast<unsigned char>(budget + static_cast<unsigned char>(g.short_abs(dx)));
        if ((budget & 0x80) == 0) {
            unsigned x = a0, z = a1;
            for (;;) {
                bool moved = false;
                bool z_step = true;
                if (w(x) != w(a2)) {
                    const unsigned nx = x + sx;
                    if (g.step_open(x, z, nx, z)) {
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
                    if (g.step_open(x, z, x, nz)) {
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
    const unsigned char v = g.cell_nibble(Cell(xs), Cell(zs));
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

// original 0x592BD0 (PSX 0x801C8490): whether two things at (x0, z0) and
// (x1, z1), 16.16, are apart: |dx| or |dz| (32-bit differences) at least
// ((size0 + 1) + (size1 + 1)) * 0x8000, the sizes' low bytes.
extern "C" unsigned char __cdecl Encounter_Apart(long x0, long z0, long x1, long z1, unsigned size0, unsigned size1) {
    const i32 sum = Add(Shl(static_cast<i32>((size1 & 0xFF) + 1), 15), Shl(static_cast<i32>((size0 & 0xFF) + 1), 15));
    i32 dx = Sub(x0, x1);
    if (dx <= 0) dx = Sub(x1, x0);
    if (dx >= sum) return 1;
    i32 dz = Sub(z0, z1);
    if (dz <= 0) dz = Sub(z1, z0);
    if (dz >= sum) return 1;
    return 0;
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
    return g.rot_trans_pers(v, sxy, &depth);
}

// original 0x5928F0 (PSX 0x801C805C): whether a thing of size class `size`
// at (x, z) is on screen. Its four corners, (size + 1) * 0x4000 out, each on
// the ground (AreaMap_Elevation) and projected: refused if any screen x is
// 320 or more or any y 240 or more. Then the first corner again: yes if its
// screen y less `margin` (a byte) is 8 or more. The PSX compares unsigned
// shorts, so a corner left of or above the screen fails there too; the PC's
// floats accept one (docs/inventory_ops.md section 5). A NaN passes the
// corners and fails the last test, as the x87 compares have it.
extern "C" unsigned char __cdecl Encounter_OnScreen(long x, long z, unsigned size, unsigned margin) {
    i32 a = Shl(static_cast<i32>((size & 0xFF) + 1), 14);
    i32 b = a;
    float sxy[2];
    for (unsigned char t = 0; t < 4; ++t) {
        const i32 pz = Add(b, z), px = Add(a, x);
        const i32 y = Shl(static_cast<std::int16_t>(g.elevation(px, pz)), 16);
        g.project(px, pz, y, reinterpret_cast<unsigned long*>(sxy));
        if (sxy[0] >= 320.0f) return 0;
        if (sxy[1] >= 240.0f) return 0;
        const i32 na = Neg(a);
        a = b;
        b = na;
    }
    const i32 pz = Add(b, z), px = Add(a, x);
    const i32 y = Shl(static_cast<std::int16_t>(g.elevation(px, pz)), 16);
    g.project(px, pz, y, reinterpret_cast<unsigned long*>(sxy));
    return static_cast<double>(sxy[1]) - static_cast<double>(margin & 0xFF) >= 8.0;
}

// ===========================================================================
// The placement: the party

// original 0x592400 (PSX 0x801C7880): whether member m's spot keeps apart
// from every other placed member of the first three (their wide bytes the
// sizes; k is compared with the argument's low byte) and is reached from the
// centre (Encounter_PathClear, cells). The member's spot is held for the
// separations; the centre and the spot are read again for the path.
extern "C" unsigned char __cdecl Encounter_MemberClear(unsigned member) {
    const u32 m = member & 0xFF;
    const unsigned char wide = Member(m)[1];
    const i32 x = Long(PosX(m)), z = Long(PosZ(m));
    for (unsigned char k = 0; k < 3; ++k) {
        if (k == static_cast<unsigned char>(member)) continue;
        if (Member(k)[0] == 0) continue;
        if (!g.apart(x, z, Long(PosX(k)), Long(PosZ(k)), wide, Member(k)[1])) return 0;
    }
    return g.path_clear(Cell(CentreX()), Cell(CentreZ()), Cell(Long(PosX(m))), Cell(Long(PosZ(m)))) != 0;
}

// original 0x5924E0 (PSX 0x801C79C0): member m's spot on the ground -
// Sprite_Current +0x3C its elevation << 16, the sprite read after the call -
// then whether it fits (Encounter_CellFits, the wide byte the size) and no
// object stands there (Sprite_ObjectAt 0xFF). The spot is read at each use.
extern "C" unsigned char __cdecl Encounter_MemberStands(unsigned member) {
    const u32 m = member & 0xFF;
    const unsigned wide = Member(m)[1];
    const long e = g.elevation(Long(PosX(m)), Long(PosZ(m)));
    SetLong(Sprite_Current + 0x3C, Shl(static_cast<std::int16_t>(e), 16));
    if (!g.cell_fits(Long(PosX(m)), Long(PosZ(m)), wide)) return 0;
    return g.object_at(Long(PosX(m)), Long(PosZ(m)), wide) == 0xFF;
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
    i32 dx = Long(At(at::kPartyOffsets + 8 * index)), dz = Long(At(at::kPartyOffsets + 8 * index + 4));
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
    const auto r = static_cast<unsigned char>(g.rand() & 1);
    for (unsigned char pass = 0; pass < 2; ++pass) {
        SetLong(PosX(m), Add(CentreX(), dx));
        SetLong(PosZ(m), Add(CentreZ(), dz));
        for (unsigned char t = 0; t < 8; ++t) {
            if (g.member_clear(member) && g.member_stands(member)) {
                Member(m)[0] = 1;
                return 1;
            }
            if ((r ^ pass) != 0) SetLong(PosX(m), Add(Long(PosX(m)), sx));
            else SetLong(PosZ(m), Add(Long(PosZ(m)), sz));
        }
    }
    return 0;
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
    const unsigned char formation = Byte(at::kFormation);
    const auto n = static_cast<unsigned char>(g.party_count(1));
    unsigned char* const order = At(at::kOrder);
    for (unsigned char i = 0; i < 3; ++i) {
        order[i] = i;
        Member(i)[0] = 0;
        const unsigned char record = At(at::kMemberRecord)[At(at::kPartyList2)[i]];
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
                x = Add(Long(At(at::kRecentre + 8 * k)), CentreX());
                z = Add(Long(At(at::kRecentre + 8 * k + 4)), CentreZ());
                if (g.cell_fits(x, z, 1) && g.path_clear(Cell(CentreZ()), Cell(CentreZ()), Cell(x), Cell(z))) break;
            }
            SetLong(At(at::kCentreX), x);
            SetLong(At(at::kCentreZ), z);
        }
    }
    for (unsigned char i = 0; i < n; ++i)
        if (!g.place_member(formation, order[i], static_cast<unsigned char>(n - 1), Byte(at::kFacing))) return 0;
    return 1;
}

// ===========================================================================
// The placement: the enemies

// original 0x592800 (PSX 0x801C7ED8): whether a thing at (x, z) of size
// class `size` keeps apart from each active enemy of the first `count` (its
// byte) - each enemy's size its kind's byte +0x86.
extern "C" unsigned char __cdecl Encounter_EnemyClear(unsigned count, long x, long z, unsigned size) {
    const auto n = static_cast<unsigned char>(count);
    for (unsigned char k = 0; k < n; ++k) {
        unsigned char* const e = Enemy(k);
        if (e[0] == 0) continue;
        if (!g.apart(x, z, Long(e + 4), Long(e + 8), size, Kind(e[1])[0x86])) return 0;
    }
    return 1;
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
    Byte(at::kPlaced) = 0;
    for (std::uint16_t i = 0; static_cast<std::int16_t>(i) < 8; ++i) {
        unsigned char* const e = Enemy(i);
        if (e[0] == 0) continue;
        unsigned char* const kind = Kind(e[1]);
        const u32 c = Byte(at::kEnemyCount);
        const i32 triangle = static_cast<i32>((c - 1) * c) >> 1;   // lea eax, [ecx - 1]; imul eax, ecx; sar eax, 1
        const auto index = static_cast<std::int16_t>(Add(triangle, Byte(at::kPlaced)));
        i32 ox = Add(Long(At(at::kEnemyOffsets + 8 * index)), 0x50000);
        i32 oz = Long(At(at::kEnemyOffsets + 8 * index + 4));
        for (auto f = static_cast<unsigned char>(Byte(at::kFacing)); f != 0; f = static_cast<unsigned char>((f + 1) & 3)) {
            const i32 t = ox;
            ox = oz;
            oz = Neg(t);
        }
        std::uint16_t t = 0;
        for (;;) {
            const auto* const j = reinterpret_cast<const signed char*>(At(at::kJitter + 2 * static_cast<std::int16_t>(t)));
            const i32 x = Add(Add(Shl(j[0], 15), CentreX()), ox);
            SetLong(e + 4, x);
            const i32 z = Add(Add(Shl(j[1], 15), CentreZ()), oz);
            SetLong(e + 8, z);
            if (g.cell_fits(x, z, kind[0x86]) && g.enemy_clear(i, Long(e + 4), Long(e + 8), kind[0x86])) {
                ++Byte(at::kPlaced);
                break;
            }
            ++t;
            if (static_cast<std::int16_t>(t) >= 0x31) break;
        }
        if (t == 0x31) e[0] = 0;
    }
    return Byte(at::kPlaced) != 0;
}

// original 0x592760 (PSX 0x801C7DC4): each active enemy slot stays active if
// some member of Party_Count(1) - asked again after each member - reaches it
// (Encounter_PathClear from the member's cell to the enemy's, its words +6
// and +0xA), counting them in 0x6BE071; answers that count != 0.
extern "C" unsigned char __cdecl Encounter_EnemiesReachable(void) {
    Byte(at::kPlaced) = 0;
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const e = Enemy(k);
        if (e[0] == 0) continue;
        unsigned char reached = 0;
        if (static_cast<unsigned char>(g.party_count(1)) != 0) {
            std::uint16_t m = 0;
            for (;;) {
                if (g.path_clear(Cell(Long(PosX(static_cast<std::int16_t>(m)))), Cell(Long(PosZ(static_cast<std::int16_t>(m)))),
                                 Word(e + 6), Word(e + 0xA))) {
                    ++Byte(at::kPlaced);
                    reached = 1;
                    break;
                }
                ++m;
                if (!(static_cast<std::int16_t>(m) < static_cast<std::int16_t>(static_cast<unsigned char>(g.party_count(1))))) break;
            }
        }
        e[0] = reached;
    }
    return Byte(at::kPlaced) != 0;
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
    SetLong(At(at::kHeightAvg), 0);
    i32 n = 0;
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const e = Enemy(k);
        if (e[0] == 0) continue;
        const long h = g.elevation(Long(e + 4), Long(e + 8));
        SetLong(At(at::kHeightAvg), Add(Long(At(at::kHeightAvg)), static_cast<std::int16_t>(h)));
        ++n;
    }
    for (unsigned char m = 0; m < static_cast<unsigned char>(g.party_count(1)); ++m) {
        const long h = g.elevation(Long(PosX(m)), Long(PosZ(m)));
        SetLong(At(at::kHeightAvg), Add(Long(At(at::kHeightAvg)), static_cast<std::int16_t>(h)));
        ++n;
    }
    SetLong(At(at::kHeightAvg), static_cast<i32>(static_cast<u32>(Long(At(at::kHeightAvg)) / n) & 0xFFFFFFE0u));
    struct {
        short m[9];
        short pad;
        long t[3];
    } matrix;
    g.rot_matrix(Camera_Angles, matrix.m);
    const u32 f = facing & 0xFF;
    const auto* const offset = reinterpret_cast<const std::int16_t*>(At(at::kCameraOffsets + 4 * f));
    short v[3];
    v[2] = static_cast<short>(Long(At(at::kHeightAvg)) >> 1);
    v[0] = static_cast<short>(Sub(0x4000, Add(Shl(offset[0], 16), CentreX()) >> 9));
    v[1] = static_cast<short>(Sub(0x4000, Add(Shl(offset[1], 16), CentreZ()) >> 9));
    long out[3];
    g.apply_matrix(matrix.m, v, out);
    matrix.t[0] = Add(Camera_ShiftX, out[0]);
    matrix.t[1] = Add(Camera_ShiftY, out[1]);
    matrix.t[2] = Add(Add(Camera_Distance, out[2]), 0x1194);
    g.rot_matrix(Camera_Angles, matrix.m);
    g.set_rot_matrix(reinterpret_cast<const unsigned long*>(&matrix));
    g.set_trans_matrix(reinterpret_cast<const unsigned long*>(&matrix));
}

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
        do r = static_cast<unsigned char>(g.rand() & 0x7F);
        while (r > 0x64);
        unsigned char* const obj = Obj(static_cast<u32>(i));
        if (i >= static_cast<int>(Field_MemberCount)) {
            if (r <= 0x32) ++count;
            continue;
        }
        const u32 record = At(at::kMemberRecord)[At(at::kPartyList)[i]];
        if (At(at::kRecords + record * at::kRecordSize)[0x38] < r) continue;
        const unsigned char e = g.effect_free();
        if (e != 0xFF) {
            unsigned char* const fx = Effect_Objects + (static_cast<u32>(e) << 7);
            fx[0] = 1;
            fx[5] = 6;
            fx[6] = 1;
            SetWord(fx + 0x2E, Word(obj + 0x2E));
            SetWord(fx + 0x30, Word(obj + 0x30));
            const u32 id = obj[0x89];
            Obj(0)[0xB] = e;
            SetLong(fx + 0xC, At(at::kAlertOffsets)[2 * id]);
            SetLong(fx + 0x10, At(at::kAlertOffsets)[2 * id + 1]);
        }
        ++count;
    }
    Byte(at::kInitiative) = count == 5 ? 1 : count != 0 ? 0 : 2;
}

// original 0x532660 (PSX 0x801BFDC8): each member's +0xB the way it turns
// (Sprite_TurnSense) to the direction 0x660B3C gives the battle's facing;
// the count and the facing read per member, the sprite after the call.
extern "C" void __cdecl Encounter_PartyTurnSense(void) {
    for (u32 i = 0; i < Field_MemberCount; ++i) {
        Sprite_Current = Obj(i);
        const unsigned char s = g.turn_sense(At(at::kFaceTable)[Byte(at::kBattleFacing)]);
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
            if (Word(At(at::kRecords + record * at::kRecordSize + 0x10)) & 0x4000) {
                g.set_anim_from(obj[0x4B], 0, At(at::kAnimBuffer), 0xA00);
                Sprite_Current[0xB] = 0;
                continue;
            }
            const unsigned char target = At(at::kFaceTable)[Byte(at::kBattleFacing)];
            const unsigned char d = obj[8], turn = obj[0xB];
            if (d == target) {
                if (turn != 0) {
                    g.set_anim_from(d, 0, At(at::kAnimBuffer), 0xA00);
                    Sprite_Current[0xB] = 0;
                }
            } else {
                obj[8] = static_cast<unsigned char>((d + turn) & 7);
                g.set_anim_from(Sprite_Current[8], 0, At(at::kAnimBuffer), 0xA00);
            }
            g.script_tick();
        }
        return 0;
    }
    if (Byte(at::kBattleFlags) & 4) return 1;
    for (u32 j = 0; static_cast<i32>(j) < static_cast<i32>(n); ++j, n = Field_MemberCount) {
        unsigned char* const obj = Obj(j);
        Field_State = obj;
        Sprite_Current = obj;
        for (u32 w = 0; w < 32; ++w) {
            const u32 o = (static_cast<u32>(obj[5]) << 5) + w;
            SetWord(At(at::kShadeSaved + 2 * o), Word(At(at::kShadeSource + 2 * o)));
        }
        g.shade_begin();
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
    if (Byte(at::kBattleFlags) & 4) return 1;
    const u32 f = Byte(at::kBattleFacing);
    const auto* const step = reinterpret_cast<const signed char*>(At(at::kSideSteps + 2 * f));
    SetLong(At(at::kCentreX), Add(Shl(step[0], 16), Field_Kind2X));
    SetLong(At(at::kCentreZ), Add(Shl(step[1], 16), Field_Kind2Z));
    u32 n = Field_MemberCount;
    if (n != 0) {
        for (u32 i = 0; i < n; ++i, n = Field_MemberCount) {
            unsigned char* const obj = Obj(i);
            Field_State = obj;
            Sprite_Current = obj;
            const unsigned char state = obj[0xB];
            if (state == 0) {
                if (g.shade_lower(8)) {
                    unsigned char* const fs = Field_State;
                    const unsigned char id = fs[0x89];
                    u32 k = 0;
                    while (k < 3 && At(at::kPartyList2)[k] != id) ++k;
                    fs[0x138] = static_cast<unsigned char>(fs[0x138] & 0xFE);
                    Sprite_Current[0x29] = Draw_OtSlot;
                    SetLong(Sprite_Current + 0x34, Long(PosX(k)));
                    SetLong(Sprite_Current + 0x38, Long(PosZ(k)));
                    const long ground = g.ground_at(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
                    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground));
                    Sprite_Current[0x48] = 0;
                    Sprite_Current[0xB] = 1;
                }
            } else if (state == 1) {
                if (g.shade_raise(8)) {
                    unsigned char* const sc = Sprite_Current;
                    for (u32 w = 0; w < 32; ++w) {
                        const u32 o = (static_cast<u32>(sc[5]) << 5) + w;
                        SetWord(At(at::kShadeClut + 2 * o), Word(At(at::kShadeSaved + 2 * o)));
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
            g.script_tick();
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
        SetLong(obj + 0x34, Long(PosX(i)));
        SetLong(Sprite_Current + 0x38, Long(PosZ(i)));
        const long ground = g.ground_at(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
        SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground));
        Sprite_Current[0xB] = 0;
        const u32 record = Field_State[0x148];
        if (At(at::kRecords + record * at::kRecordSize + 0x11)[0] & 0x40)
            g.set_animation(static_cast<unsigned char>(Byte(at::kBattleFacing) + 0x1C));
        else
            g.set_animation(Byte(at::kBattleFacing));
    }
}

// original 0x532B60 (the PSX twin not paired): bit 4 of 0x904AE5 answers 1
// at once; else each member (Field_State and Sprite_Current it) whose +0xB is
// 0 gets +0xB 1 when Sprite_ScriptTickOnce answers non-zero. Answers whether
// every member's +0xB is 1 (1 for no members), the count the last one read.
// Like 0x532A70 it first searches the first party list for nothing.
extern "C" unsigned char __cdecl Encounter_PartyScriptOnce(void) {
    if (Byte(at::kBattleFlags) & 0x10) return 1;
    u32 n = Field_MemberCount;
    if (n != 0) {
        for (u32 i = 0; i < n; ++i, n = Field_MemberCount) {
            unsigned char* const obj = Obj(i);
            Field_State = obj;
            Sprite_Current = obj;
            if (obj[0xB] == 0 && g.script_once()) Sprite_Current[0xB] = 1;
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
    BOF3_INJECT(Encounter_PlaceParty);
    BOF3_INJECT(Encounter_PlaceMember);
    BOF3_INJECT(Encounter_MemberClear);
    BOF3_INJECT(Encounter_MemberStands);
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
    BOF3_INJECT(Sprite_TurnSense);
    BOF3_INJECT(Encounter_RollInitiative);
    BOF3_INJECT(Encounter_PartyTurnSense);
    BOF3_INJECT(Encounter_PartyTurn);
    BOF3_INJECT(Encounter_PartyToPlaces);
    BOF3_INJECT(Encounter_PartyAtPlaces);
    BOF3_INJECT(Encounter_PartyScriptOnce);
    BOF3_INJECT(Sprite_ShadeLower);
}
