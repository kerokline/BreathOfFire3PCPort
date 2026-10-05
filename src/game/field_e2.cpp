// Round twelve group FE2 (docs/field_e2.md): the field engine's resident code
// in 0x5341C0..0x5372D8, 0x56D240..0x5729F8 and 0x593960..0x594060 that no
// earlier group took - 51 functions:
//
//   the event script's rest   Scenario_CallB, Field_AfterBattleTally and its
//                             two record counts, Field_FaceMode11Object,
//                             Field_FloorHurt, the wide cell tests,
//                             Field_ObjectTrigger and the two hooks under it
//   the leader's hop helpers  Leader_* (called by FC3's and FE1's states)
//   the mode-11 object        Mode11_Object* (the object at 0x905DA0 that
//                             Game_Mode 11 moves) and its draw
//   the field tail            FieldTail_* (seven Field_ModeTailKinds slots)
//   draw layers               three MapCell_Handlers kinds, AreaMap_ClearCell
//   the trade screen          ItemTrade_* (0x66A47C / 0x66A484's states)
//
// Every call goes through the scenario harness (SH_CALL / SH_AT, a table's
// word read in place), so the start-up fuzz (field_e2_fuzz.cpp) stands
// recorders in for ours as for the originals' copies. Faithful: the one
// difference is DIV-0023's, extended - the fourth word of the SVECTORs the
// draw functions build on their stacks is 0 here where the originals leave
// stale stack (docs/DIVERGENCE.md DIV-0023). Where an original indexes past
// its table, divides by 0 or walks a record list past its end, ours aborts
// with a message (docs/field_e2.md section 6).
#include "game/field_e2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_e2_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_e2::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U a) { return *At(a); }
U L(U a) { return static_cast<U>(Long(At(a))); }
void SetL(U a, U v) { SetLong(At(a), static_cast<std::int32_t>(v)); }
std::uint16_t W(U a) { return Word(At(a)); }
void SetW(U a, unsigned v) { SetWord(At(a), v); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U LongAt(const unsigned char* p) { return static_cast<U>(Long(p)); }
int S8(unsigned v) { return static_cast<signed char>(v); }
int S16(unsigned v) { return static_cast<short>(v); }
// `sub; cdq; xor; sub`: the 32-bit difference's magnitude, wrapping as the
// original's (the magnitude of 0x80000000 is itself, negative).
std::int32_t AbsDiff(U a, U b) {
    const U d = a - b;
    return static_cast<std::int32_t>(static_cast<std::int32_t>(d) < 0 ? 0u - d : d);
}

using Handler = void (__cdecl*)();

// A .data table's entry read in place (swapped for recorders while the fuzz
// runs); an entry that is not code - an index past the table - aborts, where
// the original jumps there (scena_sc0.cpp's CodeAt).
U CodeAt(U table, U index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = L(cell);
    if (!scenario_harness::g_active && (entry < at::kTextLo || entry >= at::kTextHi))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell);
    return entry;
}

// Sprite_EnsureAnimation of a byte (the callee compares al and hands on dl;
// the originals push whatever register held it).
void Ensure(unsigned animation) { SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(animation)); }

// A member slot's CLUT word and palette (Sprite_Current +5).
U ClutWord(const unsigned char* s) { return at::kClutWords + (static_cast<U>(s[5]) << 6); }
unsigned short* Palette(const unsigned char* s) {
    return reinterpret_cast<unsigned short*>(static_cast<std::uintptr_t>(at::kPalettes + (static_cast<U>(s[5]) << 6)));
}

// The pace's reach: Field_DirectionSteps[+8] times (+0x70 + 2), from +0x34 / +0x38.
void Ahead(const unsigned char* s, std::int32_t& x, std::int32_t& z) {
    const U dir = s[8], k = static_cast<U>(s[0x70]) + 2u;
    x = static_cast<std::int32_t>(L(at::kDirectionSteps + dir * 8u) * k + LongAt(s + 0x34));
    z = static_cast<std::int32_t>(L(at::kDirectionSteps + 4u + dir * 8u) * k + LongAt(s + 0x38));
}

// The view follows the leader when +5 is 0 and bit 3 of the script flags'
// low bytes is clear; then every fourth frame one tick of its script.
void FollowAndTick(unsigned char* s) {
    if (s[5] == 0 && ((B(at::kScriptFlags2) | B(at::kScriptFlagsLow)) & 8) == 0)
        SH_CALL(MapView_SetElevation)(static_cast<short>(W(Key(s) + 0x3E)));
    if ((Frame_Counter & 3) == 0) SH_CALL(Sprite_ScriptTickOnce)();
}

}  // namespace

// ============================================================================
// The event script's rest
// ============================================================================

// original 0x5341C0: entry (n & 0xFF) of the current chapter's call table B
// (Scenario_CallBTables 0x660BD4[Cond_ByteFA], PSX 0x801C2E34), reached with
// the caller's arguments in place. Scenario_CallA's shape (field_modes.cpp):
// a tail jump, because the entries take anything from nothing to several
// words where the callers left them. Unchecked, as the original - chapter 0's
// table pointer is 0 (docs/field_e2.md section 6).
extern "C" __attribute__((naked)) void __cdecl Scenario_CallB(unsigned) {
    asm("movsbl 0x8034E0, %eax\n\t"          // Cond_ByteFA, the chapter
        "movl 4(%esp), %ecx\n\t"
        "movl 0x660BD4(,%eax,4), %edx\n\t"   // Scenario_CallBTables
        "andl $0xFF, %ecx\n\t"
        "jmp *(%edx,%ecx,4)");
}

// original 0x5343C0: with no pair of the sixty at 0x9046D0 (+0 in use, +1 the
// record, 1-based) naming any of the eight records at 0x9048B0 whose kind +0
// is 4 and key +1 is `key` (a byte), *counter + 1. One count over all the
// matching records: a single pair naming any of them spares the counter.
extern "C" void __cdecl Records_CountUnpaired(unsigned key, U* counter) {
    unsigned char named = 0;
    for (U r = 0; r < 8; ++r) {
        if (B(at::kRecords + 8u * r) != 4) continue;
        if (B(at::kRecords + 1u + 8u * r) != static_cast<unsigned char>(key)) continue;
        for (U p = at::kPairs + 1u; p < at::kPairsEnd + 1u; p += 8)
            if (B(p - 1) != 0 && B(p) == r + 1u) ++named;
    }
    if (named == 0) ++*counter;
}

// original 0x534420: for each of the eight records at 0x9048B0 whose kind +0
// is `kind` (a byte) and whose +3 is not 0, its tally dword +4 + 1 unless a
// pair of the sixty at 0x9046D0 names it.
extern "C" void __cdecl Records_CountUnpairedOfKind(unsigned kind) {
    for (U r = 0; r < 8; ++r) {
        unsigned char named = 0;
        if (B(at::kRecords + 8u * r) != static_cast<unsigned char>(kind)) continue;
        if (B(at::kRecords + 3u + 8u * r) == 0) continue;
        for (U p = at::kPairs + 1u; p < at::kPairsEnd + 1u; p += 8)
            if (B(p - 1) != 0 && B(p) == r + 1u) ++named;
        if (named == 0) SetL(at::kRecords + 4u + 8u * r, L(at::kRecords + 4u + 8u * r) + 1u);
    }
}

// original 0x5341E0: the tallies GameMode_Field takes on its return from a
// battle (0x4961F2, after Field_ZoneCounterRoll). Bit 4 of 0x904654 set and
// of 0x904657 clear: the byte 0x9045FB up by one below 0x1E. From chapter 8
// with 0x9046CB not 0: the first clear story flag n of 0xF6..0xFF (none: on
// to the counts); the in-use pairs at 0x9046D0 naming record 0xA and those
// naming 0xB counted (bytes); with the 0xB count - the 0xA count for n 0xF6,
// 0xF9, 0xFC - not 0, the dword 0x9046B0 up by one, and at the threshold
// max(5, t[n - 0xF6] - bias) (t 20 20 20 40 20 20 60 20 20 20 on the stack;
// the bias 0x9046CD for those three flags, 0x9046CE for the rest; unsigned
// compare) story flag n set and the dword 0. Then Records_CountUnpaired(1,
// 0x9046C0) below 7 at 0x9046CC, (0, 0x9046C4) below 10 at 0x9046CB, and
// Records_CountUnpairedOfKind(5), (0xB). Last, story flag 0xF5 set when clear
// with 0x9046CC at 4 or more. No PSX twin paired.
extern "C" void __cdecl Field_AfterBattleTally(void) {
    static const signed char kThresholds[10] = {0x14, 0x14, 0x14, 0x28, 0x14, 0x14, 0x3C, 0x14, 0x14, 0x14};
    if (SH_CALL(Flags_Test)(At(at::kTallyBits), 4) != 0 && SH_CALL(Flags_Test)(At(at::kTallyBits + 3), 4) == 0) {
        const unsigned char v = B(at::kTallyByte);
        if (v < 0x1E) B(at::kTallyByte) = static_cast<unsigned char>(v + 1);
    }
    if (Cond_ByteFA >= 8 && B(at::kLevelA) != 0) {
        U flag = 0xF6;
        bool found = false;
        for (; flag <= 0xFF; ++flag)
            if (SH_CALL(Flags_Test)(At(at::kStoryFlags), flag) == 0) {
                found = true;
                break;
            }
        if (found) {
            unsigned char a = 0, b = 0;
            for (U p = at::kPairs + 1u; p < at::kPairsEnd + 1u; p += 8) {
                if (B(p - 1) == 0) continue;
                if (B(p) == 0xA) ++a;
                else if (B(p) == 0xB) ++b;
            }
            const bool first = flag == 0xF6 || flag == 0xF9 || flag == 0xFC;
            if (first) b = a;
            if (b != 0) {
                SetL(at::kTallyDword, L(at::kTallyDword) + 1u);
                int need = kThresholds[flag - 0xF6] - static_cast<int>(B(first ? at::kBiasA : at::kBiasB));
                if (need < 5) need = 5;
                if (L(at::kTallyDword) >= static_cast<U>(need)) {
                    SH_CALL(Flags_Set)(At(at::kStoryFlags), flag);
                    SetL(at::kTallyDword, 0);
                }
            }
        }
        if (B(at::kLevelB) < 7) SH_CALL(Records_CountUnpaired)(1, reinterpret_cast<U*>(At(at::kTallyA)));
        if (B(at::kLevelA) < 0xA) SH_CALL(Records_CountUnpaired)(0, reinterpret_cast<U*>(At(at::kTallyB)));
        SH_CALL(Records_CountUnpairedOfKind)(5);
        SH_CALL(Records_CountUnpairedOfKind)(0xB);
    }
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0xF5) == 0 && B(at::kLevelB) >= 4)
        SH_CALL(Flags_Set)(At(at::kStoryFlags), 0xF5);
}

// original 0x534480: Field_LeaderStates[14] and the members' 0x65F960[14]:
// unless Field_Request is 9, state +1 = 1. Else the facing +8 toward the
// mode-11 object (0x905DA0's +0x34 / +0x38) by the signs of dx, dz: (-,-) 0,
// (+,+) 4, (-,+) 6, (-,0) 7, (0,+) 5, (0,-) 1, (+,-) 2, (+,0) 3, (0,0) kept;
// then Sprite_EnsureAnimation(+8). PSX twin 0x801C32D0 (table-anchored, a
// hypothesis).
extern "C" void __cdecl Field_FaceMode11Object(void) {
    if (Field_Request != 9) {
        Sprite_Current[1] = 1;
        return;
    }
    unsigned char* const s = Sprite_Current;
    const auto dx = static_cast<std::int32_t>(L(at::kObject + 0x34) - LongAt(s + 0x34));
    const auto dz = static_cast<std::int32_t>(L(at::kObject + 0x38) - LongAt(s + 0x38));
    int facing = -1;
    if (dx < 0) facing = dz < 0 ? 0 : dz > 0 ? 6 : 7;
    else if (dx > 0) facing = dz > 0 ? 4 : dz < 0 ? 2 : 3;
    else if (dz > 0) facing = 5;
    else if (dz < 0) facing = 1;
    if (facing >= 0) s[8] = static_cast<unsigned char>(facing);
    Ensure(Sprite_Current[8]);
}

