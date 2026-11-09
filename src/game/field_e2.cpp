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
#include "common/asm_name.h"
#include "game/field_e2_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_e2::at;
// move_script::At: named in full below (source-layout B0, a clash on At)
// move_script::Long: named in full below (source-layout B0, a clash on Long)
using move_script::SetLong;
// move_script::SetWord: named in full below (source-layout B0, a clash on SetWord)
// move_script::Word: named in full below (source-layout B0, a clash on Word)
using U = std::uint32_t;

unsigned char& BFieldE2(U a) { return *move_script::At(a); }
U LFieldE2(U a) { return static_cast<U>(move_script::Long(move_script::At(a))); }
void SetL(U a, U v) { move_script::SetLong(move_script::At(a), static_cast<std::int32_t>(v)); }
std::uint16_t WFieldE2(U a) { return move_script::Word(move_script::At(a)); }
void SetW(U a, unsigned v) { move_script::SetWord(move_script::At(a), v); }
U Key(const void* p) { return Low32(p); }

// The record views (game/record_views.h): the leader helpers run on the leader,
// an ObjTrio record, and Field_State is one.
U LongAt(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
int S8FieldE2(unsigned v) { return static_cast<signed char>(v); }
int S16FieldE2(unsigned v) { return static_cast<short>(v); }
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
U CodeAtFieldE2(U table, U index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = LFieldE2(cell);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}

// Sprite_EnsureAnimation of a byte (the callee compares al and hands on dl;
// the originals push whatever register held it).
void Ensure(unsigned animation) { SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(animation)); }

// A member slot's CLUT word and palette (Sprite_Current +5).
U ClutWord(const unsigned char* s) { return ::bof3::Img(Gfx_ClutStrip_addr) + 0x1e3e + (static_cast<U>(s[5]) << 6); }
unsigned short* Palette(const unsigned char* s) {
    return reinterpret_cast<unsigned short*>(static_cast<std::uintptr_t>(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + (static_cast<U>(s[5]) << 6)));
}

// The pace's reach: Field_DirectionSteps[+8] times (+0x70 + 2), from +0x34 / +0x38.
void Ahead(const unsigned char* s, std::int32_t& x, std::int32_t& z) {
    const U dir = s[8], k = static_cast<U>(s[0x70]) + 2u;
    x = static_cast<std::int32_t>(LFieldE2(::bof3::Img(at::kDirectionSteps) + dir * 8u) * k + LongAt(s + 0x34));
    z = static_cast<std::int32_t>(LFieldE2(::bof3::Img(at::kDirectionSteps) + 4u + dir * 8u) * k + LongAt(s + 0x38));
}

// The view follows the leader when +5 is 0 and bit 3 of the script flags'
// low bytes is clear; then every fourth frame one tick of its script.
void FollowAndTick(unsigned char* s) {
    if (s[5] == 0 && ((BFieldE2(::bof3::Img(at::kScriptFlags2)) | BFieldE2(::bof3::Img(at::kScriptFlagsLow))) & 8) == 0)
        SH_CALL(MapView_SetElevation)(static_cast<short>(rv::As<records::SpriteHead>(s)->height_whole));
    if ((Frame_Counter & 3) == 0) SH_CALL(Sprite_ScriptTickOnce)();
}

}  // namespace

// ============================================================================
// The event script's rest
// ============================================================================

// original 0x5341C0 (PSX 0x801C2E34): entry (n & 0xFF) of the current chapter's call table B
// (Scenario_CallBTables 0x660BD4[Cond_ByteFA], PSX 0x801C2E34), reached with
// the caller's arguments in place. Scenario_CallA's shape (field_modes.cpp):
// a tail jump, because the entries take anything from nothing to several
// words where the callers left them. Unchecked, as the original - chapter 0's
// table pointer is 0 (docs/field_e2.md section 6).
// A pair by width (docs/x64-asm-policy.md 8 unit 3): the i686 text as it
// was, and the same tail jump for the x86-64 register arguments.
extern "C" __attribute__((naked)) void __cdecl Scenario_CallB(unsigned) {
#if defined(__i386__)
    // edx holds the image's move first (Bof3_ImageMove, src/hook/host.cpp;
    // task_switch_i686.h says why the VAs are displacements); eax, ecx and
    // edx are the three the original writes.
    asm("movl " BOF3_SYM(Bof3_ImageMove) ", %%edx\n\t"
        "movsbl %c0(%%edx), %%eax\n\t"     // Cond_ByteFA, the chapter
        "movl 4(%%esp), %%ecx\n\t"
        "movl %c1(%%edx,%%eax,4), %%edx\n\t"   // Scenario_CallBTables
        "andl $0xFF, %%ecx\n\t"
        "jmp *(%%edx,%%ecx,4)"
        :
        : "i"(0x8034E0), "i"(0x660BD4));
#elif defined(__x86_64__)
    // The same for the Win64 arguments: n in ecx, the rest in rdx, r8, r9 and
    // the caller's stack, forwarded untouched by a tail jump; the chapter
    // sign-extended to 64 bits (a negative one reads below the table, as the
    // i686 wrap does); the table word is a 32-bit code place, loaded into
    // eax and jumped through; r10 is the scratch (volatile, not an
    // argument). The entry sees ecx = n & 0xFF, as the i686 entry does.
    asm("movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%r10\n\t"
        "movsbq %c0(%%r10), %%rax\n\t"          // Cond_ByteFA, the chapter
        "andl $0xFF, %%ecx\n\t"
        "movl %c1(%%r10,%%rax,4), %%r10d\n\t"   // the chapter's table, a 32-bit place
        "movl (%%r10,%%rcx,4), %%eax\n\t"       // the entry's code word
        "jmp *%%rax"
        :
        : "i"(0x8034E0), "i"(0x660BD4));
#else
#error "Scenario_CallB: no tail jump for this architecture"
#endif
}

