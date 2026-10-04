// The party sets' field actions' shared helpers - round fourteen, stage-A
// group R0A, the seven functions 0x51C390..0x524E4E of the cut
// (analysis/round14_cut.tsv), taken before wave one so that its six
// field-action groups call them by name. Each read with capstone to its last
// instruction (docs/rest_0a.md section 1); each is a helper called directly
// (E8) by the party sets' state handlers, none a state, a case or a shared tail.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. Where the original would index
// Effect_Objects past its 20 records by an argument (0x51C6A0, 0x51DD70), ours
// aborts with a message; no caller passes such an index (section 5).
#include "game/rest_0a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

constexpr unsigned kEffectStride = 0x80;
constexpr unsigned kEffectCount = 20;
constexpr unsigned kMemberStride = 0x14C;
// ObjTrio's record 1 (0x802D40 + 0x14C): the originals start their member loop
// here, at index 1, as `mov edi, 0x802E8C`.
constexpr U kMemberOne = 0x802D40 + kMemberStride;   // ObjTrio + 0x14C
constexpr U kSteps = 0x6697B0;                        // Field_DirectionSteps
constexpr U kEffects = 0x7E11E0;                      // Effect_Objects

// Field_DirectionSteps' row for a direction (x then z, 16.16), read in place
// with the direction unmasked, as the originals index it (`shl eax, 3` on the
// zero-extended byte): the table has 8 rows; a byte above 7 reads the .data
// after it (docs/rest_0a.md section 5).
U StepX(unsigned d) { return static_cast<U>(Long(At(kSteps + d * 8))); }
U StepZ(unsigned d) { return static_cast<U>(Long(At(kSteps + d * 8 + 4))); }

// A 16.16 coordinate's cell, the high word, as the originals pass it to
// AreaMap_ByteAt (a dword whose low word is the cell; the callee reads that
// word sign-extended), and the cell one on, wrapping in 16 bits.
short Cell(U v) { return static_cast<short>(static_cast<std::uint16_t>(v >> 16)); }
short CellPlusOne(U v) { return static_cast<short>(static_cast<std::uint16_t>((v >> 16) + 1)); }

// An Effect_Objects record by an index argument's low byte (`and esi, 0xFF;
// shl esi, 7`). The originals read the record unchecked; ours aborts past the
// 20 records (no caller hands one: each passes PartyAction_Kind30Ahead's 0..19).
const unsigned char* EffectByArgument(unsigned index, const char* who) {
    const unsigned i = index & 0xFF;
    if (i >= kEffectCount)
        bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original reads 0x%X unchecked", who, i,
                    static_cast<unsigned>(kEffects + i * kEffectStride));
    return Effect_Objects + i * kEffectStride;
}

const unsigned char* Member(unsigned m) { return At(kMemberOne + (m - 1) * kMemberStride); }

bool MapTarget(unsigned char b) { return b == 0xF2; }
bool MapBlocked(unsigned char b) { return b == 0xF0 || b == 0xF1 || b == 0xF4 || b == 0xF6 || b == 0xF7; }

// The two steps ahead of Sprite_Current (x + 2 * step, z + 2 * step), read
// before any call: 0x51C390's and 0x522560's point.
void TwoAhead(U& x, U& z) {
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    x = static_cast<U>(Long(s + 0x34)) + 2u * StepX(d);
    z = static_cast<U>(Long(s + 0x38)) + 2u * StepZ(d);
}

// 0x524DA0's side probe in direction d (3 or 5): the point one step that way
// from Sprite_Current (re-read); the slope there steep (the scratch flag set,
// the low word above 0x40 signed) and the ground there above the sprite's
// height word (both s16): +0x2B = 0.
void SideProbe(unsigned d) {
    const unsigned char* s = Sprite_Current;
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (At(bof3::addr::DamageScratch)[0] == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = Sprite_Current;
    if (static_cast<short>(Word(s + 0x3E)) < static_cast<short>(ground)) Sprite_Current[0x2B] = 0;
}

}  // namespace