// original 0x534C20: the floor's damage of kind k (0..8; Field_FloorDamage's
// cell code - 0x80). A 27-byte table on the stack, three bytes a kind:
// Char_LoseHp(t0, Field_State +0x89) - HP by 4, 8, 16, 32, 2 for kinds 0..4 -
// and 0x537500(t1, +0x89) - AP by 4, 8, 32 for kinds 6..8 -, then the flags
// t2: bit 7 Field_State +0x124 = 10, bit 5 +0x125 = 40 (kinds 4 and 5 carry
// 0x80 and 0x20). The actor Field_State +0x148 names: with HP (+0x18) at most
// a quarter of +0x20, its state word +0x10 and Field_State +0x90 |= 0x2000;
// its state word |= t2; Actor_EquipCount(actor, 3, 8) not 0: its state word
// &= ~0x20 and Field_State +0x125 = 0. Both HP calls push stale upper bytes
// (Char_LoseHp and 0x537500 read a word and a byte). A kind above 8 reads
// past the stack table: ours aborts. PSX twin 0x801C3E2C (gap20, a
// hypothesis).
extern "C" void __cdecl Field_FloorHurt(unsigned kind_arg) {
    static const unsigned char kTable[27] = {4, 0, 0,  8, 0, 0,  0x10, 0, 0,  0x20, 0, 0,  2, 0, 0x80,
                                             0, 0, 0x20,  0, 4, 0,  0, 8, 0,  0, 0x20, 0};
    const U kind = kind_arg & 0xFF;
    if (kind > 8) bof3::Fatal("Field_FloorHurt: kind %u past the nine the stack table holds", (unsigned)kind);
    const unsigned char* const t = kTable + 3u * kind;
    SH_CALL(Char_LoseHp)(t[0], Field_State[0x89]);
    SH_AT(void (__cdecl*)(unsigned, unsigned), at::kHpLoss2)(t[1], Field_State[0x89]);
    const unsigned char flags = t[2];
    if (flags & 0x80) Field_State[0x124] = 0xA;
    if (flags & 0x20) Field_State[0x125] = 0x28;
    unsigned char* fs = Field_State;
    U actor = at::kCharRecords + fs[0x148] * at::kRecordStride;
    if (W(actor + 0x18) <= (W(actor + 0x20) >> 2)) {
        SetW(actor + 0x10, W(actor + 0x10) | 0x2000u);
        SetW(Key(fs) + 0x90, W(Key(fs) + 0x90) | 0x2000u);
        fs = Field_State;
    }
    const U id = fs[0x148];
    actor = at::kCharRecords + id * at::kRecordStride;
    SetW(actor + 0x10, W(actor + 0x10) | flags);
    // the member byte as the original pushes it: edx of `lea edx, [id + id * 4]` with dl the byte
    if (SH_CALL(Actor_EquipCount)(((id * 5u) & 0xFFFFFF00u) | id, 3, 8) == 0) return;
    unsigned char* const f = Field_State;
    const U a = at::kCharRecords + f[0x148] * at::kRecordStride;
    SetW(a + 0x10, W(a + 0x10) & 0xFFDFu);
    f[0x125] = 0;
}

// The map's cell byte at (x, z), whole words (AreaMap_ByteAt reads them as
// s16s), each and 0xF0 when `mask` (a byte) is set.
namespace {
unsigned char Cell(U x, U z) { return SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z)); }
unsigned char Masked(unsigned char c, U mask) { return (mask & 0xFF) != 0 ? static_cast<unsigned char>(c & 0xF0) : c; }
// AreaMap_CellsNoneWide's filter: a 0x2n cell made 0x2n & 0x21.
unsigned char Doors(unsigned char c) { return (c & 0xF0) == 0x20 ? static_cast<unsigned char>(c & 0x21) : c; }
}  // namespace

// original 0x535490: AreaMap_CellsAll's other footprint (its `wide` not 0):
// al 1 when every cell under the 16.16 point equals `code` (a byte) - the
// four at (x, z) .. (x + 1, z + 1), and with no x fraction the two at x - 1,
// with no z fraction the two at z - 1, with neither (x - 1, z - 1) - each and
// 0xF0 when `mask` is set; else 0. PSX twin 0x801C4B70 by the body (psx_pair
// has it and AreaMap_CellsAll4 swapped; a hypothesis).
extern "C" unsigned char __cdecl AreaMap_CellsAllWide(long x, long z, unsigned code, unsigned mask) {
    const U xi = static_cast<U>(x) >> 16, zi = static_cast<U>(z) >> 16;
    const auto c = static_cast<unsigned char>(code);
    unsigned char b[4] = {Cell(xi, zi), Cell(xi + 1, zi), Cell(xi, zi + 1), Cell(xi + 1, zi + 1)};
    for (unsigned char& v : b) v = Masked(v, mask);
    if (b[0] != c || b[1] != c || b[2] != c || b[3] != c) return 0;
    if ((x & 0xFFFF) == 0) {
        const unsigned char p = Masked(Cell(xi - 1, zi), mask), q = Masked(Cell(xi - 1, zi + 1), mask);
        if (p != c || q != c) return 0;
    }
    if ((z & 0xFFFF) == 0) {
        const unsigned char p = Masked(Cell(xi, zi - 1), mask), q = Masked(Cell(xi + 1, zi - 1), mask);
        if (p != c || q != c) return 0;
    }
    if ((x & 0xFFFF) == 0 && (z & 0xFFFF) == 0 && Masked(Cell(xi - 1, zi - 1), mask) != c) return 0;
    return 1;
}

// original 0x535D60: AreaMap_CellsNone's other footprint: al 0 when any cell
// under the point equals `code` - the same cells as AreaMap_CellsAllWide,
// each 0x2n made 0x2n & 0x21 first (not the corner (x - 1, z - 1)), then and
// 0xF0 with `mask`; else 1. PSX twin 0x801C58F4 by the body (a hypothesis).
extern "C" unsigned char __cdecl AreaMap_CellsNoneWide(long x, long z, unsigned code, unsigned mask) {
    const U xi = static_cast<U>(x) >> 16, zi = static_cast<U>(z) >> 16;
    const auto c = static_cast<unsigned char>(code);
    unsigned char b[4] = {Cell(xi, zi), Cell(xi + 1, zi), Cell(xi, zi + 1), Cell(xi + 1, zi + 1)};
    for (unsigned char& v : b) v = Masked(Doors(v), mask);
    if (b[0] == c || b[1] == c || b[2] == c || b[3] == c) return 0;
    if ((x & 0xFFFF) == 0) {
        const unsigned char p = Masked(Doors(Cell(xi - 1, zi)), mask);
        const unsigned char q = Masked(Doors(Cell(xi - 1, zi + 1)), mask);
        if (p == c || q == c) return 0;
    }
    if ((z & 0xFFFF) == 0) {
        const unsigned char p = Masked(Doors(Cell(xi, zi - 1)), mask);
        const unsigned char q = Masked(Doors(Cell(xi + 1, zi - 1)), mask);
        if (p == c || q == c) return 0;
    }
    if ((x & 0xFFFF) == 0 && (z & 0xFFFF) == 0 && Masked(Cell(xi - 1, zi - 1), mask) == c) return 0;
    return 1;
}

// original 0x535830: Field_WayBlocked for a raised sprite: al 1 when
// Field_CellsBlock(x, z, 1) answers; or when a slope probe MapView_SlopeAt
// answers above 0x40 (s16) with DamageScratch's byte 0x903850 set after it -
// the points and directions, with xi / zi the whole cells: (xi.8, zi.8) 4,
// (xi.8, zi.0) 3, (xi.0, zi.8) 5; with no x fraction (xi-1.8, zi.0) 7,
// (xi-1.8, zi+1.0) 7, (xi.8, zi+1.0) 7, (xi-1.0, zi.8) 1, (xi+1.0, zi.8) 1;
// with no z fraction (xi.0, zi-1.8) 1, (xi+1.0, zi-1.8) 1, (xi+1.0, zi.8) 1,
// (xi.8, zi-1.0) 3, (xi.8, zi+1.0) 3; with neither (xi-1.8, zi-1.8) 0; or
// when MapView_GroundAt(x, z) is more than 0xC0 from `ground` (s16s); else
// Sprite_ObjectAt(x, z, 1) with Sprite_Current's height +0x3E set to `ground`
// for the call (put back after, through Sprite_Current read again) - al 1
// unless it answers 0xFF. The probe points are built as the original builds
// them, word by word into two dwords. PSX twin 0x801C5038 (call, a
// hypothesis).
extern "C" unsigned char __cdecl Field_WayBlockedWide(long x, long z, unsigned ground) {
    if (SH_CALL(Field_CellsBlock)(x, z, 1) != 0) return 1;
    const U xi = static_cast<U>(x) >> 16, zi = static_cast<U>(z) >> 16;
    // The probe's two dwords: y (lo, hi) and x (lo, hi), words as the stack holds them.
    struct { std::uint16_t ylo, yhi, xlo, xhi; } q;
    auto probe = [&](unsigned direction) {
        const U px = q.xlo | static_cast<U>(q.xhi) << 16, py = q.ylo | static_cast<U>(q.yhi) << 16;
        const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(px), static_cast<long>(py), direction);
        return S16(static_cast<U>(slope)) > 0x40 && B(at::kEffectSlot) != 0;
    };
    q.yhi = static_cast<std::uint16_t>(zi);
    q.ylo = 0x8000;
    q.xhi = static_cast<std::uint16_t>(xi);
    q.xlo = 0x8000;
    if (probe(4)) return 1;
    q.ylo = 0;
    if (probe(3)) return 1;
    q.ylo = 0x8000;
    q.xlo = 0;
    if (probe(5)) return 1;
    if ((x & 0xFFFF) == 0) {
        q.ylo = 0;
        q.yhi = static_cast<std::uint16_t>(zi);
        q.xhi = static_cast<std::uint16_t>(xi - 1);
        q.xlo = 0x8000;
        if (probe(7)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi + 1);
        if (probe(7)) return 1;
        q.xhi = static_cast<std::uint16_t>(xi);
        if (probe(7)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi);
        q.ylo = 0x8000;
        q.xhi = static_cast<std::uint16_t>(xi - 1);
        q.xlo = 0;
        if (probe(1)) return 1;
        q.xhi = static_cast<std::uint16_t>(xi + 1);
        if (probe(1)) return 1;
    }
    if ((z & 0xFFFF) == 0) {
        q.ylo = 0x8000;
        q.yhi = static_cast<std::uint16_t>(zi - 1);
        q.xlo = 0;
        q.xhi = static_cast<std::uint16_t>(xi);
        if (probe(1)) return 1;
        q.xhi = static_cast<std::uint16_t>(xi + 1);
        if (probe(1)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi);
        if (probe(1)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi - 1);
        q.ylo = 0;
        q.xhi = static_cast<std::uint16_t>(xi);
        q.xlo = 0x8000;
        if (probe(3)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi + 1);
        if (probe(3)) return 1;
    }
    if ((x & 0xFFFF) == 0 && (z & 0xFFFF) == 0) {
        q.yhi = static_cast<std::uint16_t>(zi - 1);
        q.ylo = 0x8000;
        q.xhi = static_cast<std::uint16_t>(xi - 1);
        q.xlo = 0x8000;
        if (probe(0)) return 1;
    }
    const long g = SH_CALL(MapView_GroundAt)(x, z);
    int d = S16(static_cast<U>(g)) - S16(ground);
    if (d < 0) d = -d;
    if (d > 0xC0) return 1;
    unsigned char* const s = Sprite_Current;
    const std::uint16_t kept = W(Key(s) + 0x3E);
    SetW(Key(s) + 0x3E, ground);
    const unsigned char hit = SH_CALL(Sprite_ObjectAt)(x, z, 1);
    SetW(Key(Sprite_Current) + 0x3E, kept);
    return hit != 0xFF ? 1 : 0;
}

// ============================================================================
// The leader's hop and step helpers (FC3's leader states 0x525CE0..0x525F90
// and FE1's 0x52F970..0x52FB50 call them; PSX twins 0x801C5D14..0x801C66DC,
// pairs_propagated "call", each a hypothesis). Each works on Sprite_Current
// and reads it again after every call, as the originals do.
// ============================================================================

// original 0x535FC0: Sprite_EnsureAnimation(0x3C) facing 7, else (0x3D).
extern "C" void __cdecl Leader_Pose3C(void) { Ensure(Sprite_Current[8] == 7 ? 0x3C : 0x3D); }

// The hop's set-up, 0x535FE0 and 0x5363C0 alike: Field_State +0x128 = +0x70
// + 2, Field_JumpSetUp; +0x14 = 0; +9 = (+0x70 + 1) * +9 * 2 and +0xA = +9 / 2
// (bytes); Sprite_EnsureAnimation(+8 + 8), Sprite_ApplyVelocity.
namespace {
void HopSetUp() {
    Field_State[0x128] = static_cast<unsigned char>(Sprite_Current[0x70] + 2);
    SH_CALL(Field_JumpSetUp)();
    SetL(Key(Sprite_Current) + 0x14, 0);
    unsigned char* s = Sprite_Current;
    s[9] = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(s[0x70] + 1) * s[9]) << 1);
    s = Sprite_Current;
    s[0xA] = static_cast<unsigned char>(s[9] >> 1);
    Ensure(static_cast<unsigned char>(Sprite_Current[8] + 8));
    SH_CALL(Sprite_ApplyVelocity)();
}
}  // namespace

// original 0x535FE0: the hop's set-up (HopSetUp), +9 - 1.
extern "C" void __cdecl Leader_HopStart(void) {
    HopSetUp();
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
}

// original 0x5363C0: nothing (al 0) until Sprite_ScriptTickOnce answers; then
// the hop's set-up, +9 - 1 and al 1.
extern "C" unsigned char __cdecl Leader_HopStartAfterTick(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return 0;
    HopSetUp();
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
    return 1;
}