// original 0x5343C0: with no pair of the sixty at 0x9046D0 (+0 in use, +1 the
// record, 1-based) naming any of the eight records at 0x9048B0 whose kind +0
// is 4 and key +1 is `key` (a byte), *counter + 1. One count over all the
// matching records: a single pair naming any of them spares the counter.
extern "C" void __cdecl Records_CountUnpaired(unsigned key, U* counter) {
    unsigned char named = 0;
    for (U r = 0; r < 8; ++r) {
        if (BFieldE2(::bof3::Img(at::kRecords) + 8u * r) != 4) continue;
        if (BFieldE2(::bof3::Img(at::kRecords) + 1u + 8u * r) != static_cast<unsigned char>(key)) continue;
        for (U p = ::bof3::Img(Community_Records_addr) + 1u; p < ::bof3::Img(at::kPairsEnd) + 1u; p += 8)
            if (BFieldE2(p - 1) != 0 && BFieldE2(p) == r + 1u) ++named;
    }
    if (named == 0) ++*counter;
}

// original 0x534420: for each of the eight records at 0x9048B0 whose kind +0
// is `kind` (a byte) and whose +3 is not 0, its tally dword +4 + 1 unless a
// pair of the sixty at 0x9046D0 names it.
extern "C" void __cdecl Records_CountUnpairedOfKind(unsigned kind) {
    for (U r = 0; r < 8; ++r) {
        unsigned char named = 0;
        if (BFieldE2(::bof3::Img(at::kRecords) + 8u * r) != static_cast<unsigned char>(kind)) continue;
        if (BFieldE2(::bof3::Img(at::kRecords) + 3u + 8u * r) == 0) continue;
        for (U p = ::bof3::Img(Community_Records_addr) + 1u; p < ::bof3::Img(at::kPairsEnd) + 1u; p += 8)
            if (BFieldE2(p - 1) != 0 && BFieldE2(p) == r + 1u) ++named;
        if (named == 0) SetL(::bof3::Img(at::kRecords) + 4u + 8u * r, LFieldE2(::bof3::Img(at::kRecords) + 4u + 8u * r) + 1u);
    }
}

// original 0x5341E0: the tallies GameMode_Field takes on its return from a
// battle (0x4961F2, after Field_EncounterThresholdRoll). Bit 4 of 0x904654 set and
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
    if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(at::kTallyBits)), 4) != 0 && SH_CALL(Flags_Test)(move_script::At(::bof3::Img(at::kTallyBits) + 3), 4) == 0) {
        const unsigned char v = AreaW1c_Byte9045FB;
        if (v < 0x1E) AreaW1c_Byte9045FB = static_cast<unsigned char>(v + 1);
    }
    if (Cond_ByteFA >= 8 && FieldE2_LevelA != 0) {
        U flag = 0xF6;
        bool found = false;
        for (; flag <= 0xFF; ++flag)
            if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), flag) == 0) {
                found = true;
                break;
            }
        if (found) {
            unsigned char a = 0, b = 0;
            for (U p = ::bof3::Img(Community_Records_addr) + 1u; p < ::bof3::Img(at::kPairsEnd) + 1u; p += 8) {
                if (BFieldE2(p - 1) == 0) continue;
                if (BFieldE2(p) == 0xA) ++a;
                else if (BFieldE2(p) == 0xB) ++b;
            }
            const bool first = flag == 0xF6 || flag == 0xF9 || flag == 0xFC;
            if (first) b = a;
            if (b != 0) {
                Rest4a_ClockA = Rest4a_ClockA + 1u;
                int need = kThresholds[flag - 0xF6] - static_cast<int>(BFieldE2(first ? ::bof3::Img(FieldE2_BiasA_addr) : ::bof3::Img(FieldE2_BiasB_addr)));
                if (need < 5) need = 5;
                if (Rest4a_ClockA >= static_cast<U>(need)) {
                    SH_CALL(Flags_Set)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), flag);
                    Rest4a_ClockA = 0;
                }
            }
        }
        if (Rest4a_LevelLit < 7) SH_CALL(Records_CountUnpaired)(1, reinterpret_cast<U*>(move_script::At(::bof3::Img(Rest4a_StampLevelLit_addr))));
        if (FieldE2_LevelA < 0xA) SH_CALL(Records_CountUnpaired)(0, reinterpret_cast<U*>(move_script::At(::bof3::Img(Rest4a_StampLevelDark_addr))));
        SH_CALL(Records_CountUnpairedOfKind)(5);
        SH_CALL(Records_CountUnpairedOfKind)(0xB);
    }
    if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xF5) == 0 && Rest4a_LevelLit >= 4)
        SH_CALL(Flags_Set)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xF5);
}