// original 0x51C390 (0x9F bytes): the point two steps ahead; 1 when
// Sprite_ObjectAt(point, 0) is not 0xFF, or AreaMap_ByteAt is 0xF2 at its cell,
// at the cell one on in x (only when x has a fraction), or one on in z (only
// when z has one); every call made whatever the earlier answered. The cells
// are the point's high words; the original pushes dwords whose upper halves
// are its own uninitialised stack, and AreaMap_ByteAt reads 16 bits.
extern "C" unsigned char __cdecl PartyAction_TargetAhead(void) {
    U x, z;
    TwoAhead(x, z);
    unsigned char found = 0;
    if (SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0) != 0xFF) found = 1;
    if (MapTarget(SH_CALL(AreaMap_ByteAt)(Cell(x), Cell(z)))) found = 1;
    if ((x & 0xFFFF) != 0 && MapTarget(SH_CALL(AreaMap_ByteAt)(CellPlusOne(x), Cell(z)))) found = 1;
    if ((z & 0xFFFF) != 0 && MapTarget(SH_CALL(AreaMap_ByteAt)(Cell(x), CellPlusOne(z)))) found = 1;
    return found;
}

// original 0x51C6A0 (0x92 bytes): the point two steps beyond effect object
// `index` in Sprite_Current's direction - the record's position and the
// direction read once, first - against ObjTrio's records 1 .. Field_MemberCount
// - 1, the count and the record's height word re-read for each:
// Sprite_PointInReach(x, z, height, 1, member) non-zero gives 1.
extern "C" unsigned char __cdecl PartyAction_MemberBeyondEffect(unsigned index) {
    const unsigned char* const e = EffectByArgument(index, "PartyAction_MemberBeyondEffect (0x51C6A0)");
    const unsigned d = Sprite_Current[8];
    const U x = static_cast<U>(Long(e + 0x34)) + 2u * StepX(d);
    const U z = static_cast<U>(Long(e + 0x38)) + 2u * StepZ(d);
    for (unsigned m = 1; m < Field_MemberCount; ++m)
        if (SH_CALL(Sprite_PointInReach)(static_cast<int>(x), static_cast<int>(z), static_cast<short>(Word(e + 0x3E)), 1,
                                         Member(m)) != 0)
            return 1;
    return 0;
}

// original 0x51DD70 (0x6A bytes): effect object `index`'s own position and its
// height word + 0x200 (a 16-bit add), all re-read for each member, against the
// same members. With Field_MemberCount 1 or less the record is not read.
extern "C" unsigned char __cdecl PartyAction_MemberOnEffect(unsigned index) {
    for (unsigned m = 1; m < Field_MemberCount; ++m) {
        const unsigned char* const e = EffectByArgument(index, "PartyAction_MemberOnEffect (0x51DD70)");
        const auto height = static_cast<short>(static_cast<std::uint16_t>(Word(e + 0x3E) + 0x200));
        if (SH_CALL(Sprite_PointInReach)(Long(e + 0x34), Long(e + 0x38), height, 1, Member(m)) != 0) return 1;
    }
    return 0;
}