// original 0x536050: the hop in flight. +9 at 0: Sprite_EnsureAnimation(+8)
// (the record read on entry), +0xA = 4, Sprite_ScriptTick, al 1. Else +9 - 1,
// Sprite_ApplyVelocity, and at +9 == +0xA (half way) the member slot's CLUT
// word 0x8113BE + +5 * 0x40 = 0 with Gfx_ClutStripDirty; Sprite_ScriptTick,
// al 0.
extern "C" unsigned char __cdecl Leader_HopFlight(void) {
    unsigned char* const s = Sprite_Current;
    if (s[9] == 0) {
        Ensure(s[8]);
        Sprite_Current[0xA] = 4;
        SH_CALL(Sprite_ScriptTick)();
        return 1;
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    SH_CALL(Sprite_ApplyVelocity)();
    unsigned char* const t = Sprite_Current;
    if (t[9] == t[0xA]) {
        Gfx_ClutStripDirty = 1;
        SetW(ClutWord(t), 0);
    }
    SH_CALL(Sprite_ScriptTick)();
    return 0;
}

// original 0x5360C0: +0xA - 1; at 0: Sprite_EnsureAnimation(0x37 facing 3,
// else 0x34), the facing +8 ^= 4, al 1. Else Sprite_EnsureAnimation(7 - +0xA
// facing 3, else +0xA + 1), +9 = 1, al 0.
extern "C" unsigned char __cdecl Leader_HopPose(void) {
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    const unsigned char* const s = Sprite_Current;
    const unsigned char left = s[0xA];
    if (left == 0) {
        Ensure(s[8] == 3 ? 0x37 : 0x34);
        Sprite_Current[8] = static_cast<unsigned char>(Sprite_Current[8] ^ 4);
        return 1;
    }
    Ensure(s[8] == 3 ? static_cast<unsigned char>(7 - left) : static_cast<unsigned char>(left + 1));
    Sprite_Current[9] = 1;
    return 0;
}

// original 0x536130: nothing (al 0) until Sprite_ScriptTickOnce answers; then
// Sprite_EnsureAnimation(0x3E facing 7, else 0x3F), al 1.
extern "C" unsigned char __cdecl Leader_Pose3E(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return 0;
    Ensure(Sprite_Current[8] == 7 ? 0x3E : 0x3F);
    return 1;
}

// original 0x536170: a step up. The point ahead (Field_DirectionSteps[+8] *
// (+0x70 + 2) from +0x34 / +0x38) and the ground there (MapView_GroundAt)
// less the actor's offset (the s16 0x66978C[Field_State +0x89], the actor as
// read on entry): with the height +0x3E (as read on entry, s16) above it, the
// height - the rise 0x6697A4[actor] into the cell read on entry, al 0; else
// the ground again (MapView_GroundAt a second time) less the offset of
// Field_State +0x89 read again into +0x3E of Sprite_Current read again,
// Sprite_EnsureAnimation(0x3C facing 7, else 0x3D), al 1. Then the view and
// the script tick (FollowAndTick).
extern "C" unsigned char __cdecl Leader_StepUp(void) {
    unsigned char* const s = Sprite_Current;
    std::int32_t x, z;
    Ahead(s, x, z);
    const std::uint16_t height = W(Key(s) + 0x3E);
    const U actor = Field_State[0x89];
    unsigned char result = 0;
    const long ground = SH_CALL(MapView_GroundAt)(x, z);
    if (S16(height) > S16(static_cast<U>(ground)) - S16(W(at::kPaceHeights + actor * 2u))) {
        SetW(Key(s) + 0x3E, static_cast<unsigned>(height) - B(at::kPaceRises + actor));
    } else {
        const long again = SH_CALL(MapView_GroundAt)(x, z);
        const std::uint16_t offset = W(at::kPaceHeights + Field_State[0x89] * 2u);
        SetW(Key(Sprite_Current) + 0x3E, static_cast<U>(again) - offset);
        Ensure(Sprite_Current[8] == 7 ? 0x3C : 0x3D);
        result = 1;
    }
    FollowAndTick(Sprite_Current);
    return result;
}

// original 0x536290: the member slot's CLUT word 0x8113BE + +5 * 0x40 = 0 with
// Gfx_ClutStripDirty, then Sprite_EnsureAnimation(0x37 facing 7, else 0x34).
extern "C" void __cdecl Leader_Pose34(void) {
    unsigned char* const s = Sprite_Current;
    Gfx_ClutStripDirty = 1;
    SetW(ClutWord(s), 0);
    Ensure(s[8] == 7 ? 0x37 : 0x34);
}

// original 0x5362D0: a step down. The point ahead as Leader_StepUp's and the
// ground there: with the height +0x3E (as read on entry) below it (s16), the
// height + the rise 0x6697A4[Field_State +0x89 read after the call] into the
// cell read on entry, al 0; else the ground again into +0x3E of
// Sprite_Current read again, Sprite_EnsureAnimation(0x3E facing 7, else
// 0x3F), al 1. Then FollowAndTick.
extern "C" unsigned char __cdecl Leader_StepDown(void) {
    unsigned char* const s = Sprite_Current;
    std::int32_t x, z;
    Ahead(s, x, z);
    const std::uint16_t height = W(Key(s) + 0x3E);
    unsigned char result = 0;
    const long ground = SH_CALL(MapView_GroundAt)(x, z);
    if (S16(height) < S16(static_cast<U>(ground))) {
        SetW(Key(s) + 0x3E, B(at::kPaceRises + Field_State[0x89]) + static_cast<unsigned>(height));
    } else {
        const long again = SH_CALL(MapView_GroundAt)(x, z);
        SetW(Key(Sprite_Current) + 0x3E, static_cast<U>(again));
        Ensure(Sprite_Current[8] == 7 ? 0x3E : 0x3F);
        result = 1;
    }
    FollowAndTick(Sprite_Current);
    return result;
}

// original 0x536440: the hop's fall. +9 (the record read on entry) at 0:
// Sprite_EnsureAnimation(+8), Sprite_ClearSteps, Field_State +0x137 = 0, the
// states +1 = 1, +2 = +3 = +4 = 0, al 1. Else +9 - 1, Sprite_ApplyVelocity,
// at +9 == +0xA the member's palette (Sprite_LoadPalette(0x80D380 + +5 *
// 0x40, 0)), Sprite_ScriptTick, al 0.
extern "C" unsigned char __cdecl Leader_HopFall(void) {
    unsigned char* const s = Sprite_Current;
    if (s[9] == 0) {
        Ensure(s[8]);
        SH_CALL(Sprite_ClearSteps)();
        Field_State[0x137] = 0;
        Sprite_Current[1] = 1;
        Sprite_Current[2] = 0;
        Sprite_Current[3] = 0;
        Sprite_Current[4] = 0;
        return 1;
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    SH_CALL(Sprite_ApplyVelocity)();
    const unsigned char* const t = Sprite_Current;
    if (t[9] == t[0xA]) SH_CALL(Sprite_LoadPalette)(Palette(t), 0);
    SH_CALL(Sprite_ScriptTick)();
    return 0;
}

// original 0x5364D0: the height +0x3E up 0x10; the ground ahead (as
// Leader_StepUp's point) less the offset of Field_State +0x89: with the
// height at or above it (s16), the state +2 = 3. Then a tail jump to
// Sprite_ScriptTick, whose al it answers.
extern "C" unsigned char __cdecl Leader_Rise(void) {
    SetW(Key(Sprite_Current) + 0x3E, W(Key(Sprite_Current) + 0x3E) + 0x10u);
    std::int32_t x, z;
    Ahead(Sprite_Current, x, z);
    const long ground = SH_CALL(MapView_GroundAt)(x, z);
    const int floor = S16(static_cast<U>(ground)) - S16(W(at::kPaceHeights + Field_State[0x89] * 2u));
    unsigned char* const s = Sprite_Current;
    if (S16(W(Key(s) + 0x3E)) >= floor) s[2] = 3;
    return SH_CALL(Sprite_ScriptTick)();
}

// original 0x536550: the height +0x3E down 0x10; with the ground under
// (+0x34, +0x38) above it (s16): +0x3E = the ground, Sprite_SetAnimation(+8),
// the member's palette, Field_State +0x137 = 0, the states +1 = 1, +2 = 0.
// Then a tail jump to Sprite_ScriptTick, whose al it answers.
extern "C" unsigned char __cdecl Leader_Sink(void) {
    SetW(Key(Sprite_Current) + 0x3E, W(Key(Sprite_Current) + 0x3E) - 0x10u);
    unsigned char* s = Sprite_Current;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(LongAt(s + 0x34)), static_cast<long>(LongAt(s + 0x38)));
    s = Sprite_Current;
    if (S16(static_cast<U>(ground)) > S16(W(Key(s) + 0x3E))) {
        SetW(Key(s) + 0x3E, static_cast<U>(ground));
        SH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
        SH_CALL(Sprite_LoadPalette)(Palette(Sprite_Current), 0);
        Field_State[0x137] = 0;
        Sprite_Current[1] = 1;
        Sprite_Current[2] = 0;
    }
    return SH_CALL(Sprite_ScriptTick)();
}

// original 0x5365D0: the facing +8 ^= 4, Sprite_EnsureAnimation(0x3A facing
// 3, else 0x3B), the member's palette, Sprite_ClearSteps, +0x20 = -8, the
// states +1 = 2, +2 = 3, +3 = 3.
extern "C" void __cdecl Leader_TurnBack(void) {
    Sprite_Current[8] = static_cast<unsigned char>(Sprite_Current[8] ^ 4);
    Ensure(Sprite_Current[8] == 3 ? 0x3A : 0x3B);
    SH_CALL(Sprite_LoadPalette)(Palette(Sprite_Current), 0);
    SH_CALL(Sprite_ClearSteps)();
    SetL(Key(Sprite_Current) + 0x20, 0xFFFFFFF8u);
    Sprite_Current[1] = 2;
    Sprite_Current[2] = 3;
    Sprite_Current[3] = 3;
}

// ============================================================================
// The party-set error screen
// ============================================================================

// original 0x536A60: PartySet_Find's end when no row holds the three ids -
// never returns: every frame the title line (0x660C90) at (0xA0, 0x64) and
// the three ids PartySet_Find left at 0x903850..0x903852, formatted by the
// string at 0x660C7C into the text scratch 0x904BA0, at (0xA0, 0x74) - both
// Text_DrawFont8, colour 0 -, then Task_Sleep(1). The two low ids come from
// one dword read after the third's byte. PSX twin 0x8016776C (callers, a
// hypothesis).
extern "C" void __cdecl PartySet_ErrorLoop(void) {
    for (;;) {
        SH_CALL(Text_DrawFont8)(0xA0, 0x64, 0, At(at::kErrorTitle));
        const U third = B(at::kErrorIds + 2);
        const U ids = L(at::kErrorIds);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(at::kTextScratch)), reinterpret_cast<const char*>(At(at::kErrorFormat)),
                             ids & 0xFF, (ids >> 8) & 0xFF, third);
        SH_CALL(Text_DrawFont8)(0xA0, 0x74, 0, At(at::kTextScratch));
        SH_CALL(Task_Sleep)(1);
    }
}

// ============================================================================
// The mode-11 object (0x905DA0; Game_Mode 11, which Field_Request 9 enters,
// runs Mode11_ObjectFrame then 0x5172C0 each frame: docs/mode-tasks.md)
// ============================================================================

// original 0x536B60: Field_InputHeld = the word Input_Held, Sprite_Current =
// the object 0x905DA0, Field_State = ObjTrio's first record, then a jump
// through Mode11_ObjectStates 0x660C2C by its state +1 (4 entries,
// unchecked: past them ours aborts where the original jumps into data).
// Called by mode 11's step GameMode11_Frame 0x496790. PSX twin 0x80167CBC (call, a
// hypothesis).
extern "C" void __cdecl Mode11_ObjectFrame(void) {
    const std::uint16_t held = W(0x7E1BE8);
    const U state = B(at::kObject + 1);
    Sprite_Current = At(at::kObject);
    Field_State = ObjTrio;
    SetW(at::kInputHeld, held);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAt(at::kObjectStates, state, "Mode11_ObjectFrame")))();
}

// original 0x536B90: state 0: +5 = 0, +0xB = the leader's flag byte
// (0x802E78), Field_State +0x128 = 3, the position +0x34 / +0x38 / +0x3C from
// the leader's (0x802D74..), the state +1 up by one. Every store through
// Sprite_Current read again.
extern "C" void __cdecl Mode11_ObjectStart(void) {
    Sprite_Current[5] = 0;
    Sprite_Current[0xB] = B(at::kLeaderFlags);
    Field_State[0x128] = 3;
    SetL(Key(Sprite_Current) + 0x34, L(at::kLeaderX));
    SetL(Key(Sprite_Current) + 0x38, L(at::kLeaderZ));
    SetL(Key(Sprite_Current) + 0x3C, L(at::kLeaderY));
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
}

namespace {
// The object stops: its velocity +0xC / +0x10 / +0x14 zero, with `steps` +9
// too, and the two script words 0 (MoveScript_F3Divisor, MoveScript_FAWord).
void ObjectStill(unsigned char* s, bool steps) {
    SetL(Key(s) + 0xC, 0);
    SetL(Key(Sprite_Current) + 0x10, 0);
    SetL(Key(Sprite_Current) + 0x14, 0);
    if (steps) Sprite_Current[9] = 0;
    SetW(at::kF3Divisor, 0);
    SetW(at::kFaWord, 0);
}
void LeaveRequest() {
    SetW(at::kScriptFlags2, W(at::kScriptFlags2) & 0x1Fu);
    Field_Request = 9;
}
}  // namespace