// original 0x534480 (PSX 0x801C32D0): Field_LeaderStates[14] and the members' 0x65F960[14]:
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
    const auto dx = static_cast<std::int32_t>(LFieldE2(::bof3::Img(at::kObject) + 0x34) - LongAt(s + 0x34));
    const auto dz = static_cast<std::int32_t>(LFieldE2(::bof3::Img(at::kObject) + 0x38) - LongAt(s + 0x38));
    int facing = -1;
    if (dx < 0) facing = dz < 0 ? 0 : dz > 0 ? 6 : 7;
    else if (dx > 0) facing = dz > 0 ? 4 : dz < 0 ? 2 : 3;
    else if (dz > 0) facing = 5;
    else if (dz < 0) facing = 1;
    if (facing >= 0) s[8] = static_cast<unsigned char>(facing);
    Ensure(Sprite_Current[8]);
}

// original 0x534C20 (PSX 0x801C3E2C): the floor's damage of kind k (0..8; Field_FloorDamage's
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
    SH_CALL(Char_LoseHp)(t[0], rv::As<records::PartyMember>(Field_State)->char_id);
    SH_AT(void (__cdecl*)(unsigned, unsigned), at::kHpLoss2.v)(t[1], rv::As<records::PartyMember>(Field_State)->char_id);
    const unsigned char flags = t[2];
    if (flags & 0x80) Field_State[0x124] = 0xA;
    if (flags & 0x20) Field_State[0x125] = 0x28;
    records::PartyMember* fs = rv::As<records::PartyMember>(Field_State);
    U actor = ::bof3::Img(CharacterRecords_addr) + fs->char_index * at::kRecordStride;
    if (rv::As<records::CharRecord>(move_script::At(actor))->hp <= (rv::As<records::CharRecord>(move_script::At(actor))->stats.max_hp >> 2)) {
        rv::As<records::CharRecord>(move_script::At(actor))->status = rv::As<records::CharRecord>(move_script::At(actor))->status | 0x2000u;
        fs->status = fs->status | 0x2000u;
        fs = rv::As<records::PartyMember>(Field_State);
    }
    const U id = fs->char_index;
    actor = ::bof3::Img(CharacterRecords_addr) + id * at::kRecordStride;
    SetW(actor + 0x10, WFieldE2(actor + 0x10) | flags);
    // the member byte as the original pushes it: edx of `lea edx, [id + id * 4]` with dl the byte
    if (SH_CALL(Actor_EquipCount)(((id * 5u) & 0xFFFFFF00u) | id, 3, 8) == 0) return;
    unsigned char* const f = Field_State;
    const U a = ::bof3::Img(CharacterRecords_addr) + f[0x148] * at::kRecordStride;
    rv::As<records::CharRecord>(move_script::At(a))->status = rv::As<records::CharRecord>(move_script::At(a))->status & 0xFFDFu;
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
// answers above 0x40 (s16) with SharedScratch's byte 0x903850 set after it -
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
        return S16FieldE2(static_cast<U>(slope)) > 0x40 && at::kEffectSlot != 0;
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
    int d = S16FieldE2(static_cast<U>(g)) - S16FieldE2(ground);
    if (d < 0) d = -d;
    if (d > 0xC0) return 1;
    unsigned char* const s = Sprite_Current;
    const std::uint16_t kept = rv::As<records::SpriteHead>(s)->height_whole;
    rv::Store<unsigned>(rv::As<records::SpriteHead>(s)->height_whole, ground);
    const unsigned char hit = SH_CALL(Sprite_ObjectAt)(x, z, 1);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, kept);
    return hit != 0xFF ? 1 : 0;
}

// ============================================================================
// The leader's hop and step helpers (FC3's leader states 0x525CE0..0x525F90
// and FE1's 0x52F970..0x52FB50 call them; PSX twins 0x801C5D14..0x801C66DC,
// pairs_propagated "call", each a hypothesis). Each works on Sprite_Current
// and reads it again after every call, as the originals do.
// ============================================================================

// original 0x535FC0 (PSX 0x801C5D14): Sprite_EnsureAnimation(0x3C) facing 7, else (0x3D).
extern "C" void __cdecl Leader_Pose3C(void) { Ensure(rv::Current<records::PartyMember>()->direction == 7 ? 0x3C : 0x3D); }

