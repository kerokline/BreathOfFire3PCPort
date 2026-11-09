// Moved here by tools/move_funcs.py from field_e2.cpp, field_event.cpp, map_view.cpp, rest_2a.cpp, scena_sx.cpp (docs/source-layout-round.md).
#include "game/field/mode_11.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/field/mode_11_callees.h"
#include "game/field_e2.h"
#include "game/field_e2_callees.h"
#include "game/field_event.h"
#include "game/field_event_callees.h"
#include "game/lang_layout.h"
#include "game/map_rim.h"
#include "game/map_view.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2a.h"
#include "game/rest_2a_callees.h"
#include "game/scena_sx.h"
#include "game/scena_sx_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = mode_11::at;

}  // namespace

namespace {

using namespace move_script;

}  // namespace

namespace {

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

U Key(const void* p) { return Low32(p); }

// The record views (game/record_views.h): the leader helpers run on the leader,
// an ObjTrio record, and Field_State is one.
U LongAt(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

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

constexpr auto& gFieldEvent = field_event::g;

std::uint32_t Address(const volatile void* p) { return Low32(p); }

unsigned char* Member(unsigned i) { return rv::Bytes(&ObjTrio[i]); }

using move_script::At;

using move_script::Long;

using move_script::Word;

// CharacterRecords' record n (stride 0xA4), unchecked as the originals index it.
unsigned char* Record(unsigned n) { return At(::bof3::Img(CharacterRecords_addr) + n * rest_2a::at::kRecordStride); }

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* RecordScenaSx(unsigned n) { return move_script::At(::bof3::Img(CharacterRecords_addr) + n * scena_sx::at::kRecordStride); }

// A member id's record: CharacterRecords[Character_IndexById[id]], both
// indexes a whole byte, unchecked (the original reads them the same way).
unsigned char* MemberRecord(unsigned char id) { return RecordScenaSx(Character_IndexById[id]); }

}  // namespace

// original 0x5366A0: 32 words of palette `index` from Sprite_Current's bank to
// dst and to its twin in Gfx_ClutStrip, and the strip marked dirty.
//
// As the original has it: the twin is 0x80F580 + ((dst - 0x80B580) >> 1) * 2,
// an arithmetic shift - an odd dst loses its low bit there, a dst below the
// source strip lands below the strip. Each word is read, then stored to dst,
// then to the twin. Nothing is checked: the bank pointer, the index, dst.
extern "C" void __cdecl Sprite_LoadPalette(unsigned short* dst, unsigned index) {
    const unsigned char* const bank = At(static_cast<std::uint32_t>(Long(Sprite_Current + 0x4C)));   // a 32-bit word
    const unsigned char* const offsets = bank + Long(bank + 4);
    const std::uint32_t d = Address(dst);
    const auto half = static_cast<std::int32_t>(d - ::bof3::Img(Gfx_ClutStripSource_addr)) >> 1;
    unsigned char* twin = At(::bof3::Img(Gfx_ClutStrip_addr) + static_cast<std::uint32_t>(half) * 2u);
    const unsigned char* from = offsets + Long(offsets + (index & 0xFF) * 4u);
    auto* to = reinterpret_cast<unsigned char*>(dst);
    for (int k = 0; k < 32; ++k) {
        const std::uint16_t w = Word(from);
        from += 2;
        SetWord(to, w);
        SetWord(twin, w);
        to += 2;
        twin += 2;
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x536700 (PSX 0x80167628). The area map's byte for a cell. No bounds check; the
// arguments are 16 bits, signed.
extern "C" unsigned char __cdecl AreaMap_ByteAt(short x, short y) {
    return AreaMap_Bytes[x + y * static_cast<int>(AreaMap_Header[0])];
}

// original 0x536730 (PSX 0x80167664): member `member`'s bytes +1..+4 cleared.
// As the original has it: member is unchecked.
extern "C" void __cdecl Member_ClearState(unsigned member) {
    records::PartyMember* const m = rv::As<records::PartyMember>(Member(member & 0xFF));
    m->state = 0;
    m->step = 0;
    m->state3 = 0;
    m->state4 = 0;
}

// original 0x536760 (PSX 0x801676AC): all three members off, their state cleared.
extern "C" void __cdecl Party_ClearAll(void) {
    for (unsigned i = 0; i < 3; ++i) {
        Member(i)[0] = 0;
        gFieldEvent.clear_state(i);
    }
}

// original 0x5367A0 (PSX 0x80167704): the state of each member that is on cleared.
extern "C" void __cdecl Party_ClearActive(void) {
    for (unsigned i = 0; i < 3; ++i)
        if (Member(i)[0] != 0) gFieldEvent.clear_state(i);
}

// original 0x5367E0: the party set for a, b, c loaded - first with mode 3
// when `mode` is 0 - each load waited out a frame at a time.
//
// As the original has it: File_LoadDone is asked before the first sleep, so
// a load already done costs no frame.
extern "C" void __cdecl PartySet_Load(unsigned a, unsigned b, unsigned c, unsigned mode) {
    if ((mode & 0xFF) == 0) {
        gFieldEvent.load_first(a, b, c);
        if (gFieldEvent.load_done() == 0) {
            do gFieldEvent.sleep(1);
            while (gFieldEvent.load_done() == 0);
        }
    }
    gFieldEvent.load_second(a, b, c, mode);
    if (gFieldEvent.load_done() == 0) {
        do gFieldEvent.sleep(1);
        while (gFieldEvent.load_done() == 0);
    }
}

// original 0x536850: the set for a, b, c selected with mode 3 (its file
// without recording it) unless it is the current one.
extern "C" void __cdecl PartySet_LoadFirst(unsigned a, unsigned b, unsigned c) {
    const unsigned char set = gFieldEvent.find(a, b, c);
    if ((AreaW0b_PartySet & 0x7F) != set) gFieldEvent.select(set, 3);
}

// original 0x536890: the set for a, b, c selected with `mode` unless
// 0x90412C already holds it as that mode would record it.
extern "C" void __cdecl PartySet_LoadSecond(unsigned a, unsigned b, unsigned c, unsigned mode) {
    const unsigned char set = gFieldEvent.find(a, b, c);
    const unsigned want = (mode & 0xFF) != 0 ? set : (set | 0x80u);
    if (AreaW0b_PartySet != want) gFieldEvent.select(set, mode);
}

// original 0x5368F0: the party-set row for the members a, b, c (0xFF for
// none). The current set 0x90412C if it is not 0xFF and holds every one of
// them; else the three sorted and looked up column by column in the 19
// sorted rows at 0x669750, narrowing the rows at each match; none found calls
// 0x536A60 (which prints the three and never returns) and gives 0xFF.
//
// As the original has it: the three ids and the sort's swap byte are left at
// 0x903850..0x903854; the match is tested against count - 1 where count is
// the ids that are not 0xFF, so all three 0xFF can never match; the matched
// count indexes the ids unchecked; the row is returned as a byte, and a row
// of 0xFF would go on to the next column (unreachable: rows stop at 0x13).
extern "C" unsigned char __cdecl PartySet_Find(unsigned a, unsigned b, unsigned c) {
    unsigned char* const ids = At(::bof3::Img(at::kFindIds));
    const unsigned char current = AreaW0b_PartySet;
    ids[0] = static_cast<unsigned char>(a);
    ids[1] = static_cast<unsigned char>(b);
    ids[2] = static_cast<unsigned char>(c);
    if (current != 0xFF) {
        unsigned i = 0;
        for (; i < 3; ++i) {
            const unsigned char id = ids[i];
            if (id == 0xFF) continue;
            const unsigned char* const row = At(::bof3::Img(MoveScript_EffectArg_addr) + 0xc) + (current & 0x7Fu) * 3u;
            unsigned j = 0;
            while (id != row[j])
                if (++j >= 3) break;
            if (j >= 3) break;
        }
        if (i >= 3) return static_cast<unsigned char>(current & 0x7F);
    }
    unsigned char count = 0;
    for (unsigned k = 0; k < 3; ++k)
        if (ids[k] != 0xFF) ++count;
    unsigned char* const swap = ids + 4;
    for (unsigned p = 0; p < 2; ++p)
        for (unsigned q = p + 1; q < 3; ++q) {
            const unsigned char low = ids[p];
            if (low > ids[q]) {
                *swap = low;
                ids[p] = ids[q];
                ids[q] = *swap;
            }
        }
    unsigned char limit = 0x13, start = 0, result = 0xFF;
    unsigned matched = 0;
    const unsigned char* const table = At(::bof3::Img(MoveScript_EffectArg_addr) + 0xc);
    for (unsigned column = 0; column < 3; ++column) {
        unsigned row = start;
        if (row >= limit) continue;
        for (;;) {
            const unsigned char* cell = table + row * 3u + column;
            const unsigned char want = ids[matched];
            if (want == *cell) {
                if (matched == count - 1u) {
                    result = static_cast<unsigned char>(row);
                    if (result != 0xFF) return result;
                    break;
                }
                start = static_cast<unsigned char>(row);
                while (row < 0x13 && want == *cell) {
                    ++row;
                    cell += 3;
                }
                limit = static_cast<unsigned char>(row);
                ++matched;
            }
            ++row;
            if (row >= limit) break;
        }
    }
    gFieldEvent.set_error();
    return result;
}

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

// original 0x536AC0 (PSX 0x80167C04): party set `set` recorded in 0x90412C
// (with bit 7 for mode 0, untouched for mode 3) and one of its four files
// loaded - LoadDatFile(set + 0x2C2 / 0xFC / 0x10F / 0x2D5) for mode 0..3;
// above 3, nothing is loaded.
extern "C" void __cdecl PartySet_Select(unsigned set_arg, unsigned mode_arg) {
    const unsigned set = set_arg & 0xFF, mode = mode_arg & 0xFF;
    if (mode != 3) *At(::bof3::Img(AreaW0b_PartySet_addr)) = static_cast<unsigned char>(mode == 0 ? set | 0x80 : set);
    switch (mode) {
    case 0: gFieldEvent.load_dat(set + 0x2C2); break;
    case 1: gFieldEvent.load_dat(set + 0xFC); break;
    case 2: gFieldEvent.load_dat(set + 0x10F); break;
    case 3: gFieldEvent.load_dat(set + 0x2D5); break;
    default: break;
    }
}

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

// original 0x5372E0 (PSX 0x80168778, pairs_propagated gap44, a hypothesis):
// for each of the 30 Sprite_Objects records with +0 bit 0, Sprite_Current =
// the record and: type +6 not 9 - draw slot +0x29 = 4 with +0x24 bit 4, else
// Draw_OtSlot; type 9 - nothing more when Draw_OtSlot is 4. Then type 0xA:
// Sprite_UpdateScreenA. Otherwise the first of the bank list's records
// (count byte 0x8C3580, the records at [0x7E0880], 8 bytes each) whose +7
// equals the sprite's u16 +0x2C (zero-extended); none: the record at the
// count (one past, read as the original reads it). Its bank word +0 in
// Mode11_ScreenBanks (to its 0xFFFF): Sprite_UpdateScreen. Sprite_Current is
// left at the last live record.
extern "C" void __cdecl Mode11_ListedSpriteScreens(void) {
    for (unsigned k = 0; k < 30; ++k) {
        unsigned char* const rec = rv::Bytes(&Sprite_Objects[k]);
        Sprite_Current = rec;
        if (!(rec[0] & 1)) continue;
        if (rec[6] != 9) {
            rec[0x29] = (rec[0x24] & 0x10) ? 4 : Draw_OtSlot;
        } else if (Draw_OtSlot == 4) {
            continue;
        }
        const unsigned char* const s = Sprite_Current;
        if (s[6] == 0xA) {
            SH_CALL(Sprite_UpdateScreenA)();
            continue;
        }
        const unsigned count = At(::bof3::Img(at::kBankCount.v))[0];
        const unsigned char* const records = reinterpret_cast<const unsigned char*>(
            static_cast<std::uintptr_t>(static_cast<U>(Long(At(::bof3::Img(MoveScript_TintRecords_addr) + 0x180)))));
        const unsigned bank_byte = rv::As<records::SpriteObject>(s)->column;
        unsigned i = 0;
        while (i < count && records[i * 8 + 7] != bank_byte) ++i;
        const unsigned bank = Word(records + (i & 0xFF) * 8);
        for (unsigned j = 0;; ++j) {
            const unsigned listed = Mode11_ScreenBanks[j];
            if (listed == 0xFFFF) break;
            if (listed == bank) {
                SH_CALL(Sprite_UpdateScreen)();
                break;
            }
        }
    }
}

// original 0x5373F0 (PSX 0x80168954, pairs_propagated call-anchored): the
// member byte's record (Character_IndexById, unchecked, as Char_LoseHp):
// with HP +0x18 below its maximum +0x20 (u16), HP = (HP + amount) as a u16,
// held at the maximum (a sum past 0xFFFF wraps first, as the original's
// 16-bit compare has it). Then, only then, the record Field_State +0x148
// names: HP above a quarter of its maximum (>> 2) clears 0x2000 of its state
// word +0x10 and of Field_State's word +0x90. Its eax is no answer (a
// record offset); Field_EquipTick reads none.
extern "C" void __cdecl Char_GainHp(unsigned amount, unsigned member) {
    const unsigned slot = Character_IndexById[member & 0xFF];
    unsigned char* const rec = Record(slot);
    const unsigned max = rv::As<records::CharRecord>(rec)->stats.max_hp;
    const unsigned hp = rv::As<records::CharRecord>(rec)->hp;
    if (hp >= max) return;
    const unsigned sum = (hp + amount) & 0xFFFF;
    rv::As<records::CharRecord>(rec)->hp = sum > max ? max : sum;
    unsigned char* const fs = Field_State;
    unsigned char* const actor = Record(fs[0x148]);
    if (rv::As<records::CharRecord>(actor)->hp <= (rv::As<records::CharRecord>(actor)->stats.max_hp >> 2)) return;
    rv::As<records::CharRecord>(actor)->status = rv::As<records::CharRecord>(actor)->status & 0xDFFF;
    rv::As<records::PartyMember>(fs)->status = rv::As<records::PartyMember>(fs)->status & 0xDFFF;
}

// original 0x537480 (PSX 0x80168AB4): a member's HP (+0x18 of its record: the second
// argument's low byte through Character_IndexById, unchecked) down by the
// first argument's word while it is more; else to 1 (an HP of 0 becomes 1
// too). Then the record Field_State +0x148 names (not the member's): with HP
// at most a quarter of +0x20 (>> 2), its state word +0x10 and Field_State's
// word +0x90 |= 0x2000. Answers what it took: the first argument whole, or
// HP - 1 (0xFFFFFFFF at HP 0); Field_Bit80Tick reads ax. PSX twin 0x80168AB4
// (pairs_propagated "call").
extern "C" std::uint32_t __cdecl Char_LoseHp(std::uint32_t amount, unsigned member_arg) {
    unsigned char* const rec = MemberRecord(static_cast<unsigned char>(member_arg));
    const unsigned hp = rv::As<records::CharRecord>(rec)->hp;
    std::uint32_t took = amount;
    if (hp <= (amount & 0xFFFF)) took = hp - 1;
    rv::Store<unsigned>(rv::As<records::CharRecord>(rec)->hp, hp - took);
    unsigned char* const actor = RecordScenaSx(Field_State[0x148]);
    if (rv::As<records::CharRecord>(actor)->hp <= (rv::As<records::CharRecord>(actor)->stats.max_hp >> 2)) {
        rv::Store<unsigned>(rv::As<records::CharRecord>(actor)->status, rv::As<records::CharRecord>(actor)->status | 0x2000);
        rv::Store<unsigned>(rv::As<records::PartyMember>(Field_State)->status, rv::As<records::PartyMember>(Field_State)->status | 0x2000);
    }
    return took;
}

// original 0x537500 (PSX 0x80168BC0, pairs_propagated call): the member
// byte's record's AP +0x1A down by the amount while it is more (16-bit
// compare), else to 1 (an AP of 0 becomes 1 too). Answers what it took: the
// amount whole, or AP - 1 (0xFFFFFFFF at AP 0). Char_LoseHp without its
// actor test.
extern "C" unsigned __cdecl Char_LoseAp(unsigned amount, unsigned member) {
    const unsigned slot = Character_IndexById[member & 0xFF];
    unsigned char* const rec = Record(slot);
    const unsigned ap = rv::As<records::CharRecord>(rec)->ap;
    unsigned took = amount;
    if (ap <= (amount & 0xFFFF)) took = ap - 1;
    rv::As<records::CharRecord>(rec)->ap = ap - took;
    return took;
}

void Mode11_Inject() {
    BOF3_INJECT(Sprite_LoadPalette);
    BOF3_INJECT(AreaMap_ByteAt);
    BOF3_INJECT(Member_ClearState);
    BOF3_INJECT(Party_ClearAll);
    BOF3_INJECT(Party_ClearActive);
    BOF3_INJECT(PartySet_Load);
    BOF3_INJECT(PartySet_LoadFirst);
    BOF3_INJECT(PartySet_LoadSecond);
    BOF3_INJECT(PartySet_Find);
    BOF3_INJECT(PartySet_ErrorLoop);
    BOF3_INJECT(PartySet_Select);
    BOF3_INJECT(Mode11_ObjectFrame);
    BOF3_INJECT(Mode11_ObjectStart);
    BOF3_INJECT(Mode11_ObjectControl);
    BOF3_INJECT(Mode11_ObjectMove);
    BOF3_INJECT(Mode11_ObjectEnd);
    BOF3_INJECT(Mode11_ObjectHalt);
    BOF3_INJECT(Mode11_ObjectDraw);
    BOF3_INJECT(Mode11_ListedSpriteScreens);
    BOF3_INJECT(Char_GainHp);
    BOF3_INJECT(Char_LoseHp);
    BOF3_INJECT(Char_LoseAp);
}