// original 0x536BF0: state 1, the object's control. With Input_Pressed bit
// 11: the camera's kind-2 point Field_Kind2X / Z moved to Field_State's
// +0x34 / +0x38, MoveScript_F3Divisor 0x20, n = the larger of the two
// distances moved >> 13 (a byte): MoveScript_FAWord = (the s16 +0x3E -
// MapView_Elevation) / n (C division; 0 when n is 0), Field_State +0x128 = 3,
// the object's +0 |= 0x40 and state 3. Else with Field_InputHeld &
// 0x903586: +0x128 = 3, Game_Step up by one, states 1 and 0. Else with no
// direction held (Field_LeaderDirection al 0): +0x128 = 3, the object still,
// state 1. Else when Field_LeaderStepTarget answers: +0x128 = 3, +9 = 0,
// state 1, the leave request (Field_ScriptFlags2 &= 0x1F, Field_Request 9).
// Else, within 0x60000 of the leader in x and z (the step ahead of the
// object subtracted) and Field_Request not 5: the pace +0x128 4 when the run
// buttons 0x903582 held differ from the toggle 0x903A5E, else 3;
// Field_JumpStart, Field_JumpCheckHeight, +9 - 1, Field_LeaderStepTick,
// state 2 and the leave request; farther: the object still, state 1, the
// leave request. PSX twin none paired.
extern "C" void __cdecl Mode11_ObjectControl(void) {
    if (L(at::kInputPressed) & 0x800u) {
        unsigned char* const fs = Field_State;
        const U nx = LongAt(fs + 0x34);
        const std::int32_t dx = AbsDiff(L(at::kKind2X), nx);
        const std::int32_t dz = AbsDiff(L(at::kKind2Z), LongAt(fs + 0x38));
        SetL(at::kKind2X, nx);
        SetL(at::kKind2Z, LongAt(fs + 0x38));
        SetW(at::kF3Divisor, 0x20);
        const std::int32_t far = dx >= dz ? dx : dz;
        const U n = static_cast<U>(far >> 13) & 0xFF;
        U word = 0;
        if (n != 0) word = static_cast<U>(S16(static_cast<U>(W(Key(fs) + 0x3E) - W(at::kElevation))) / static_cast<int>(n));
        SetW(at::kFaWord, word);
        fs[0x128] = 3;
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
        Sprite_Current[1] = 3;
        return;
    }
    if (W(at::kInputHeld) & W(at::kButtonsLeave)) {
        Field_State[0x128] = 3;
        SetW(at::kGameStep, W(at::kGameStep) + 1u);
        Sprite_Current[1] = 1;
        Sprite_Current[2] = 0;
        return;
    }
    if (SH_CALL(Field_LeaderDirection)() == 0) {
        Field_State[0x128] = 3;
        ObjectStill(Sprite_Current, true);
        Sprite_Current[1] = 1;
        return;
    }
    if (SH_CALL(Field_LeaderStepTarget)() != 0) {
        Field_State[0x128] = 3;
        Sprite_Current[9] = 0;
        Sprite_Current[1] = 1;
        LeaveRequest();
        return;
    }
    unsigned char* const s = Sprite_Current;
    const U dir = s[8];
    bool near = AbsDiff(L(at::kLeaderX) - L(at::kDirectionSteps + dir * 8u), LongAt(s + 0x34)) <= 0x60000;
    if (near)
        near = AbsDiff(L(at::kLeaderZ) - L(at::kDirectionSteps + 4u + dir * 8u), LongAt(s + 0x38)) <= 0x60000 && Field_Request != 5;
    if (!near) {
        ObjectStill(s, true);
        Sprite_Current[1] = 1;
        LeaveRequest();
        return;
    }
    const U running = (W(at::kButtonsRun) & W(at::kInputHeld)) != 0 ? 1u : 0u;
    Field_State[0x128] = (running ^ B(at::kRunDefault)) != 0 ? 4 : 3;
    SH_CALL(Field_JumpStart)();
    SH_CALL(Field_JumpCheckHeight)();
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
    SH_CALL(Field_LeaderStepTick)();
    Sprite_Current[1] = 2;
    LeaveRequest();
}

// original 0x536EC0 (a start of its own, not in the cut: 0x536E70 jumps to it
// from inside 0x536E90's extent): the object's velocity and the two script
// words 0 (not +9), Field_State +0x128 = 3; with a direction held
// (Field_InputHeld & 0xF000) a tail jump to Mode11_ObjectControl, else
// state 1.
extern "C" void __cdecl Mode11_ObjectHalt(void) {
    ObjectStill(Sprite_Current, false);
    Field_State[0x128] = 3;
    if (L(at::kInputHeld) & 0xF000u) {
        SH_CALL(Mode11_ObjectControl)();
        return;
    }
    Sprite_Current[1] = 1;
}

// original 0x536E70: state 2: +9 at 0, a tail jump to Mode11_ObjectHalt;
// else +9 - 1 and a tail jump to Field_LeaderStepTick.
extern "C" void __cdecl Mode11_ObjectMove(void) {
    unsigned char* const s = Sprite_Current;
    if (s[9] == 0) {
        SH_CALL(Mode11_ObjectHalt)();
        return;
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    SH_CALL(Field_LeaderStepTick)();
}

// original 0x536E90: state 3: once Field_Kind2Hold is 0,
// MapView_SetElevation(Field_State's +0x3E) and Field_Request 0.
extern "C" void __cdecl Mode11_ObjectEnd(void) {
    if (B(at::kKind2Hold) != 0) return;
    SH_CALL(MapView_SetElevation)(static_cast<short>(W(Key(Field_State) + 0x3E)));
    Field_Request = 0;
}

namespace {
// x87 as the original has it (the game's control word 0x027F: round to
// nearest, 53 bits).
// `fild dword [&v]; fst dword [a]; fadd dword [c]; fstp dword [b]`
void FildFstFadd(std::int32_t v, unsigned char* a, const unsigned char* c, unsigned char* b) {
    __asm__ volatile(
        "fildl %[v]\n\t"
        "fsts (%[a])\n\t"
        "fadds (%[c])\n\t"
        "fstps (%[b])\n\t"
        :
        : [v] "m"(v), [a] "r"(a), [c] "r"(c), [b] "r"(b)
        : "st", "memory");
}
// `fild qword [lo, 0]; fstp dword [a]`
void Fild64Fstp(U lo, unsigned char* a) {
    std::uint64_t v = lo;
    __asm__ volatile(
        "fildll %[v]\n\t"
        "fstps (%[a])\n\t"
        :
        : [v] "m"(v), [a] "r"(a)
        : "st", "memory");
}
// `fld dword [a]; fadd dword [c]; fst dword [b]; fstp dword [d]`
void FldFaddFstFstp(const unsigned char* a, const unsigned char* c, unsigned char* b, unsigned char* d) {
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fadds (%[c])\n\t"
        "fsts (%[b])\n\t"
        "fstps (%[d])\n\t"
        :
        : [a] "r"(a), [c] "r"(c), [b] "r"(b), [d] "r"(d)
        : "st", "memory");
}
// `fld dword [a]` then the CRT's _ftol 0x5B9550 (round toward zero set in a
// copy of the control word for one fistp to 64 bits, the word put back): the
// low dword (area_w3g.cpp's Ftol).
U Ftol(const unsigned char* a) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// A draw's commit: MapView_LinkPrimAt at the object's position unless its
// +0xB bit 0, then Gfx_CommitPrim to slot 3.
void CommitAtObject(unsigned size) {
    const unsigned char* const s = Sprite_Current;
    if ((s[0xB] & 1) == 0)
        SH_CALL(MapView_LinkPrimAt)(LongAt(s + 0x34), LongAt(s + 0x38), 1, size);
    else
        SH_CALL(Gfx_CommitPrim)(3, size);
}
// A 16.16 coordinate as the draws place it: (v >> 9) - 0x4000, a word.
short Place(U v) { return static_cast<short>(static_cast<U>(static_cast<std::int32_t>(v) >> 9) - 0x4000u); }
}  // namespace

// original 0x536F10: the mode-11 object's draw, called by FC3's 0x5172C0 each
// frame of mode 11 (Sprite_Current = 0x905DA0 on every path). Nothing more
// when its +0 has bit 6. Else a sprite quad (POLY_FT4 at Gfx_PacketNext):
// the point (+0x34, +0x38, -(s16 +0x3E + 0x80) / 2) through Gte_RotTransPers
// (the depth to +0x60, the screen x and y through _ftol to +0x2E / +0x30),
// Gte_PrimDepthFlat4_10; the corners x = +0x2E - 6 and that + the float at
// 0x5C4254, y = +0x30 - 0x18 - a bob of (Frame_Counter >> 1) & 3 - or with
// Frame_Counter bit 3 +0x30 - 0x1B + the bob - (a dword loaded as a qword with
// a high half of 0) and that + the float at 0x5C41F0; fixed colours, texture
// and CLUT words; linked at the object (CommitAtObject, 0x48 bytes). Then
// under a pushed matrix a flat disc: the translation (+0x34, +0x38, -(s16
// +0x3E) / 2) through Gte_RotTrans into a matrix built by Gte_RotMatrix of
// (0, 0, 0) and Gte_MulMatrix0 with Camera_Matrix, set as rotation and
// translation; a draw mode (tpage 0x5E) committed at the object (0xC); 32
// POLY_G3 fans (angles 0, 0x80, .. 0xF80) of radius r = 0x32 + 2 * the bob
// (Frame_Counter bit 3) or (0x1C - the bob) * 2, through
// Prim_VertexScratch's three SVECTORs (centre 0, then (cos a * r >> 12, sin a
// * r >> 12, 0) and the same at a + 0x80) and Gte_RotTransPers3, semi-
// transparent, the centre 0x808080 and the rim black, each linked at the
// object (0x34 bytes). The vertex's and the translation's fourth words are 0
// (DIV-0023's ruling; the original leaves stale stack there). PSX twin
// 0x80168288 (callers, a hypothesis).
extern "C" void __cdecl Mode11_ObjectDraw(void) {
    const unsigned char flags = B(at::kObject);
    Sprite_Current = At(at::kObject);
    if (flags & 0x40) return;
    alignas(4) short vertex[4];
    vertex[0] = Place(L(at::kObject + 0x34));
    vertex[1] = Place(L(at::kObject + 0x38));
    vertex[2] = static_cast<short>(-((S16(W(at::kObject + 0x3E)) + 0x80) / 2));
    vertex[3] = 0;
    alignas(4) unsigned char screen[8];
    long depth, flag;
    const long z = SH_CALL(Gte_RotTransPers)(vertex, reinterpret_cast<unsigned long*>(screen), &depth);
    static_cast<void>(flag);
    SetL(Key(Sprite_Current) + 0x60, static_cast<U>(z));
    SetW(Key(Sprite_Current) + 0x2E, Ftol(screen));
    SetW(Key(Sprite_Current) + 0x30, Ftol(screen + 4));
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gte_PrimDepthFlat4_10)(prim);
    FildFstFadd(S16(W(Key(Sprite_Current) + 0x2E)) - 6, prim + 8, At(at::kQuadLift), prim + 0x18);
    std::memcpy(prim + 0x28, prim + 8, 4);
    std::memcpy(prim + 0x38, prim + 0x18, 4);
    U counter = Frame_Counter;
    const U bob = (counter >> 1) & 3;
    const U y = (counter & 8) ? bob + static_cast<U>(S16(W(Key(Sprite_Current) + 0x30))) - 0x1Bu
                              : static_cast<U>(S16(W(Key(Sprite_Current) + 0x30))) - bob - 0x18u;
    Fild64Fstp(y, prim + 0xC);
    std::memcpy(prim + 0x1C, prim + 0xC, 4);
    FldFaddFstFstp(prim + 0xC, At(at::kQuadWidth), prim + 0x2C, prim + 0x3C);
    prim[0x14] = 0xCC;
    prim[0x24] = 0xCC;
    prim[0x25] = 0x9B;
    prim[0x34] = 0xE4;
    prim[0x44] = 0xE4;
    prim[0x45] = 0x9B;
    prim[0x15] = 0xA7;
    prim[0x35] = 0xA7;
    prim[4] = 0x80;
    prim[5] = 0x80;
    prim[6] = 0x80;
    SetW(Key(prim) + 0x26, 0xF);
    SetW(Key(prim) + 0x16, 0x7802);
    CommitAtObject(0x48);
    SH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sprite_Current;
    alignas(4) short angles[4] = {0, 0, 0, 0};
    alignas(4) short move[4];
    move[0] = Place(LongAt(s + 0x34));
    move[1] = Place(LongAt(s + 0x38));
    move[2] = static_cast<short>(-(S16(W(Key(s) + 0x3E)) / 2));
    move[3] = 0;
    alignas(4) unsigned char matrix[0x20];
    SH_CALL(Gte_RotTrans)(move, reinterpret_cast<long*>(matrix + 0x14));
    SH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<short*>(matrix), reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<unsigned long*>(matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<unsigned long*>(matrix));
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x5E, 0);
    CommitAtObject(0xC);
    counter = Frame_Counter;
    const U wobble = (counter >> 1) & 3;
    const U radius = (counter & 8) ? wobble * 2u + 0x32u : (0x1Cu - wobble) * 2u;
    const std::int32_t r = static_cast<short>(radius);
    short* const v = Prim_VertexScratch;
    for (U a = 0; a < 0x1000; a += 0x80) {
        v[0] = 0;
        v[1] = 0;
        v[2] = 0;
        const U c0 = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        v[4] = static_cast<short>(static_cast<std::int32_t>(c0 * static_cast<U>(r)) >> 12);
        const U s0 = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        v[6] = 0;
        v[5] = static_cast<short>(static_cast<std::int32_t>(s0 * static_cast<U>(r)) >> 12);
        const U c1 = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a + 0x80)));
        v[8] = static_cast<short>(static_cast<std::int32_t>(c1 * static_cast<U>(r)) >> 12);
        const U s1 = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a + 0x80)));
        prim = Gfx_PacketNext;
        v[10] = 0;
        v[9] = static_cast<short>(static_cast<std::int32_t>(s1 * static_cast<U>(r)) >> 12);
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        long fan_depth;
        SH_CALL(Gte_RotTransPers3)(v, v + 4, v + 8, reinterpret_cast<float*>(prim + 8), reinterpret_cast<float*>(prim + 0x18),
                                   reinterpret_cast<float*>(prim + 0x28), &fan_depth);
        SH_CALL(Gte_PrimDepths3_10B)(prim);
        prim[4] = 0x80;
        prim[5] = 0x80;
        prim[6] = 0x80;
        prim[0x14] = 0;
        prim[0x15] = 0;
        prim[0x16] = 0;
        prim[0x24] = 0;
        prim[0x25] = 0;
        prim[0x26] = 0;
        CommitAtObject(0x34);
    }
    SH_CALL(Gte_PopMatrix)();
}