// The hop's set-up, 0x535FE0 and 0x5363C0 alike: Field_State +0x128 = +0x70
// + 2, Field_JumpSetUp; +0x14 = 0; +9 = (+0x70 + 1) * +9 * 2 and +0xA = +9 / 2
// (bytes); Sprite_EnsureAnimation(+8 + 8), Sprite_ApplyVelocity.
namespace {
void HopSetUp() {
    Field_State[0x128] = static_cast<unsigned char>(Sprite_Current[0x70] + 2);
    SH_CALL(Field_JumpSetUp)();
    rv::Store<U>(rv::Current<records::PartyMember>()->step_y, 0);
    records::PartyMember* s = rv::Current<records::PartyMember>();
    s->step_frames = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(rv::Bytes(s)[0x70] + 1) * s->step_frames) << 1);
    s = rv::Current<records::PartyMember>();
    rv::Bytes(s)[0xA] = static_cast<unsigned char>(s->step_frames >> 1);
    Ensure(static_cast<unsigned char>(rv::Current<records::PartyMember>()->direction + 8));
    SH_CALL(Sprite_ApplyVelocity)();
}
}  // namespace

// original 0x535FE0 (PSX 0x801C5D50): the hop's set-up (HopSetUp), +9 - 1.
extern "C" void __cdecl Leader_HopStart(void) {
    HopSetUp();
    rv::Current<records::PartyMember>()->step_frames = static_cast<unsigned char>(rv::Current<records::PartyMember>()->step_frames - 1);
}

// original 0x5363C0 (PSX 0x801C6394): nothing (al 0) until Sprite_ScriptTickOnce answers; then
// the hop's set-up, +9 - 1 and al 1.
extern "C" unsigned char __cdecl Leader_HopStartAfterTick(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return 0;
    HopSetUp();
    rv::Current<records::PartyMember>()->step_frames = static_cast<unsigned char>(rv::Current<records::PartyMember>()->step_frames - 1);
    return 1;
}

// original 0x536050 (PSX 0x801C5E0C): the hop in flight. +9 at 0: Sprite_EnsureAnimation(+8)
// (the record read on entry), +0xA = 4, Sprite_ScriptTick, al 1. Else +9 - 1,
// Sprite_ApplyVelocity, and at +9 == +0xA (half way) the member slot's CLUT
// word 0x8113BE + +5 * 0x40 = 0 with Gfx_ClutStripDirty; Sprite_ScriptTick,
// al 0.
extern "C" unsigned char __cdecl Leader_HopFlight(void) {
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    if (s->step_frames == 0) {
        Ensure(s->direction);
        Sprite_Current[0xA] = 4;
        SH_CALL(Sprite_ScriptTick)();
        return 1;
    }
    s->step_frames = static_cast<unsigned char>(s->step_frames - 1);
    SH_CALL(Sprite_ApplyVelocity)();
    unsigned char* const t = Sprite_Current;
    if (t[9] == t[0xA]) {
        Gfx_ClutStripDirty = 1;
        SetW(ClutWord(t), 0);
    }
    SH_CALL(Sprite_ScriptTick)();
    return 0;
}

// original 0x5360C0 (PSX 0x801C5EBC): +0xA - 1; at 0: Sprite_EnsureAnimation(0x37 facing 3,
// else 0x34), the facing +8 ^= 4, al 1. Else Sprite_EnsureAnimation(7 - +0xA
// facing 3, else +0xA + 1), +9 = 1, al 0.
extern "C" unsigned char __cdecl Leader_HopPose(void) {
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    const records::PartyMember* const s = rv::Current<records::PartyMember>();
    const unsigned char left = rv::Bytes(s)[0xA];
    if (left == 0) {
        Ensure(s->direction == 3 ? 0x37 : 0x34);
        rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>(rv::Current<records::PartyMember>()->direction ^ 4);
        return 1;
    }
    Ensure(s->direction == 3 ? static_cast<unsigned char>(7 - left) : static_cast<unsigned char>(left + 1));
    rv::Current<records::PartyMember>()->step_frames = 1;
    return 0;
}

// original 0x536130 (PSX 0x801C5F80): nothing (al 0) until Sprite_ScriptTickOnce answers; then
// Sprite_EnsureAnimation(0x3E facing 7, else 0x3F), al 1.
extern "C" unsigned char __cdecl Leader_Pose3E(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return 0;
    Ensure(rv::Current<records::PartyMember>()->direction == 7 ? 0x3E : 0x3F);
    return 1;
}

// original 0x536170 (PSX 0x801C5FD8): a step up. The point ahead (Field_DirectionSteps[+8] *
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
    const std::uint16_t height = rv::As<records::PartyMember>(s)->height_whole;
    const U actor = rv::As<records::PartyMember>(Field_State)->char_id;
    unsigned char result = 0;
    const long ground = SH_CALL(MapView_GroundAt)(x, z);
    if (S16FieldE2(height) > S16FieldE2(static_cast<U>(ground)) - S16FieldE2(WFieldE2(::bof3::Img(at::kPaceHeights) + actor * 2u))) {
        rv::Store<unsigned>(rv::As<records::PartyMember>(s)->height_whole, static_cast<unsigned>(height) - BFieldE2(::bof3::Img(at::kPaceRises) + actor));
    } else {
        const long again = SH_CALL(MapView_GroundAt)(x, z);
        const std::uint16_t offset = WFieldE2(::bof3::Img(at::kPaceHeights) + rv::As<records::PartyMember>(Field_State)->char_id * 2u);
        rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, static_cast<U>(again) - offset);
        Ensure(rv::Current<records::PartyMember>()->direction == 7 ? 0x3C : 0x3D);
        result = 1;
    }
    FollowAndTick(Sprite_Current);
    return result;
}

