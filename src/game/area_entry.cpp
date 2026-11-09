// The area's link list and the drop-in party placement - five functions of
// the field overlay's PC twin, each read to its last instruction against the
// PSX (docs/area-entry.md section 1 has the pairs). The start-up fuzz is
// area_entry_fuzz.cpp.
#include "game/area_entry.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/area_entry_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"

namespace area_entry {

const Callees kOriginals = {
    Flags_Test,         Area_ClassifyPending, Area_PickMusic,     Kind2_Place,       Member_ClearState, ScriptContext_Reset,
    Sprite_LoadPalette, Field_MemberSprite,   Party_SetUpMembers, Party_SwapMembers, ObjTrio_SwapFields,
};
Callees g = kOriginals;

}  // namespace area_entry

namespace {

using namespace move_script;
using namespace area_entry;

std::uint32_t Address(const volatile void* p) { return Low32(p); }
unsigned char* Ptr(const unsigned char* at) { return move_script::At(static_cast<std::uint32_t>(move_script::Long(at))); }
unsigned char* Descriptor() { return Area_Descriptors[Game_AreaNumber]; }
unsigned char* Record(unsigned slot) { return rv::Bytes(ObjTrio) + slot * kRecord; }

}  // namespace

// original 0x531F90 (PSX 0x801BF1A8, call-anchored; read, the same): the
// party placed from entry `entry` of the area's placement table
// (descriptor +0x14). The entry's first byte: N in its low nibble, bit 5
// "set bit 3 of Field_ScriptFlags", bits 4-5 both "then place the kind-2
// object", and a kind in bits 6-7:
//   0     exactly N members (else 1): N wanted members follow - a member
//         number, or 0x80 for "any one not yet taken". For each, the
//         lowest-slot record holding it (ObjTrio +0x89) or the first free
//         one; a wanted member nobody holds is 1. The party list 0x904062
//         is rewritten in the wanted order and the records put in it by
//         Party_SwapMembers, a selection sort; then the members set up from
//         the N bytes after the wanted list.
//   0x80  at least N members (else 1), set up from the N bytes after the
//         header, nothing re-ordered.
//   0x40, 0xC0  no test, set up likewise.
// Then Kind2_Place of the byte after the set-up list, when bits 4-5 are both
// set; Field_ScriptFlags |= 0x100, Field_ScriptFlags2 |= 0x8000; 0.
//
// As the original has it: Party_SetUpMembers gets the member count as it
// is after the swaps (every kind joins at its load, 0x53214D - the fuzz's
// first run caught a misreading of that join). The scratch lists are
// four bytes apart and N is not limited, as on the PSX. The first byte is
// also written over the argument's low byte (no caller reads the slot
// after the call: every one of the 393 sites releases it first - a scan,
// docs/area-entry.md section 2), which ours keeps in a local. The result:
// 1 on a failed kind-0 or kind-0x80 test with the placement table's
// address in the upper 24 bits (1 alone when a wanted member is missing), 0
// otherwise - or, after Kind2_Place, that callee's leftover eax with al
// cleared, which ours does not reproduce (0).
extern "C" unsigned __cdecl Party_DropIn(unsigned entry_arg) {
    const unsigned entry = entry_arg & 0xFFu;
    const auto table = static_cast<std::uint32_t>(move_script::Long(Descriptor() + 0x14));
    const unsigned char* list = Ptr(move_script::At(table) + entry * 4);
    const unsigned char header = list[0];
    const unsigned n = header & 0xFu;
    ++list;
    if ((header & 0xC0) == 0) {
        if (Field_MemberCount != n) return (table & 0xFFFFFF00u) | 1u;
        for (unsigned i = 0; i < n; ++i) move_script::At(::bof3::Img(kPickFree.v))[i] = Cond_Flags[0xd2 + i];
        for (unsigned i = 0; i < n; ++i) {
            const unsigned char wanted = list[i];
            const unsigned members = Field_MemberCount;
            unsigned j = 0;
            if (wanted != 0x80) {
                while (j < members && Record(j)[kRecMember] != wanted) ++j;
                if (j == members) return 1;
            } else {
                while (j < members && move_script::At(::bof3::Img(kPickFree.v))[j] == 0xFF) ++j;
                if (j == members) continue;
            }
            move_script::At(::bof3::Img(kPickColumn.v))[i] = Record(j)[kRecColumn];
            move_script::At(::bof3::Img(kPickSlot.v))[i] = static_cast<unsigned char>(j);
            move_script::At(::bof3::Img(kPickFree.v))[j] = 0xFF;
        }
        for (unsigned i = 0; i < n; ++i) Cond_Flags[0xd2 + i] = Record(move_script::At(::bof3::Img(kPickSlot.v))[i])[kRecMember];
        for (unsigned s = 0; static_cast<int>(s) < static_cast<int>(n) - 1; ++s) {
            const unsigned char there = move_script::At(::bof3::Img(kPickSlot.v))[s];
            if (s == there) continue;
            area_entry::g.swap_members(s, there, 0);
            for (unsigned t = s; t < n; ++t) {
                if (move_script::At(::bof3::Img(kPickSlot.v))[t] == s) {
                    move_script::At(::bof3::Img(kPickSlot.v))[t] = move_script::At(::bof3::Img(kPickSlot.v))[s];
                    break;
                }
            }
        }
        list += n;
    } else if ((header & 0xC0) == 0x80 && Field_MemberCount < n) {
        return (table & 0xFFFFFF00u) | 1u;
    }
    area_entry::g.set_up(Field_MemberCount, list);
    list += n;
    // Byte-wide, as the original: bit 3 and bit 8 of Field_ScriptFlags, bit
    // 15 of Field_ScriptFlags2.
    if (header & 0x20) move_script::At(Address(&Field_ScriptFlags))[0] |= 8;
    if ((header & 0x30) == 0x30) area_entry::g.kind2_place(list[0]);
    move_script::At(Address(&Field_ScriptFlags) + 1)[0] |= 1;
    move_script::At(Address(&Field_ScriptFlags2) + 1)[0] |= 0x80;
    return 0;
}