// ============================================================================
// The object triggers and the cell hook
// ============================================================================

// original 0x56E020: Field_ObjectTriggers' entry for the object's +0x86 (the
// table read from 0x662E1C: entry n is Field_ObjectTriggers[n - 1], entry 0
// WorldMap_FieldHooks' last), called with (object, the story flags
// 0x904030). Past the table ours aborts where the original calls data.
// PSX twin 0x801A96D4 (call, a hypothesis).
extern "C" void __cdecl Field_ObjectTriggerByKind(unsigned char* object) {
    const U entry = CodeAt(at::kTriggers, object[0x86], "Field_ObjectTriggerByKind");
    reinterpret_cast<void (__cdecl*)(unsigned char*, unsigned char*)>(static_cast<std::uintptr_t>(entry))(object, At(at::kStoryFlags));
}

// original 0x56D6B0: with the object's +0x89 bit 6, Field_ObjectTriggerByKind
// (object); else the chapter's vtable slot 1 (Scenario_Hooks[Cond_ByteFA]
// +4) with the object. Then +0x86 = 0xFF. Called by Field_ObjectIdle when
// +0x86 is not 0xFF, and by FE1's 0x52F8F0. PSX twin 0x801A8880 (call, a
// hypothesis).
extern "C" void __cdecl Field_ObjectTrigger(unsigned char* object) {
    if (object[0x89] & 0x40) {
        SH_CALL(Field_ObjectTriggerByKind)(object);
    } else {
        const U hooks = L(at::kScenarioHooks + 4u * static_cast<U>(static_cast<std::int32_t>(Cond_ByteFA)));
        const U entry = CodeAt(hooks, 1, "Field_ObjectTrigger");
        reinterpret_cast<void (__cdecl*)(unsigned char*)>(static_cast<std::uintptr_t>(entry))(object);
    }
    object[0x86] = 0xFF;
}

// original 0x56D7A0: the cell a leader faces, (x, z): the chapter's vtable
// slot 4 (Scenario_Hooks[Cond_ByteFA] +0x10), when it is not 0, with (x, z);
// its al not negative (movsx) answers 1. Else Area_CellHook(x, z), whose eax
// it answers. Sprite_Current, read on entry, is put back on both paths.
// Called by Field_LeaderTalkTest. PSX twin 0x801A8A3C (call-anchored, a
// hypothesis).
extern "C" int __cdecl Scenario_CellHook(unsigned x, unsigned z) {
    unsigned char* const kept = Sprite_Current;
    const U hooks = L(at::kScenarioHooks + 4u * static_cast<U>(static_cast<std::int32_t>(Cond_ByteFA)));
    const U hook = L(hooks + 0x10);
    if (hook != 0) {
        const U entry = CodeAt(hooks, 4, "Scenario_CellHook");
        const unsigned char answer = reinterpret_cast<unsigned char (__cdecl*)(unsigned, unsigned)>(static_cast<std::uintptr_t>(entry))(x, z);
        if (static_cast<signed char>(answer) >= 0) {
            Sprite_Current = kept;
            return 1;
        }
    }
    const int answer = SH_CALL(Area_CellHook)(x, z);
    Sprite_Current = kept;
    return answer;
}

// ============================================================================
// The field tail: seven of Field_ModeTailKinds' slots (Field_ModeTailRun
// jumps through them by the s8 0x9039F3 each field frame). Each is a state
// machine on the s8 0x9039F4 with an argument byte 0x9039F5; the area code
// arms them (kind, state 0, argument). A state out of range does nothing.
// No PSX twin paired for any.
// ============================================================================

namespace {
signed char TailState() { return static_cast<signed char>(B(at::kTailState)); }
void TailDone() {
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
    B(at::kTailArg) = 0;
}
void TailStep(unsigned char state) { B(at::kTailState) = state; }
}  // namespace

// original 0x56D930 (slot 4): 0 - Field_ScriptFlags2 |= 7, LoadDatFile(0x31C),
// the state up; 1 - once File_LoadDone, Gfx_ClutStripCopyRow(2), 0x585A00,
// the state up; 2 - a tail jump to 0x586670 (a dispatch on 0x9398CF); 3 -
// the party set 0x90412C |= 0x80 and Snd_LoadBankFile(0x2C2 + its low seven
// bits), the state up; 4 - once File_LoadDone, Field_ScriptFlags2 &= ~7,
// ScriptFlags_Clear40, the tail cleared; 5 - with Cond_ByteFE at 2, state 0.
extern "C" void __cdecl FieldTail_LoadBank(void) {
    switch (TailState()) {
    case 0:
        B(at::kScriptFlags2) = static_cast<unsigned char>(B(at::kScriptFlags2) | 7);
        SH_CALL(LoadDatFile)(0x31C);
        B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + 1);
        return;
    case 1:
        if (SH_CALL(File_LoadDone)() == 0) return;
        SH_CALL(Gfx_ClutStripCopyRow)(2);
        SH_AT(void (__cdecl*)(), at::kLoadWait)();
        B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + 1);
        return;
    case 2:
        SH_AT(void (__cdecl*)(), at::kLoadStep)();
        return;
    case 3: {
        const auto set = static_cast<unsigned char>(B(at::kPartySet) | 0x80);
        B(at::kPartySet) = set;
        SH_CALL(Snd_LoadBankFile)((set & 0x7Fu) + 0x2C2u);
        B(at::kTailState) = static_cast<unsigned char>(B(at::kTailState) + 1);
        return;
    }
    case 4:
        if (SH_CALL(File_LoadDone)() == 0) return;
        SetW(at::kScriptFlags2, W(at::kScriptFlags2) & 0xFFF8u);
        SH_CALL(ScriptFlags_Clear40)();
        TailDone();
        return;
    case 5:
        if (B(at::kCondByteFE) == 2) B(at::kTailState) = 0;
        return;
    default: return;
    }
}

// original 0x56DA10 (slot 5): 0 - unless a message is up (Field_Request 2),
// ScriptFlags_Set40 and state 1; 1 - Party_DropIn(0x662DE8[argument]), state
// 2; 2 - once the counter byte 0x90384B is 0x20, the timer 0x9039F6 = 0x10
// and state 3; 3 - once the timer counts down to 0, Field_ChangeArea by the
// argument (0 and 4: area 0x6C at (0x11, 0x59) facing 0x85; 1 and 2: 0x22 at
// (0x34, 0x43), 0x81; 3 and 5: 4 at (0x3E, 5), 0x81; above 5 none) and state
// 4; 4 - once the counter byte is 0x31, it 0, ScriptFlags_Clear40, story
// flags 0x1E and 0x1F cleared, the tail cleared.
extern "C" void __cdecl FieldTail_DropInMove(void) {
    switch (TailState()) {
    case 0:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Set40)();
        TailStep(1);
        return;
    case 1:
        SH_CALL(Party_DropIn)(B(at::kDropInTable + B(at::kTailArg)));
        TailStep(2);
        return;
    case 2:
        if (B(at::kCounterB) != 0x20) return;
        SetW(at::kTailTimer, 0x10);
        TailStep(3);
        return;
    case 3: {
        const auto left = static_cast<std::uint16_t>(W(at::kTailTimer) - 1u);
        SetW(at::kTailTimer, left);
        if (left != 0) return;
        switch (B(at::kTailArg)) {
        case 0:
        case 4: SH_CALL(Field_ChangeArea)(0x6C, 0x110000, 0x590000, 0x85); break;
        case 1:
        case 2: SH_CALL(Field_ChangeArea)(0x22, 0x340000, 0x430000, 0x81); break;
        case 3:
        case 5: SH_CALL(Field_ChangeArea)(4, 0x3E0000, 0x50000, 0x81); break;
        default: break;
        }
        TailStep(4);
        return;
    }
    case 4:
        if (B(at::kCounterB) != 0x31) return;
        B(at::kCounterB) = 0;
        SH_CALL(ScriptFlags_Clear40)();
        SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x1E);
        SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x1F);
        TailDone();
        return;
    default: return;
    }
}

// original 0x56DB80 (slot 10): two runs through a byte table, states 0..3 and
// 20..23 the same four steps - ScriptFlags_Set40 and Transition_Start(0);
// once MoveScript_WaitWordDA is 0, Draw_PassFlags 0, Party_HealJoined,
// Sound_StopMusic, Sound_LoadStream(0); once Sound_StreamDone,
// Sound_ResumeAll, Transition_Start(1), Draw_PassFlags 0x1F; once the wait
// word is 0 - then state 10 for the first, and for the second the menu set
// up (0x929F00 = 0, 0x929F0C = 0xFE, 0x929EC2 = 1, 0x929EC3 = 0, Game_Mode 7)
// and state 6. State 5: unless a message is up, ScriptFlags_Set40 and state
// 6; 6: the same menu set-up and state 7; 7: once Game_Mode is 2, state 10;
// 10: the argument not 0xFF, Msg_OpenScript(it) and Field_Request 2 - state
// 11; 11: once no message is up, ScriptFlags_Clear40, the tail cleared.
// States 4, 8, 9, 12..19 do nothing.
extern "C" void __cdecl FieldTail_HealAndMenu(void) {
    static const unsigned char kCase[24] = {0, 1, 2, 3, 13, 4, 5, 6, 13, 13, 7, 8, 13, 13, 13, 13, 13, 13, 13, 13, 9, 10, 11, 12};
    const signed char state = TailState();
    if (static_cast<U>(static_cast<std::int32_t>(state)) > 0x17) return;
    auto rest = [](unsigned char next) {
        if (W(at::kWaitWord) != 0) return;
        B(at::kPassFlags) = 0;
        SH_CALL(Party_HealJoined)();
        SH_CALL(Sound_StopMusic)();
        SH_CALL(Sound_LoadStream)(0);
        TailStep(next);
    };
    auto resume = [](unsigned char next) {
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Sound_ResumeAll)();
        SH_CALL(Transition_Start)(1);
        B(at::kPassFlags) = 0x1F;
        TailStep(next);
    };
    switch (kCase[state]) {
    case 0:
    case 9:
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(Transition_Start)(0);
        TailStep(kCase[state] == 0 ? 1 : 0x15);
        return;
    case 1: rest(2); return;
    case 10: rest(0x16); return;
    case 2: resume(3); return;
    case 11: resume(0x17); return;
    case 3:
        if (W(at::kWaitWord) != 0) return;
        TailStep(0xA);
        return;
    case 4:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Set40)();
        TailStep(6);
        return;
    case 5:
        B(at::kMenuMode) = 0;
        B(at::kFieldEC3) = 0;
        B(at::kMenuCursor) = 0xFE;
        B(at::kFieldEC2) = 1;
        SetW(at::kGameMode, 7);
        TailStep(7);
        return;
    case 6:
        if (W(at::kGameMode) != 2) return;
        TailStep(0xA);
        return;
    case 7: {
        const unsigned char message = B(at::kTailArg);
        if (message != 0xFF) {
            SH_CALL(Msg_OpenScript)(message);
            Field_Request = 2;
        }
        TailStep(0xB);
        return;
    }
    case 8:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Clear40)();
        TailDone();
        return;
    case 12:
        if (W(at::kWaitWord) != 0) return;
        B(at::kMenuMode) = 0;
        B(at::kMenuCursor) = 0xFE;
        B(at::kFieldEC2) = 1;
        B(at::kFieldEC3) = 0;
        SetW(at::kGameMode, 7);
        TailStep(6);
        return;
    default: return;
    }
}