// original 0x536290 (PSX 0x801C61A8): the member slot's CLUT word 0x8113BE + +5 * 0x40 = 0 with
// Gfx_ClutStripDirty, then Sprite_EnsureAnimation(0x37 facing 7, else 0x34).
extern "C" void __cdecl Leader_Pose34(void) {
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    Gfx_ClutStripDirty = 1;
    SetW(ClutWord(rv::Bytes(s)), 0);
    Ensure(s->direction == 7 ? 0x37 : 0x34);
}

// original 0x5362D0 (PSX 0x801C6204): a step down. The point ahead as Leader_StepUp's and the
// ground there: with the height +0x3E (as read on entry) below it (s16), the
// height + the rise 0x6697A4[Field_State +0x89 read after the call] into the
// cell read on entry, al 0; else the ground again into +0x3E of
// Sprite_Current read again, Sprite_EnsureAnimation(0x3E facing 7, else
// 0x3F), al 1. Then FollowAndTick.
extern "C" unsigned char __cdecl Leader_StepDown(void) {
    unsigned char* const s = Sprite_Current;
    std::int32_t x, z;
    Ahead(s, x, z);
    const std::uint16_t height = rv::As<records::PartyMember>(s)->height_whole;
    unsigned char result = 0;
    const long ground = SH_CALL(MapView_GroundAt)(x, z);
    if (S16FieldE2(height) < S16FieldE2(static_cast<U>(ground))) {
        rv::Store<unsigned>(rv::As<records::PartyMember>(s)->height_whole, BFieldE2(::bof3::Img(at::kPaceRises) + rv::As<records::PartyMember>(Field_State)->char_id) + static_cast<unsigned>(height));
    } else {
        const long again = SH_CALL(MapView_GroundAt)(x, z);
        rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, static_cast<U>(again));
        Ensure(rv::Current<records::PartyMember>()->direction == 7 ? 0x3E : 0x3F);
        result = 1;
    }
    FollowAndTick(Sprite_Current);
    return result;
}

// original 0x536440 (PSX 0x801C646C): the hop's fall. +9 (the record read on entry) at 0:
// Sprite_EnsureAnimation(+8), Sprite_ClearSteps, Field_State +0x137 = 0, the
// states +1 = 1, +2 = +3 = +4 = 0, al 1. Else +9 - 1, Sprite_ApplyVelocity,
// at +9 == +0xA the member's palette (Sprite_LoadPalette(0x80D380 + +5 *
// 0x40, 0)), Sprite_ScriptTick, al 0.
extern "C" unsigned char __cdecl Leader_HopFall(void) {
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    if (s->step_frames == 0) {
        Ensure(s->direction);
        SH_CALL(Sprite_ClearSteps)();
        Field_State[0x137] = 0;
        rv::Current<records::PartyMember>()->state = 1;
        rv::Current<records::PartyMember>()->step = 0;
        rv::Current<records::PartyMember>()->state3 = 0;
        rv::Current<records::PartyMember>()->state4 = 0;
        return 1;
    }
    s->step_frames = static_cast<unsigned char>(s->step_frames - 1);
    SH_CALL(Sprite_ApplyVelocity)();
    const unsigned char* const t = Sprite_Current;
    if (t[9] == t[0xA]) SH_CALL(Sprite_LoadPalette)(Palette(t), 0);
    SH_CALL(Sprite_ScriptTick)();
    return 0;
}

// original 0x5364D0 (PSX 0x801C6550): the height +0x3E up 0x10; the ground ahead (as
// Leader_StepUp's point) less the offset of Field_State +0x89: with the
// height at or above it (s16), the state +2 = 3. Then a tail jump to
// Sprite_ScriptTick, whose al it answers.
extern "C" unsigned char __cdecl Leader_Rise(void) {
    rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, rv::Current<records::PartyMember>()->height_whole + 0x10u);
    std::int32_t x, z;
    Ahead(Sprite_Current, x, z);
    const long ground = SH_CALL(MapView_GroundAt)(x, z);
    const int floor = S16FieldE2(static_cast<U>(ground)) - S16FieldE2(WFieldE2(::bof3::Img(at::kPaceHeights) + rv::As<records::PartyMember>(Field_State)->char_id * 2u));
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    if (S16FieldE2(s->height_whole) >= floor) s->step = 3;
    return SH_CALL(Sprite_ScriptTick)();
}