// original 0x5321C0 (PSX 0x801BF56C, callers pairing; read, the same):
// the first `count` party records set up for the area: each made
// Sprite_Current, bit 5 of +0 cleared, +0x5D..+0x5F 0 and +0x5C 2,
// Member_ClearState(slot), +1 2, +7 and +9 0, its script pointer +0x130 from
// the area descriptor's +0x18 table at the list's byte, its script context
// +0x124 reset, its palette (+5) loaded into palette 0 of the 64-byte block;
// then the low three bits of Field_ScriptFlags2 cleared.
//
// As the original has it: the record pointer is kept across the calls, but
// Sprite_Current is re-read after Member_ClearState and after
// ScriptContext_Reset; the descriptor is looked up afresh for each record;
// the count is a byte and not limited to the three records ObjTrio holds.
extern "C" void __cdecl Party_SetUpMembers(unsigned count_arg, const unsigned char* list) {
    const unsigned count = count_arg & 0xFFu;
    unsigned char* record = rv::Bytes(ObjTrio);
    for (unsigned i = 0; i < count; ++i, record += kRecord) {
        Sprite_Current = record;
        record[0] &= 0xDF;
        Sprite_Current[0x5D] = 0;
        Sprite_Current[0x5E] = 0;
        Sprite_Current[0x5F] = 0;
        Sprite_Current[0x5C] = 2;
        area_entry::g.member_clear(i);
        Sprite_Current[1] = 2;
        Sprite_Current[7] = 0;
        Sprite_Current[9] = 0;
        move_script::SetLong(record + kRecScript, move_script::Long(Ptr(Descriptor() + 0x18) + list[i] * 4u));
        area_entry::g.context_reset(record + kRecContext);
        area_entry::g.load_palette(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(kPalettes.v) + Sprite_Current[kRecPalette] * 64u)), 0);
    }
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF8);
}