// original 0x56DDD0 (slot 6): 0 - ScriptFlags_Set40, Msg_OpenScript(0xD8),
// Field_Request 2 and the state up; 1 - once no message is up,
// ScriptFlags_Clear40 and the tail cleared.
extern "C" void __cdecl FieldTail_Message(void) {
    switch (TailState()) {
    case 0: {
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(Msg_OpenScript)(0xD8);
        const auto next = static_cast<unsigned char>(B(at::kTailState) + 1);
        Field_Request = 2;
        B(at::kTailState) = next;
        return;
    }
    case 1:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Clear40)();
        TailDone();
        return;
    default: return;
    }
}

// original 0x56DE30 (slot 27): a tail jump through WorldMap_FieldHooks by
// WorldMap_RecordIndex's low byte (0..11; past the twelve ours aborts where
// the original jumps into Field_ObjectTriggers).
extern "C" void __cdecl FieldTail_WorldMapHook(void) {
    const U index = SH_CALL(WorldMap_RecordIndex)() & 0xFF;
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAt(at::kWorldMapHooks, index, "FieldTail_WorldMapHook")))();
}

// original 0x56DE50 (slot 44): 0 - unless a message is up: bit (argument) of
// 0x904650 set, the system message 0x4172 + argument copied (8 bytes, the
// CRT's copy) into Text_Records, Msg_OpenSystem(0xFA), Field_Request 2,
// Sound_StopMusic, Sound_LoadStream(9), state 1; 1 - once Sound_StreamDone
// and no message is up, Sound_ResumeAll, the byte 0x903804 points at 0,
// ScriptFlags_Clear40, the tail cleared.
extern "C" void __cdecl FieldTail_FlagMessage(void) {
    switch (TailState()) {
    case 0: {
        if (Field_Request == 2) return;
        SH_CALL(Flags_Set)(At(at::kMessageBits), B(at::kTailArg));
        const unsigned char* const text = SH_CALL(Msg_SystemPtr)(B(at::kTailArg) + 0x4172u);
        SH_AT(void* (__cdecl*)(void*, const void*, unsigned), at::kMemcpy)(At(at::kTextRecords), text, 8);
        SH_CALL(Msg_OpenSystem)(0xFA);
        Field_Request = 2;
        SH_CALL(Sound_StopMusic)();
        SH_CALL(Sound_LoadStream)(9);
        TailStep(1);
        return;
    }
    case 1:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        if (Field_Request == 2) return;
        SH_CALL(Sound_ResumeAll)();
        *At(L(at::kStreamFlag)) = 0;
        SH_CALL(ScriptFlags_Clear40)();
        TailDone();
        return;
    default: return;
    }
}

// original 0x56DF10 (slot 55): 0 - ScriptFlags_Set40, Msg_OpenScript(0x21)
// when Inventory_Count(0, 0x57, 0) is not 0, else (0x20); Field_Request 2,
// the state up; 1 - once no message is up, ScriptFlags_Clear40, the kind and
// state 0 (the argument kept); 2 - once no message is up: the highest set
// story flag n of 0xFF..0xF6 picks the area 0xB9 - (0xFF - n) (0xAF with
// none: the word of a dword 0xB9 + k * 0xFFFF); the leader's position and
// Game_AreaNumber kept at 0x904148..0x904150; Field_ChangeArea(area,
// 0x430000, 0x160000, 4), Field_ScriptFlags2 |= 0x40, ScriptFlags_Clear40,
// the kind and state 0.
extern "C" void __cdecl FieldTail_StoryWarp(void) {
    switch (TailState()) {
    case 0: {
        SH_CALL(ScriptFlags_Set40)();
        const bool has = static_cast<std::uint16_t>(SH_CALL(Inventory_Count)(0, 0x57, 0)) != 0;
        SH_CALL(Msg_OpenScript)(has ? 0x21 : 0x20);
        const auto next = static_cast<unsigned char>(B(at::kTailState) + 1);
        Field_Request = 2;
        B(at::kTailState) = next;
        return;
    }
    case 1:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Clear40)();
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    case 2: {
        if (Field_Request == 2) return;
        U flag = 0xFF, area = 0xB9;
        for (;;) {
            if (SH_CALL(Flags_Test)(At(at::kStoryFlags), flag) != 0) break;
            --flag;
            area += 0xFFFF;
            if (static_cast<std::int32_t>(flag) < 0xF6) break;
        }
        SetL(at::kReturnX, L(at::kLeaderX));
        SetL(at::kReturnZ, L(at::kLeaderZ));
        SetW(at::kReturnArea, W(at::kAreaNumber));
        SH_CALL(Field_ChangeArea)(area, 0x430000, 0x160000, 4);
        B(at::kScriptFlags2) = static_cast<unsigned char>(B(at::kScriptFlags2) | 0x40);
        SH_CALL(ScriptFlags_Clear40)();
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        return;
    }
    default: return;
    }
}

// ============================================================================
// Draw layers: three MapCell_Handlers kinds (DrawLayer_Open calls each with
// the cell's record, its byte 1 and byte 0), and a map cell cleared
// ============================================================================

namespace {
// A vertex dword of the quads: the s8 at byte 3 and the s8 at byte 2, each
// doubled, about the cell's origin, and the low word; the fourth word 0
// (DIV-0023, as map_cells.cpp's QuadVertex).
void QuadVertex(short* v, U dword, U x0, U y0) {
    v[0] = static_cast<short>(x0 + 2u * static_cast<U>(S8(dword >> 24)));
    v[1] = static_cast<short>(y0 + 2u * static_cast<U>(S8(dword >> 16)));
    v[2] = static_cast<short>(dword);
    v[3] = 0;
}
// The four corners (v[0..3], eight words each) projected into the POLY_FT4 or
// POLY_G4 at prim as the originals order them - v1, v2, v3, then v0 to the
// first point - and their depths.
void Project4(short* v, unsigned char* prim) {
    long unused;
    SH_CALL(Gte_RotTransPers4)(v + 4, v + 8, v + 12, v, reinterpret_cast<float*>(prim + 0x18), reinterpret_cast<float*>(prim + 0x28),
                               reinterpret_cast<float*>(prim + 0x38), reinterpret_cast<float*>(prim + 8), &unused);
    SH_CALL(Gte_StoreDepthF4)(reinterpret_cast<float*>(prim + 0x20), reinterpret_cast<float*>(prim + 0x30),
                              reinterpret_cast<float*>(prim + 0x40), reinterpret_cast<float*>(prim + 0x10));
}
}  // namespace

// original 0x570870 (MapCell_Handlers[16], in Area_TestCondition's extent):
// a cell's animated quads. Unless the record's condition (its low word) says
// no, or its byte +2 is 1, walks subrecords from dword 1 until the dword
// index equals byte +2 again: four vertex dwords (QuadVertex about ((b1 -
// 0x80) * 128, (b0 - 0x80) * 128)) into a POLY_FT4 at Gfx_PacketNext
// (Gte_RotTransPers4, Gte_StoreDepthF4, shade off); then a frame dword F:
// Frame_Counter % (F & 0xFF) (unsigned) against the threshold bytes from F's
// byte 2 on - the first above it (signed compare) picks the frame -, n =
// ((F >> 8 & 0xFF) + 5) / 4 dwords of thresholds, the texture word n + frame
// dwords after F (Prim_SetTexture), committed to Draw_OtSlot (0x48); the next
// subrecord n + (F >> 8 & 0xFF) dwords after F. Kept as the original: a byte
// +2 no subrecord reaches never ends the walk, and the threshold scan has no
// bound. A period of 0 divides by 0: ours aborts. No PSX twin paired.
extern "C" void __cdecl MapCell_DrawFrames(const unsigned char* record, unsigned b1, unsigned b0) {
    if (SH_CALL(Area_TestCondition)(W(Key(record))) == 0) return;
    const U x0 = (b1 - 0x80u) << 7, y0 = (b0 - 0x80u) << 7;
    if ((LongAt(record) & 0xFF0000u) == 0x10000u) return;
    U at = 1;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        alignas(4) short v[16];
        for (U k = 0; k < 4; ++k) QuadVertex(v + 4 * k, LongAt(record + (at + k) * 4u), x0, y0);
        at += 4;
        Project4(v, prim);
        const U frames = LongAt(record + at * 4u);
        const U period = frames & 0xFF;
        if (period == 0) bof3::Fatal("MapCell_DrawFrames: a frame period of 0 at record 0x%X + %u (the original divides by it)",
                                     (unsigned)Key(record), (unsigned)(at * 4u));
        const auto phase = static_cast<std::int32_t>(Frame_Counter % period);
        U frame = 0;
        for (const unsigned char* p = record + at * 4u + 2; phase >= static_cast<std::int32_t>(*p); ++p) ++frame;
        const U skip = (((frames >> 8) & 0xFF) + 5u) >> 2;
        SH_CALL(Prim_SetTexture)(LongAt(record + (frame + skip + at) * 4u), prim, 1);
        const unsigned char slot = Draw_OtSlot;
        at += record[at * 4u + 1] + skip;
        SH_CALL(Gfx_CommitPrim)(slot, at::kPrimBytes);
    } while (at != record[2]);
}

// original 0x570BC0 (MapCell_Handlers[33], in MapCell_FlatOverlay's extent): a
// cell's shaded quads. Unless the record's condition says no: unless byte +2
// is 1, subrecords of eight dwords from dword 1 until the dword index equals
// byte +2 - a draw mode (tpage 0x95 | (bits 24..25 of the first colour) << 5,
// dtd 1) committed to Draw_OtSlot (0xC), a POLY_G4 (Gpu_SetPolyG4) at
// Gfx_PacketNext, four vertex dwords (QuadVertex) through Gte_RotTransPers4
// and Gte_StoreDepthF4, semi-transparent by the first colour's bit 31, four
// colours (bytes 2, 1, 0 of each dword to r, g, b), committed to Draw_OtSlot
// (0x44). Then, and when byte +2 is 1, a draw mode (tpage 0x95) committed to
// Draw_OtSlot (0xC). A byte +2 no subrecord reaches never ends the walk, as
// the original. No PSX twin paired.
extern "C" void __cdecl MapCell_DrawShaded(const unsigned char* record, unsigned b1, unsigned b0) {
    if (SH_CALL(Area_TestCondition)(W(Key(record))) == 0) return;
    const U x0 = (b1 - 0x80u) << 7, y0 = (b0 - 0x80u) << 7;
    if ((LongAt(record) & 0xFF0000u) != 0x10000u) {
        U at = 1;
        const unsigned char* sub = record + 4;
        do {
            const U tpage = (((LongAt(sub + 0x10) >> 24) & 3u) << 5) | 0x95u;
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
            SH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
            unsigned char* const prim = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(prim);
            at += 4;
            alignas(4) short v[16];
            for (U k = 0; k < 4; ++k) QuadVertex(v + 4 * k, LongAt(sub + 4u * k), x0, y0);
            sub += 0x10;
            Project4(v, prim);
            SH_CALL(Gpu_SetSemiTrans)(prim, LongAt(sub) >> 31);
            static const unsigned kColours[4] = {4, 0x14, 0x24, 0x34};
            for (U k = 0; k < 4; ++k) {
                const U c = LongAt(sub + 4u * k);
                prim[kColours[k]] = static_cast<unsigned char>(c >> 16);
                prim[kColours[k] + 1] = static_cast<unsigned char>(c >> 8);
                prim[kColours[k] + 2] = static_cast<unsigned char>(c);
            }
            sub += 0x10;
            at += 4;
            SH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x44);
        } while (at != record[2]);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
}