// original 0x536550 (PSX 0x801C6618): the height +0x3E down 0x10; with the ground under
// (+0x34, +0x38) above it (s16): +0x3E = the ground, Sprite_SetAnimation(+8),
// the member's palette, Field_State +0x137 = 0, the states +1 = 1, +2 = 0.
// Then a tail jump to Sprite_ScriptTick, whose al it answers.
extern "C" unsigned char __cdecl Leader_Sink(void) {
    rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, rv::Current<records::PartyMember>()->height_whole - 0x10u);
    unsigned char* s = Sprite_Current;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(LongAt(s + 0x34)), static_cast<long>(LongAt(s + 0x38)));
    s = Sprite_Current;
    if (S16FieldE2(static_cast<U>(ground)) > S16FieldE2(rv::As<records::PartyMember>(s)->height_whole)) {
        rv::Store<unsigned>(rv::As<records::PartyMember>(s)->height_whole, static_cast<U>(ground));
        SH_CALL(Sprite_SetAnimation)(rv::Current<records::PartyMember>()->direction);
        SH_CALL(Sprite_LoadPalette)(Palette(Sprite_Current), 0);
        Field_State[0x137] = 0;
        rv::Current<records::PartyMember>()->state = 1;
        rv::Current<records::PartyMember>()->step = 0;
    }
    return SH_CALL(Sprite_ScriptTick)();
}

// original 0x5365D0 (PSX 0x801C66DC): the facing +8 ^= 4, Sprite_EnsureAnimation(0x3A facing
// 3, else 0x3B), the member's palette, Sprite_ClearSteps, +0x20 = -8, the
// states +1 = 2, +2 = 3, +3 = 3.
extern "C" void __cdecl Leader_TurnBack(void) {
    rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>(rv::Current<records::PartyMember>()->direction ^ 4);
    Ensure(rv::Current<records::PartyMember>()->direction == 3 ? 0x3A : 0x3B);
    SH_CALL(Sprite_LoadPalette)(Palette(Sprite_Current), 0);
    SH_CALL(Sprite_ClearSteps)();
    rv::Store<U>(rv::Current<records::PartyMember>()->work_20, 0xFFFFFFF8u);
    rv::Current<records::PartyMember>()->state = 2;
    rv::Current<records::PartyMember>()->step = 3;
    rv::Current<records::PartyMember>()->state3 = 3;
}

// ============================================================================
// The party-set error screen
// ============================================================================

// original 0x536A60 (PSX 0x8016776C): PartySet_Find's end when no row holds the three ids -
// never returns: every frame the title line (0x660C90) at (0xA0, 0x64) and
// the three ids PartySet_Find left at 0x903850..0x903852, formatted by the
// string at 0x660C7C into the text scratch 0x904BA0, at (0xA0, 0x74) - both
// Text_DrawFont8, colour 0 -, then Task_Sleep(1). The two low ids come from
// one dword read after the third's byte. PSX twin 0x8016776C (callers, a
// hypothesis).
extern "C" void __cdecl PartySet_ErrorLoop(void) {
    for (;;) {
        SH_CALL(Text_DrawFont8)(0xA0, 0x64, 0, move_script::At(::bof3::Img(at::kErrorTitle)));
        const U third = at::kErrorIds.b_02;
        const U ids = static_cast<std::uint32_t>(at::kErrorIds.d_00);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(move_script::At(::bof3::Img(at::kTextScratch))), reinterpret_cast<const char*>(move_script::At(::bof3::Img(at::kErrorFormat))),
                             ids & 0xFF, (ids >> 8) & 0xFF, third);
        SH_CALL(Text_DrawFont8)(0xA0, 0x74, 0, move_script::At(::bof3::Img(at::kTextScratch)));
        SH_CALL(Task_Sleep)(1);
    }
}

// ============================================================================
// The mode-11 object (0x905DA0; Game_Mode 11, which Field_Request 9 enters,
// runs Mode11_ObjectFrame then 0x5172C0 each frame: docs/mode-tasks.md)
// ============================================================================

// original 0x536B60 (PSX 0x80167CBC): Field_InputHeld = the word Input_Held, Sprite_Current =
// the object 0x905DA0, Field_State = ObjTrio's first record, then a jump
// through Mode11_ObjectStates 0x660C2C by its state +1 (4 entries,
// unchecked: past them ours aborts where the original jumps into data).
// Called by mode 11's step GameMode11_Frame 0x496790. PSX twin 0x80167CBC (call, a
// hypothesis).
extern "C" void __cdecl Mode11_ObjectFrame(void) {
    const std::uint16_t held = Input_Held;
    const U state = BFieldE2(::bof3::Img(at::kObject) + 1);
    Sprite_Current = move_script::At(::bof3::Img(FieldEvent_Object905DA0_addr));
    Field_State = rv::Bytes(ObjTrio);
    Field_InputHeld = held;
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAtFieldE2(::bof3::Img(Mode11_ObjectStates_addr), state, "Mode11_ObjectFrame")))();
}