// original 0x5322D0 (PSX 0x801BF730, call-anchored; read, the same): party
// records a and b trade places - ObjTrio_SwapFields(a, b, keep), then for a
// and then b: the record made Field_State and Sprite_Current,
// Field_MemberSprite(its member, its slot), its character block +0x80
// (0xA4 bytes) copied from the character record Character_IndexById
// gives for its member, and +0x148 set to that table's byte again.
//
// As the original has it: the second lookup reads +0x89 AFTER the copy,
// which has just overwritten it with the character record's +9 - so +0x148
// is the table's byte for that, not for the member (the PSX's +0x13C the
// same). Field_State is re-read after Field_MemberSprite and after the copy;
// the member is passed with a stale upper half (Field_MemberSprite masks).
extern "C" void __cdecl Party_SwapMembers(unsigned a, unsigned b, unsigned keep) {
    area_entry::g.swap_fields(a, b, keep);
    const unsigned slots[2] = {a, b};
    for (const unsigned slot : slots) {
        unsigned char* const record = Record(slot & 0xFFu);
        Field_State = record;
        Sprite_Current = record;
        area_entry::g.member_sprite(record[kRecMember], slot);
        unsigned char* state = Field_State;
        const unsigned char character = Character_IndexById[state[kRecMember]];
        const unsigned char* const from = move_script::At(::bof3::Img(CharacterRecords_addr) + character * kCharacterBytes);
        for (unsigned k = 0; k < kCharacterBytes; k += 4) move_script::SetLong(state + kRecCharacter + k, move_script::Long(from + k));
        state = Field_State;
        state[kRecCharacterIndex] = Character_IndexById[state[kRecMember]];
    }
}

// original 0x5323E0 (PSX 0x801BF918, call-anchored; read, the same): the
// fields of party records a and b that go with the member rather than the
// slot, exchanged: the member +0x89; unless `keep`, also +0x08, +0x29,
// +0x138 and the position +0x34 / +0x38 / +0x3C; always the three dwords
// +0x0C..+0x14 and the script context's first two bytes +0x124 / +0x125;
// then +0x4B of both 0xFF.
//
// As the original has it: a, b and keep are bytes; nothing limits a and b to
// the three records.
extern "C" void __cdecl ObjTrio_SwapFields(unsigned a, unsigned b, unsigned keep) {
    unsigned char* const ra = Record(a & 0xFFu);
    unsigned char* const rb = Record(b & 0xFFu);
    const auto byte = [&](unsigned at) {
        const unsigned char va = ra[at], vb = rb[at];
        ra[at] = vb;
        rb[at] = va;
    };
    const auto dword = [&](unsigned at) {
        const std::int32_t va = move_script::Long(ra + at), vb = move_script::Long(rb + at);
        move_script::SetLong(ra + at, vb);
        move_script::SetLong(rb + at, va);
    };
    byte(0x89);
    if ((keep & 0xFFu) == 0) {
        byte(0x08);
        byte(0x29);
        byte(0x138);
        dword(0x34);
        dword(0x38);
        dword(0x3C);
    }
    dword(0x0C);
    dword(0x10);
    dword(0x14);
    byte(0x125);
    byte(0x124);
    ra[0x4B] = 0xFF;
    rb[0x4B] = 0xFF;
}

void AreaEntry_Inject() {
    if (bof3::WantsShadow("area_entry")) area_entry::SelfTest();
    BOF3_INJECT(Party_DropIn);
    BOF3_INJECT(Party_SetUpMembers);
    BOF3_INJECT(Party_SwapMembers);
    BOF3_INJECT(ObjTrio_SwapFields);
}