// original 0x570DE0 (MapCell_Handlers[34], in MapCell_FlatOverlay's extent): a
// cell's spinning quads. Unless the record's condition says no: the centre
// (words of ((b1 - 0x80) * 64 + s8 byte 3 of dword 1) * 2, ((b0 - 0x80) * 64 +
// s8 byte 2) * 2 and dword 1's low word) and a spin S = the three 10-bit
// fields of dword 2 (bits 20, 10, 0) times (Frame_Counter & 0xFFF) * 4, as
// words. Unless byte +2 is 3, subrecords of six dwords from dword 3 until
// the dword index equals byte +2: under a pushed matrix the angles (S + the
// subrecord's first dword's three fields * 4) & 0xFFF through Gte_RotMatrix,
// the centre through Gte_RotTrans into its translation, Gte_MulMatrix0 with
// Camera_Matrix, set as rotation and translation; a POLY_FT4, four vertex
// dwords of three 10-bit fields each (bit 9 the sign; the top field keeps
// bits 10 and 11 when its bit 9 is clear, as the original's shift leaves
// them) through Gte_RotTransPers4 and Gte_StoreDepthF4, the texture word
// (dword 5) through Prim_SetTexture, committed (0x48) to slot 6 when that
// word, read again, has bit 30, else Draw_OtSlot; the matrix popped. The
// SVECTORs' fourth words are 0 (DIV-0023). A byte +2 no subrecord reaches
// never ends the walk, as the original. No PSX twin paired.
extern "C" void __cdecl MapCell_DrawSpinning(const unsigned char* record, unsigned b1, unsigned b0) {
    if (SH_CALL(Area_TestCondition)(W(Key(record))) == 0) return;
    const U d1 = LongAt(record + 4), d2 = LongAt(record + 8);
    const U counter = Frame_Counter & 0xFFFu;
    alignas(4) short centre[4];
    centre[0] = static_cast<short>((static_cast<U>(static_cast<std::uint16_t>(S8(d1 >> 24))) + ((b1 - 0x80u) << 6)) << 1);
    centre[1] = static_cast<short>((static_cast<U>(static_cast<std::uint16_t>(S8(d1 >> 16))) + ((b0 - 0x80u) << 6)) << 1);
    centre[2] = static_cast<short>(d1);
    centre[3] = 0;
    const U spin[3] = {((d2 >> 20) & 0x3FFu) * counter * 4u, ((d2 >> 10) & 0x3FFu) * counter * 4u, (d2 & 0x3FFu) * counter * 4u};
    if ((LongAt(record) & 0xFF0000u) == 0x30000u) return;
    U at = 3;
    const unsigned char* sub = record + 0xC;
    do {
        SH_CALL(Gte_PushMatrix)();
        const U d = LongAt(sub);
        alignas(4) short angles[4];
        angles[0] = static_cast<short>((spin[0] + ((d >> 20) & 0x3FFu) * 4u) & 0xFFFu);
        angles[1] = static_cast<short>((spin[1] + ((d >> 10) & 0x3FFu) * 4u) & 0xFFFu);
        angles[2] = static_cast<short>((spin[2] + (d & 0x3FFu) * 4u) & 0xFFFu);
        angles[3] = 0;
        alignas(4) unsigned char matrix[0x20];
        SH_CALL(Gte_RotTrans)(centre, reinterpret_cast<long*>(matrix + 0x14));
        SH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(matrix));
        SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<short*>(matrix), reinterpret_cast<short*>(matrix));
        SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<unsigned long*>(matrix));
        SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<unsigned long*>(matrix));
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        alignas(4) short v[16];
        for (U k = 0; k < 4; ++k) {
            const U e = LongAt(sub + 4u + 4u * k);
            U fx = e >> 20, fy = (e >> 10) & 0x3FFu, fz = e & 0x3FFu;
            if (fx & 0x200u) fx |= 0xFFFFFC00u;
            if (fy & 0x200u) fy |= 0xFFFFFC00u;
            if (fz & 0x200u) fz |= 0xFFFFFC00u;
            v[4 * k] = static_cast<short>(fx);
            v[4 * k + 1] = static_cast<short>(fy);
            v[4 * k + 2] = static_cast<short>(fz);
            v[4 * k + 3] = 0;
        }
        Project4(v, prim);
        SH_CALL(Prim_SetTexture)(LongAt(sub + 0x14), prim, 1);
        SH_CALL(Gfx_CommitPrim)((LongAt(sub + 0x14) & 0x40000000u) ? 6u : Draw_OtSlot, at::kPrimBytes);
        SH_CALL(Gte_PopMatrix)();
        at += 6;
        sub += 0x18;
    } while (at != record[2]);
}

// original 0x5728D0: a map cell's byte cleared and its drawn records
// retyped. AreaMap_Bytes[x + z * width] = 0 (s16 x and z, the header's width
// byte). Then the cell in the view: c = x + 1 - MapView_Origin's x, r = z -
// its z (s16s); with c + r in 0..0x37 and c - r in 0..0x37, the row (c + r +
// MapView_Row + 1, wrapped once at 0x38) and column ((c - r) / 2 +
// MapView_Column + 1, wrapped once at 0x1C) name a MapView_Cells word; not
// 0, it plus AreaMap_CellBase's word is a run of records in the area block
// (their count the high word of the dword before), walked by each record's
// byte +2 dwords, and every record of kind 3, 9 or 0x27 (the top byte)
// becomes kind 0x30. Ours aborts where the original reads past MapView_Cells
// (a row or column past its wrap) or where the walk would never end (a step
// of 0, or past the run's end). Called by FC2's 0x46D180, Field_CellPickup
// and 19 Capcom functions; FieldHidden_ClearCell's cells. PSX twin
// 0x80155E74 (gap4, a hypothesis).
extern "C" void __cdecl AreaMap_ClearCell(unsigned x, unsigned z) {
    const int zs = static_cast<short>(z), xs = static_cast<short>(x);
    const U width = LongAt(AreaMap_Header) & 0xFF;
    const_cast<unsigned char*>(AreaMap_Bytes)[static_cast<std::int32_t>(width * static_cast<U>(zs)) + xs] = 0;
    const int c = static_cast<short>(x + 1u - W(at::kMapOrigin));
    const int r = static_cast<short>(z - W(at::kMapOrigin + 2));
    int row = c + r;
    if (row >= 0x38 || row < 0) return;
    row += S16(W(at::kMapRow)) + 1;
    if (row >= 0x38) row -= 0x38;
    int column = c - r;
    if (column >= 0x38 || column < 0) return;
    column = column / 2 + S16(W(at::kMapColumn)) + 1;
    if (column >= 0x1C) column -= 0x1C;
    if (row < 0 || row >= 0x38 || column < 0 || column >= 0x1C)
        bof3::Fatal("AreaMap_ClearCell: view cell (%d, %d) past MapView_Cells' 0x38 x 0x1C (the original reads there)", row, column);
    const U item = W(at::kMapCells + 2u * static_cast<U>(column + row * 0x1C));
    if (item == 0) return;
    const U n = item + (L(at::kCellBase) & 0xFFFF);
    U p = at::kCellRecords + n * 4u;
    const U end = p + (L(at::kCellRecords - 4u + n * 4u) >> 16) * 4u - 4u;
    while (p != end) {
        if (p > end) bof3::Fatal("AreaMap_ClearCell: the record walk passed its end 0x%X at 0x%X (the original runs on)", (unsigned)end, (unsigned)p);
        const U v = L(p);
        const U kind = v & 0xFF000000u;
        if (kind == 0x03000000u || kind == 0x09000000u || kind == 0x27000000u) SetL(p, (v & 0xFFFFFFu) | 0x30000000u);
        const U step = B(p + 2);
        if (step == 0) bof3::Fatal("AreaMap_ClearCell: a record of 0 dwords at 0x%X (the original never ends)", (unsigned)p);
        p += step * 4u;
    }
}

// ============================================================================
// The trade screen (ItemTrade_States 0x66A470, dispatched by 0x593950 on
// 0x93985C - nobody's): a list of entries (0x66AD10's row 0x905B88, ten
// record numbers) of the 8-byte records at 0x66AB58 (+0 the item, +1 its
// category, +2..+4 three ingredients, +5..+7 their amounts); a count to
// choose; a yes / no. The state handlers are hidden starts in
// Sprite_ClutWord's extent. PSX twins 0x800F5048..0x800F5828 (table-
// anchored and callers, each a hypothesis).
// ============================================================================

namespace {
// The record the row's entry k names (0x66AD10[k + row * 10] * 8), unchecked.
U TradeRecord(int k) {
    return at::kTradeRecords + static_cast<U>(B(at::kTradeIndex + static_cast<U>(k + B(at::kTradeRow) * 10))) * 8u;
}
void TradeBox() { SH_AT(void (__cdecl*)(int, int, int, int, int), at::kTradeBox)(0x14, 0x12, 0x118, 0x13, 0); }
void TradeList(unsigned flag) { SH_AT(void (__cdecl*)(unsigned), at::kTradeList)(flag); }
unsigned char TradeLacks(unsigned k, unsigned quantity) {
    return SH_AT(unsigned char (__cdecl*)(unsigned, unsigned), at::kTradeLacks)(k, quantity);
}
const unsigned char* Pool(U word_cell) { return At(bof3::addr::MessagePools + (L(word_cell) & 0xFFFF)); }
}  // namespace

// original 0x593960 (ItemTrade_States[0]): the open step (ItemTrade_OpenSteps
// 0x66A47C by 0x93985E, past its two ours aborts only where the table runs
// into what is not code), then the frame 0x469750(0x14, 0x12, 0x118, 0x13, 0),
// the list 0x594410(0), 0x5947D0 and a tail jump to 0x5942C0.
extern "C" void __cdecl ItemTrade_Open(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAt(at::kTradeOpenSteps, B(at::kTradeStep), "ItemTrade_Open")))();
    TradeBox();
    TradeList(0);
    SH_AT(void (__cdecl*)(), at::kTradeCursor)();
    SH_AT(void (__cdecl*)(), at::kTradeFrame)();
}

// original 0x5939A0 (ItemTrade_OpenSteps[0]): Transition_Start(1), the pick
// 0x6BE08C = 0, the quantity 0x6BE08E = 1, the row count 0x6BE08D = 0x594790's
// al, the step up.
extern "C" void __cdecl ItemTrade_OpenStart(void) {
    SH_CALL(Transition_Start)(1);
    B(at::kTradePick) = 0;
    B(at::kTradeQuantity) = 1;
    B(at::kTradeRowCount) = SH_AT(unsigned char (__cdecl*)(), at::kTradeRows)();
    B(at::kTradeStep) = static_cast<unsigned char>(B(at::kTradeStep) + 1);
}

// original 0x5939D0 (ItemTrade_OpenSteps[1]): once MoveScript_WaitWordDA is
// 0, the state 0x93985C = 1 and the step 0.
extern "C" void __cdecl ItemTrade_OpenWait(void) {
    if (W(at::kWaitWord) != 0) return;
    B(at::kTradeState) = 1;
    B(at::kTradeStep) = 0;
}

// original 0x5939F0 (ItemTrade_States[1]): the run step (ItemTrade_RunSteps
// 0x66A484 by 0x93985E: pick, count, confirm, and 0x594060's message), then
// 0x5942C0, 0x5947D0 and, with the pick's bits 6 and 7 clear, the hand at
// (0x1A, 0x51 + 13 * the s8 pick).
extern "C" void __cdecl ItemTrade_Run(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAt(at::kTradeRunSteps, B(at::kTradeStep), "ItemTrade_Run")))();
    SH_AT(void (__cdecl*)(), at::kTradeFrame)();
    SH_AT(void (__cdecl*)(), at::kTradeCursor)();
    const unsigned char pick = B(at::kTradePick);
    if (pick & 0xC0) return;
    SH_CALL(Menu_DrawHand)(0x1A, static_cast<int>(13u * static_cast<U>(S8(pick)) + 0x51u), 0);
}

// original 0x593A30 (ItemTrade_RunSteps[0]): the pick. Up and down
// (Input_AutoRepeat of the pressed word's 0x5000) move it, wrapping at the
// row count, with sound 0x100. Confirm: 0x594700(pick, 1) answering (an
// ingredient short) - sound 0x107; else the two Inventory_Count of the
// entry's item (in the bag, then equipped) summed as a byte: 99 or more - the
// pick's bit 7, sound 0x104, the quantity 0, step 3 (0x594060's message);
// else bit 6, sound 0x104, the step up. Cancel: bit 7, 0x6BE08F = 1, sound
// 0x106, the state 0x93985C up. Then the frame, and with bits 6 and 7 clear
// the entry's help line (Item_HelpMessage through Msg_SystemPtr) at (0x18,
// 0x14); the list 0x594410(0).
extern "C" void __cdecl ItemTrade_PickItem(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0x5000u);
    if (moved & 0x1000u) {
        const auto pick = static_cast<unsigned char>(B(at::kTradePick) - 1);
        B(at::kTradePick) = pick;
        if (static_cast<signed char>(pick) < 0) B(at::kTradePick) = static_cast<unsigned char>(B(at::kTradeRowCount) - 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else if (moved & 0x4000u) {
        const auto pick = static_cast<unsigned char>(B(at::kTradePick) + 1);
        B(at::kTradePick) = pick;
        if (static_cast<signed char>(pick) >= static_cast<int>(B(at::kTradeRowCount))) B(at::kTradePick) = 0;
        SH_CALL(Sound_PlayEffect)(0x100);
    }
    const std::uint16_t pressed = Input_Pressed;
    if (W(at::kConfirm) & pressed) {
        if (TradeLacks(B(at::kTradePick), 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x107);
        } else {
            const U record = TradeRecord(S8(B(at::kTradePick)));
            const auto bag = static_cast<unsigned char>(SH_CALL(Inventory_Count)(B(record + 1), B(record), 1));
            const auto worn = static_cast<unsigned char>(SH_CALL(Inventory_Count)(B(record + 1), B(record), 0));
            if (static_cast<unsigned char>(bag + worn) >= 0x63) {
                B(at::kTradePick) = static_cast<unsigned char>(B(at::kTradePick) | 0x80);
                SH_CALL(Sound_PlayEffect)(0x104);
                B(at::kTradeQuantity) = 0;
                B(at::kTradeStep) = 3;
            } else {
                B(at::kTradePick) = static_cast<unsigned char>(B(at::kTradePick) | 0x40);
                SH_CALL(Sound_PlayEffect)(0x104);
                B(at::kTradeStep) = static_cast<unsigned char>(B(at::kTradeStep) + 1);
            }
        }
    } else if (W(at::kCancel) & pressed) {
        B(at::kTradeAnswer) = 1;
        B(at::kTradePick) = static_cast<unsigned char>(B(at::kTradePick) | 0x80);
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kTradeState) = static_cast<unsigned char>(B(at::kTradeState) + 1);
    }
    TradeBox();
    const unsigned char pick = B(at::kTradePick);
    if ((pick & 0xC0) == 0) {
        const U record = TradeRecord(S8(pick));
        const unsigned help = SH_CALL(Item_HelpMessage)(B(record + 1), B(record));
        const unsigned char* const text = SH_CALL(Msg_SystemPtr)(help);
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, text);
    }
    TradeList(0);
}