// original 0x536B90: state 0: +5 = 0, +0xB = the leader's flag byte
// (0x802E78), Field_State +0x128 = 3, the position +0x34 / +0x38 / +0x3C from
// the leader's (0x802D74..), the state +1 up by one. Every store through
// Sprite_Current read again.
extern "C" void __cdecl Mode11_ObjectStart(void) {
    Sprite_Current[5] = 0;
    Sprite_Current[0xB] = rv::Bytes(ObjTrio)[0x138];
    Field_State[0x128] = 3;
    rv::Store<U>(rv::Current<records::SpriteHead>()->x, static_cast<U>(ObjTrio[0].x));
    rv::Store<U>(rv::Current<records::SpriteHead>()->z, static_cast<U>(ObjTrio[0].z));
    rv::Store<U>(rv::Current<records::SpriteHead>()->height, static_cast<U>(ObjTrio[0].height));
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
}

namespace {
// The object stops: its velocity +0xC / +0x10 / +0x14 zero, with `steps` +9
// too, and the two script words 0 (MoveScript_F3Divisor, MoveScript_FAWord).
void ObjectStill(unsigned char* s, bool steps) {
    rv::Store<U>(rv::As<records::SpriteHead>(s)->step_x, 0);
    rv::Store<U>(rv::Current<records::SpriteHead>()->step_z, 0);
    rv::Store<U>(rv::Current<records::SpriteHead>()->step_y, 0);
    if (steps) Sprite_Current[9] = 0;
    MoveScript_F3Divisor = 0;
    MoveScript_FAWord = 0;
}
void LeaveRequest() {
    Field_ScriptFlags2 = Field_ScriptFlags2 & 0x1Fu;
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
    if (LFieldE2(::bof3::Img(at::kInputPressed)) & 0x800u) {
        unsigned char* const fs = Field_State;
        const U nx = LongAt(fs + 0x34);
        const std::int32_t dx = AbsDiff(static_cast<std::uint32_t>(Field_Kind2X), nx);
        const std::int32_t dz = AbsDiff(static_cast<std::uint32_t>(Field_Kind2Z), LongAt(fs + 0x38));
        Field_Kind2X = nx;
        Field_Kind2Z = LongAt(fs + 0x38);
        MoveScript_F3Divisor = 0x20;
        const std::int32_t far = dx >= dz ? dx : dz;
        const U n = static_cast<U>(far >> 13) & 0xFF;
        U word = 0;
        if (n != 0) word = static_cast<U>(S16FieldE2(static_cast<U>(rv::As<records::PartyMember>(fs)->height_whole - WFieldE2(::bof3::Img(at::kElevation)))) / static_cast<int>(n));
        MoveScript_FAWord = word;
        fs[0x128] = 3;
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
        Sprite_Current[1] = 3;
        return;
    }
    if (Field_InputHeld & WFieldE2(::bof3::Img(at::kButtonsLeave))) {
        Field_State[0x128] = 3;
        Game_Step = Game_Step + 1u;
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
    bool near = AbsDiff(static_cast<U>(ObjTrio[0].x) - LFieldE2(::bof3::Img(at::kDirectionSteps) + dir * 8u), LongAt(s + 0x34)) <= 0x60000;
    if (near)
        near = AbsDiff(static_cast<U>(ObjTrio[0].z) - LFieldE2(::bof3::Img(at::kDirectionSteps) + 4u + dir * 8u), LongAt(s + 0x38)) <= 0x60000 && Field_Request != 5;
    if (!near) {
        ObjectStill(s, true);
        Sprite_Current[1] = 1;
        LeaveRequest();
        return;
    }
    const U running = (WFieldE2(::bof3::Img(at::kButtonsRun)) & Field_InputHeld) != 0 ? 1u : 0u;
    Field_State[0x128] = (running ^ FieldE2_RunDefault) != 0 ? 4 : 3;
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
    if (LFieldE2(::bof3::Img(at::kInputHeld)) & 0xF000u) {
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
    if (Field_Kind2Hold != 0) return;
    SH_CALL(MapView_SetElevation)(static_cast<short>(rv::As<records::PartyMember>(Field_State)->height_whole));
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
    const unsigned char flags = FieldEvent_Object905DA0;
    Sprite_Current = move_script::At(::bof3::Img(FieldEvent_Object905DA0_addr));
    if (flags & 0x40) return;
    alignas(4) short vertex[4];
    vertex[0] = Place(LFieldE2(::bof3::Img(at::kObject) + 0x34));
    vertex[1] = Place(LFieldE2(::bof3::Img(at::kObject) + 0x38));
    vertex[2] = static_cast<short>(-((S16FieldE2(WFieldE2(::bof3::Img(at::kObject) + 0x3E)) + 0x80) / 2));
    vertex[3] = 0;
    alignas(4) unsigned char screen[8];
    long depth, flag;
    const long z = SH_CALL(Gte_RotTransPers)(vertex, reinterpret_cast<unsigned long*>(screen), &depth);
    static_cast<void>(flag);
    SetL(Key(Sprite_Current) + 0x60, static_cast<U>(z));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_x, Ftol(screen));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_y, Ftol(screen + 4));
    unsigned char* prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gte_PrimDepthFlat4_10)(prim);
    FildFstFadd(S16FieldE2(static_cast<std::uint16_t>(rv::Current<records::SpriteHead>()->screen_x)) - 6, prim + 8, move_script::At(at::kQuadLift), prim + 0x18);
    std::memcpy(prim + 0x28, prim + 8, 4);
    std::memcpy(prim + 0x38, prim + 0x18, 4);
    U counter = Frame_Counter;
    const U bob = (counter >> 1) & 3;
    const U y = (counter & 8) ? bob + static_cast<U>(S16FieldE2(static_cast<std::uint16_t>(rv::Current<records::SpriteHead>()->screen_y))) - 0x1Bu
                              : static_cast<U>(S16FieldE2(static_cast<std::uint16_t>(rv::Current<records::SpriteHead>()->screen_y))) - bob - 0x18u;
    Fild64Fstp(y, prim + 0xC);
    std::memcpy(prim + 0x1C, prim + 0xC, 4);
    FldFaddFstFstp(prim + 0xC, move_script::At(at::kQuadWidth), prim + 0x2C, prim + 0x3C);
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
    rv::As<records::GpuPolyFT4>(prim)->tpage = 0xF;
    rv::As<records::GpuPolyFT4>(prim)->clut = 0x7802;
    CommitAtObject(0x48);
    SH_CALL(Gte_PushMatrix)();
    const unsigned char* const s = Sprite_Current;
    alignas(4) short angles[4] = {0, 0, 0, 0};
    alignas(4) short move[4];
    move[0] = Place(LongAt(s + 0x34));
    move[1] = Place(LongAt(s + 0x38));
    move[2] = static_cast<short>(-(S16FieldE2(rv::As<records::SpriteHead>(s)->height_whole) / 2));
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
    short* const v = reinterpret_cast<short*>(Prim_VertexScratch);
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

// ============================================================================
// The field tail: seven of Field_ModeTailKinds' slots (Field_ModeTailRun
// jumps through them by the s8 0x9039F3 each field frame). Each is a state
// machine on the s8 0x9039F4 with an argument byte 0x9039F5; the area code
// arms them (kind, state 0, argument). A state out of range does nothing.
// No PSX twin paired for any.
// ============================================================================

// original 0x56DE50 (slot 44): 0 - unless a message is up: bit (argument) of
// 0x904650 set, the system message 0x4172 + argument copied (8 bytes, the
// CRT's copy) into Text_Records, Msg_OpenSystem(0xFA), Field_Request 2,
// Sound_StopMusic, Sound_LoadStream(9), state 1; 1 - once Sound_StreamDone
// and no message is up, Sound_ResumeAll, the byte 0x903804 points at 0,
// ScriptFlags_Clear40, the tail cleared. Bit (argument) of 0x904650 is the
// dragon gene's (Gene_Bits) and message 0xFA names it.
//
// DIVERGENCE DIV-0166 (known-defects D351): the copy (`push 8` ... `call
// 0x5B9450` at 0x56DED1..0x56DEE2) is strncpy, so a name of 8 bytes or more
// leaves Text_Records' first record unterminated and the message prints on
// into whatever an earlier copy left at +8. The US disc (GAME.EMI#0
// 0x801A83DC; a `sb $zero` to the buffer + 8 after the copy) terminates it;
// the JP (0x801A93E4) does not. With g_name_nul (1 through
// PatchBytes("GeneNameNul") in FieldE2_Inject, after the self-test) the byte
// at +8 is 0 after the copy, as on the US disc.
namespace field_e2 { std::uint8_t g_name_nul = 0; }

// ============================================================================
// Draw layers: three MapCell_Handlers kinds (DrawLayer_Open calls each with
// the cell's record, its byte 1 and byte 0), and a map cell cleared
// ============================================================================

// ============================================================================
// The trade screen (ItemTrade_States 0x66A470, dispatched by 0x593950 on
// 0x93985C - nobody's): a list of entries (0x66AD10's row 0x905B88, ten
// record numbers) of the 8-byte records at 0x66AB58 (+0 the item, +1 its
// category, +2..+4 three ingredients, +5..+7 their amounts); a count to
// choose; a yes / no. The state handlers are hidden starts in
// Sprite_ClutWord's extent. PSX twins 0x800F5048..0x800F5828 (table-
// anchored and callers, each a hypothesis).
// ============================================================================

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

// ============================================================================

void FieldE2_Inject() {
    if (bof3::WantsShadow("field_e2")) field_e2::SelfTest();
    {
        // DIV-0166: after the self-test, which compared Capcom's unterminated copy.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("GeneNameNul", Low32(&field_e2::g_name_nul), &was, &is, 1);
        bof3::Log("DIV-0166    message 0xFA's gene name is terminated after its 8-byte copy (on unless the line above says OFF)");
    }
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
    // DIVERGENCE DIV-0027 (amended 2026-10-03): after the self-test, which
    // compares the original's prompt; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("TradeConfirmLayout", Low32(&g_trade_confirm_layout),
                         &was, &is, 1);
        bof3::Log("DIV-0027    Manillo's \"Is <item> OK?\": answers re-spaced, the hand two units left of each");
    }
}