// original 0x521510 (0xAA bytes): 0xFF unless Field_State +0x89 is 2 and its
// +0x138 bit 0 clear. The point one step ahead of Sprite_Current, read once;
// then each of the 20 Effect_Objects records in use, of kind 0x30, that
// Sprite_PointInReach(point, Sprite_Current's height word, 1, record) answers
// non-zero for: its index when its x equals Sprite_Current's x, or its z
// Sprite_Current's z (Sprite_Current re-read after each call, as the
// original's `mov ecx, [Sprite_Current]` there); else on. 0xFF at the end.
extern "C" unsigned char __cdecl PartyAction_Kind30Ahead(void) {
    const unsigned char* const state = Field_State;
    if (state[0x89] != 2 || (state[0x138] & 1) != 0) return 0xFF;
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(Long(s + 0x34)) + StepX(d);
    const U z = static_cast<U>(Long(s + 0x38)) + StepZ(d);
    for (unsigned i = 0; i < kEffectCount; ++i) {
        const unsigned char* const e = Effect_Objects + i * kEffectStride;
        if (e[0] == 0 || e[5] != 0x30) continue;
        if (SH_CALL(Sprite_PointInReach)(static_cast<int>(x), static_cast<int>(z),
                                         static_cast<short>(Word(Sprite_Current + 0x3E)), 1, e) == 0)
            continue;
        if (Long(e + 0x34) == Long(Sprite_Current + 0x34)) return static_cast<unsigned char>(i);
        if (Long(e + 0x38) == Long(Sprite_Current + 0x38)) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x522560 (0xED bytes): the point two steps ahead; 1 when
// Sprite_ObjectAt(point, 0) is not 0xFF, Field_EffectAhead is not 0xFF, or
// AreaMap_ByteAt is 0xF0, 0xF1, 0xF4, 0xF6 or 0xF7 at its cell, at the cell
// one on in x (x with a fraction), or one on in z (z with one) - every call
// made whatever the earlier answered. The cells as PartyAction_TargetAhead's.
extern "C" unsigned char __cdecl PartyAction_BlockedAhead(void) {
    U x, z;
    TwoAhead(x, z);
    unsigned char blocked = 0;
    if (SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0) != 0xFF) blocked = 1;
    if (SH_CALL(Field_EffectAhead)() != 0xFF) blocked = 1;
    if (MapBlocked(SH_CALL(AreaMap_ByteAt)(Cell(x), Cell(z)))) blocked = 1;
    if ((x & 0xFFFF) != 0 && MapBlocked(SH_CALL(AreaMap_ByteAt)(CellPlusOne(x), Cell(z)))) blocked = 1;
    if ((z & 0xFFFF) != 0 && MapBlocked(SH_CALL(AreaMap_ByteAt)(Cell(x), CellPlusOne(z)))) blocked = 1;
    return blocked;
}

// original 0x522FB0 (0x76 bytes): with Effect_FindFree's slot not 0xFF, the
// record gets +0 = 1, +5 = 0x34, +1 = the state's byte, +0x34 / +0x38 = the
// words x and z (movsx) shifted to 16.16, +0x3E = AreaMap_Elevation(x read
// back from the record, z) + 0x200 (its low word), +0xB = 1. The slot is used
// as FindFree answers it, unchecked (FindFree gives 0..19 or 0xFF), as
// Effect_SpawnAtCell uses it. The original's eax at its ret is no caller's.
extern "C" void __cdecl Effect_SpawnAtCellHigh(unsigned state, unsigned x, unsigned z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = Effect_Objects + slot * kEffectStride;
    e[0] = 1;
    e[5] = 0x34;
    e[1] = static_cast<unsigned char>(state);
    SetLong(e + 0x34, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<short>(x))) << 16));
    const auto zz = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<short>(z))) << 16);
    SetLong(e + 0x38, zz);
    const long ground = SH_CALL(AreaMap_Elevation)(Long(e + 0x34), zz);
    SetWord(e + 0x3E, static_cast<unsigned>(ground) + 0x200);
    e[0xB] = 1;
}

// original 0x524DA0 (0xAF bytes): Sprite_Current +0x2B = 1, then the side
// probes in directions 3 and 5 (the original reads Field_DirectionSteps' rows 3
// and 5 by their addresses, 0x6697C8 and 0x6697D8, and pushes the direction
// as an immediate). The original's eax at its ret is the last callee's, no
// caller's.
extern "C" void __cdecl PartyAction_SideProbes(void) {
    Sprite_Current[0x2B] = 1;
    SideProbe(3);
    SideProbe(5);
}

void Rest0A_Inject() {
    if (bof3::WantsShadow("rest_0a")) rest_0a::SelfTest();
    BOF3_INJECT(PartyAction_TargetAhead);
    BOF3_INJECT(PartyAction_MemberBeyondEffect);
    BOF3_INJECT(PartyAction_MemberOnEffect);
    BOF3_INJECT(PartyAction_Kind30Ahead);
    BOF3_INJECT(PartyAction_BlockedAhead);
    BOF3_INJECT(Effect_SpawnAtCellHigh);
    BOF3_INJECT(PartyAction_SideProbes);
}