// original 0x593C60 (ItemTrade_RunSteps[1]): the count. Input_AutoRepeat of
// the pressed word's 0xF000: 0x2000 +1, 0x1000 +10, 0x4000 -10, 0x8000 -1
// (a byte), the quantity 0x6BE08E held to 1..99 (s8); lowered while
// 0x594700(pick & 0xF, quantity) answers; lowered while it plus the item's
// two Inventory_Count summed as a byte passes 99; sound 0x100 if it moved.
// Confirm: 0x6BE08F = 0, sound 0x104, the step up. Cancel: the pick's bit 6
// cleared, the quantity 1, sound 0x106, the step down. Then the frame, the
// prompt (MessagePools + the word at 0x80361C) at (0x18, 0x14), the list
// 0x594410(1), 0x594AD0 and the hand at (0x1A, 0x6B).
extern "C" void __cdecl ItemTrade_PickCount(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000u) & 0xFFFFu;
    unsigned char step = 0;
    if (moved == 0x8000u) step = 0xFF;
    else if (moved == 0x4000u) step = 0xF6;
    else if (moved == 0x1000u) step = 0xA;
    else if (moved == 0x2000u) step = 1;
    const unsigned char old = B(at::kTradeQuantity);
    auto quantity = static_cast<unsigned char>(old + step);
    if (static_cast<signed char>(quantity) < 1) quantity = 1;
    else if (static_cast<signed char>(quantity) > 0x63) quantity = 0x63;
    const unsigned k = B(at::kTradePick) & 0xFu;
    B(at::kTradeQuantity) = quantity;
    while (TradeLacks(k, quantity) != 0) {
        quantity = static_cast<unsigned char>(B(at::kTradeQuantity) - 1);
        B(at::kTradeQuantity) = quantity;
    }
    const U record = TradeRecord(static_cast<int>(k));
    const auto bag = static_cast<unsigned char>(SH_CALL(Inventory_Count)(B(record + 1), B(record), 1));
    const auto worn = static_cast<unsigned char>(SH_CALL(Inventory_Count)(B(record + 1), B(record), 0));
    const U held = static_cast<unsigned char>(bag + worn);
    // The s8 quantity comes down until it plus the held count is 99 or less:
    // with more than 227 held no s8 does, and the original loops for ever.
    if (held > 227) bof3::Fatal("ItemTrade_PickCount: %u held - no quantity brings the sum to 99 (the original never ends)", (unsigned)held);
    unsigned char q = B(at::kTradeQuantity);
    while (static_cast<int>(S8(q)) + static_cast<int>(held) > 0x63) q = static_cast<unsigned char>(q - 1);
    B(at::kTradeQuantity) = q;
    if (q != old) SH_CALL(Sound_PlayEffect)(0x100);
    const std::uint16_t pressed = Input_Pressed;
    if (W(at::kConfirm) & pressed) {
        B(at::kTradeAnswer) = 0;
        SH_CALL(Sound_PlayEffect)(0x104);
        B(at::kTradeStep) = static_cast<unsigned char>(B(at::kTradeStep) + 1);
    } else if (W(at::kCancel) & pressed) {
        B(at::kTradeQuantity) = 1;
        B(at::kTradePick) = static_cast<unsigned char>(B(at::kTradePick) & 0xBF);
        SH_CALL(Sound_PlayEffect)(0x106);
        B(at::kTradeStep) = static_cast<unsigned char>(B(at::kTradeStep) - 1);
    }
    TradeBox();
    SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, Pool(at::kPoolWordPick));
    TradeList(1);
    SH_AT(void (__cdecl*)(), at::kTradeCount)();
    SH_CALL(Menu_DrawHand)(0x1A, 0x6B, 0);
}

// DIVERGENCE DIV-0027 (amended 2026-10-03, after the live check): the per-item
// "Is <item> OK?" answers. Capcom draws the two answer words, one space apart,
// at x 0xDE and the hand at 0xDC + 36 * the answer - stops fitted to the Chinese
// words: under the English overlay the hand on No covers Yes (the owner's
// masterAndManillo.txt, frames 2700 and 3030). With the flag on (FieldE2_Inject,
// a Latin overlay, after the self-test) the layout is the load / save screen's,
// as the leave prompt's is (effect_1g.cpp, yes_no_layout.cpp): the first answer
// three spaces further left, the second where it was, the hand two units left
// of each.
unsigned char g_trade_confirm_layout = 0;

namespace {

struct ConfirmAnswers {
    const unsigned char* line;   // a static buffer, good until the next call
    int x;
    int stop[2];                 // the hand on the first answer, on the second
};

ConfirmAnswers ConfirmLayout(const unsigned char* s) {
    constexpr unsigned kMoved = 3;
    static unsigned char line[32];
    static const unsigned char kSpace[2] = {0x20, 0};
    unsigned first = 0;
    while (s[first] && s[first] != 0x20) ++first;
    unsigned second = first;
    while (s[second] == 0x20) ++second;
    unsigned end = second;
    while (s[end] && s[end] != 0x20) ++end;
    bool plain = first != 0 && second != first && end != second && s[end] == 0 && end + kMoved < sizeof line;
    for (unsigned i = 0; plain && i < end; ++i) plain = s[i] >= 0x20 && s[i] < 0x80;
    if (!plain) bof3::Fatal("DIV-0027: the trade's answers are not two one-byte words with spaces between");
    unsigned n = 0;
    for (unsigned i = 0; i < first; ++i) line[n++] = s[i];
    for (unsigned i = 0; i < second - first + kMoved; ++i) line[n++] = 0x20;
    const unsigned second_at = n;
    for (unsigned i = second; i < end; ++i) line[n++] = s[i];
    line[n] = 0;
    const int x = 0xDE - static_cast<int>(kMoved) * TextAdvance_Of(kSpace);
    int pen = x;
    for (unsigned i = 0; i < second_at; ++i) pen += TextAdvance_Of(line + i);
    return {line, x, {x - 2, pen - 2}};
}

}  // namespace

// original 0x593E60 (ItemTrade_RunSteps[2]): the yes / no. Left or right
// (Input_AutoRepeat of 0xA000) flips the hand 0x6BE08F, sound 0x100. Else
// cancel: sound 0x106, the step down; confirm on no: the same; confirm on
// yes: sound 0x105, the pick's bit 6 cleared, Inventory_Add(the entry's
// category, item, the quantity) (a fourth word 0 pushed), 0x594D90 (the
// ingredients taken), the quantity 1 and step 0. Then the frame; at step 2
// the item's name (Item_NamePtr, 16 bytes) into Text_Records, the two
// prompts (the words at 0x80360C, 0x803614) at (0x18, 0x14) and (0xDE, 0x14),
// the hand at (0xDC + 36 * the answer, 0x16); the list 0x594410(1); at any
// step but 0 a tail jump to 0x594AD0.
extern "C" void __cdecl ItemTrade_Confirm(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xA000u);
    if (moved & 0xA000u) {
        B(at::kTradeAnswer) = static_cast<unsigned char>(B(at::kTradeAnswer) ^ 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else {
        const std::uint16_t pressed = Input_Pressed;
        if (W(at::kCancel) & pressed) {
            SH_CALL(Sound_PlayEffect)(0x106);
            B(at::kTradeStep) = static_cast<unsigned char>(B(at::kTradeStep) - 1);
        } else if (W(at::kConfirm) & pressed) {
            if (B(at::kTradeAnswer) != 0) {
                SH_CALL(Sound_PlayEffect)(0x106);
                B(at::kTradeStep) = static_cast<unsigned char>(B(at::kTradeStep) - 1);
            } else {
                SH_CALL(Sound_PlayEffect)(0x105);
                const auto pick = static_cast<unsigned char>(B(at::kTradePick) & 0xBF);
                B(at::kTradePick) = pick;
                const U record = TradeRecord(S8(pick));
                SH_CALL(Inventory_Add)(B(record + 1), B(record), B(at::kTradeQuantity));
                SH_AT(void (__cdecl*)(), at::kTradeTake)();
                B(at::kTradeQuantity) = 1;
                B(at::kTradeStep) = 0;
            }
        }
    }
    TradeBox();
    if (B(at::kTradeStep) == 2) {
        const U record = TradeRecord(static_cast<int>(B(at::kTradePick) & 0xFu));
        const unsigned char* const name = SH_CALL(Item_NamePtr)(B(record + 1), B(record));
        std::memcpy(At(at::kTextRecords), name, 0x10);
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, Pool(at::kPoolWordName));
        if (g_trade_confirm_layout) {
            const ConfirmAnswers a = ConfirmLayout(Pool(at::kPoolWordAsk));
            SH_CALL(Text_DrawAt)(a.x, 0x14, 0, 0xFF, a.line);
            SH_CALL(Menu_DrawHand)(a.stop[B(at::kTradeAnswer) != 0 ? 1 : 0], 0x16, 0);
        } else {
            SH_CALL(Text_DrawAt)(0xDE, 0x14, 0, 0xFF, Pool(at::kPoolWordAsk));
            SH_CALL(Menu_DrawHand)(static_cast<int>(B(at::kTradeAnswer) * 36u + 0xDCu), 0x16, 0);
        }
    }
    TradeList(1);
    if (B(at::kTradeStep) == 0) return;
    SH_AT(void (__cdecl*)(), at::kTradeCount)();
}

// ============================================================================

void FieldE2_Inject() {
    if (bof3::WantsShadow("field_e2")) field_e2::SelfTest();
    BOF3_INJECT(Scenario_CallB);
    BOF3_INJECT(Field_AfterBattleTally);
    BOF3_INJECT(Records_CountUnpaired);
    BOF3_INJECT(Records_CountUnpairedOfKind);
    BOF3_INJECT(Field_FaceMode11Object);
    BOF3_INJECT(Field_FloorHurt);
    BOF3_INJECT(AreaMap_CellsAllWide);
    BOF3_INJECT(Field_WayBlockedWide);
    BOF3_INJECT(AreaMap_CellsNoneWide);
    BOF3_INJECT(Leader_Pose3C);
    BOF3_INJECT(Leader_HopStart);
    BOF3_INJECT(Leader_HopFlight);
    BOF3_INJECT(Leader_HopPose);
    BOF3_INJECT(Leader_Pose3E);
    BOF3_INJECT(Leader_StepUp);
    BOF3_INJECT(Leader_Pose34);
    BOF3_INJECT(Leader_StepDown);
    BOF3_INJECT(Leader_HopStartAfterTick);
    BOF3_INJECT(Leader_HopFall);
    BOF3_INJECT(Leader_Rise);
    BOF3_INJECT(Leader_Sink);
    BOF3_INJECT(Leader_TurnBack);
    BOF3_INJECT(PartySet_ErrorLoop);
    BOF3_INJECT(Mode11_ObjectFrame);
    BOF3_INJECT(Mode11_ObjectStart);
    BOF3_INJECT(Mode11_ObjectControl);
    BOF3_INJECT(Mode11_ObjectMove);
    BOF3_INJECT(Mode11_ObjectEnd);
    BOF3_INJECT(Mode11_ObjectHalt);
    BOF3_INJECT(Mode11_ObjectDraw);
    BOF3_INJECT(Field_ObjectTrigger);
    BOF3_INJECT(Scenario_CellHook);
    BOF3_INJECT(FieldTail_LoadBank);
    BOF3_INJECT(FieldTail_DropInMove);
    BOF3_INJECT(FieldTail_HealAndMenu);
    BOF3_INJECT(FieldTail_Message);
    BOF3_INJECT(FieldTail_WorldMapHook);
    BOF3_INJECT(FieldTail_FlagMessage);
    BOF3_INJECT(FieldTail_StoryWarp);
    BOF3_INJECT(Field_ObjectTriggerByKind);
    BOF3_INJECT(MapCell_DrawFrames);
    BOF3_INJECT(MapCell_DrawShaded);
    BOF3_INJECT(MapCell_DrawSpinning);
    BOF3_INJECT(AreaMap_ClearCell);
    BOF3_INJECT(ItemTrade_Open);
    BOF3_INJECT(ItemTrade_OpenStart);
    BOF3_INJECT(ItemTrade_OpenWait);
    BOF3_INJECT(ItemTrade_Run);
    BOF3_INJECT(ItemTrade_PickItem);
    BOF3_INJECT(ItemTrade_PickCount);
    BOF3_INJECT(ItemTrade_Confirm);
    // DIVERGENCE DIV-0027 (amended 2026-10-03): after the self-test, which
    // compares the original's prompt; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("TradeConfirmLayout", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_trade_confirm_layout)),
                         &was, &is, 1);
        bof3::Log("DIV-0027    Manillo's \"Is <item> OK?\": answers re-spaced, the hand two units left of each");
    }
}
