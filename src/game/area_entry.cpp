// The area's link list and the drop-in party placement - five functions of
// the field overlay's PC twin, each read to its last instruction against the
// PSX (docs/area-entry.md section 1 has the pairs). The start-up fuzz is
// area_entry_fuzz.cpp.
#include "game/area_entry.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
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

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Ptr(const unsigned char* at) { return At(static_cast<std::uint32_t>(Long(at))); }
unsigned char* Descriptor() { return Area_Descriptors[Game_AreaNumber]; }
unsigned char* Record(unsigned slot) { return ObjTrio + slot * kRecord; }

}  // namespace

// original 0x5951D0 (PSX 0x801A1050, gap pairing; read, the same): the area's
// link at cell (x, z), made the pending area change. The descriptor's +0x20
// holds 12-byte links, +0x30 the index of the last: each link is a run of
// +9 cells from (+0, +1) along z when +8 is set, along x when it is not. The
// first link found - searched from the last link down and each run from its
// far end - gives the destination: area word +2, x and z words +4 / +6 and
// the flags byte +0xA; or, when +2 is 0xFFFF, the word +4 picks a list of
// 10-byte alternatives (descriptor +0x24's pointers, +0x28's counts), the
// first whose story flag Flags_Test(Cond_Flags + [6] * 8, [7]) holds or the
// one after the last when none does (area +0, x +2, z +4, flags +8). The
// area, (x & 0xFF80) << 8, (z & 0xFF80) << 8 and flags & 0x8F go where
// Field_ChangeArea puts them, bits 4-6 of the flags to 0x903851, then
// Area_ClassifyPending and Area_PickMusic of what is now pending - but not
// Field_Request 5: the caller decides whether to go. 0 when a link was
// found, 1 when none.
//
// As the original has it: x and z are compared as bytes, the run's start
// plus its index as an int (a run past 0xFF never matches there); the
// descriptor pointer is read once; Area_PickMusic's x and z are the pending
// dwords shifted arithmetically, read back after Area_ClassifyPending, and
// its area is the pending word (the original pushes it with a stale upper
// half, which Area_PickMusic masks). The result is `al` alone - six of the
// twelve call sites test al and the other six (GameMode_Enter among them)
// ignore it; the upper 24 bits are an address in the link list, or whatever
// Area_PickMusic left.
extern "C" unsigned char __cdecl Area_LinkAt(unsigned x_arg, unsigned z_arg) {
    const unsigned x = x_arg & 0xFFu, z = z_arg & 0xFFu;
    unsigned char* const descriptor = Descriptor();
    int n = descriptor[0x30];
    const unsigned char* link = Ptr(descriptor + 0x20) + n * 12;
    for (; n >= 0; --n, link -= 12) {
        for (int i = static_cast<int>(link[9]) - 1; i >= 0; --i) {
            if (link[8] != 0) {
                if (x == link[0] && z == link[1] + static_cast<unsigned>(i)) goto found;
            } else if (x == link[0] + static_cast<unsigned>(i) && z == link[1]) {
                goto found;
            }
        }
    }
    return 1;
found:
    unsigned char flags;
    if (Word(link + 2) != 0xFFFF) {
        SetWord(At(kPendingArea), Word(link + 2));
        SetLong(At(kPendingX), static_cast<std::int32_t>((Word(link + 4) & 0xFF80u) << 8));
        SetLong(At(kPendingZ), static_cast<std::int32_t>((Word(link + 6) & 0xFF80u) << 8));
        flags = link[0xA];
    } else {
        const unsigned index = Word(link + 4);
        const unsigned count = Ptr(descriptor + 0x28)[index];
        const unsigned char* choice = Ptr(Ptr(descriptor + 0x24) + index * 4);
        for (unsigned i = 0; i < count; ++i, choice += 10)
            if (g.flags_test(Cond_Flags + choice[6] * 8u, choice[7]) != 0) break;
        SetWord(At(kPendingArea), Word(choice));
        SetLong(At(kPendingX), static_cast<std::int32_t>((Word(choice + 2) & 0xFF80u) << 8));
        SetLong(At(kPendingZ), static_cast<std::int32_t>((Word(choice + 4) & 0xFF80u) << 8));
        flags = choice[8];
    }
    At(kPendingFlags)[0] = static_cast<unsigned char>(flags & 0x8F);
    At(kLinkHigh)[0] = static_cast<unsigned char>((flags >> 4) & 7);
    g.classify();
    g.pick_music(static_cast<unsigned>(Long(At(kPendingX)) >> 16), static_cast<unsigned>(Long(At(kPendingZ)) >> 16),
                 Word(At(kPendingArea)));
    return 0;
}

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
    const auto table = static_cast<std::uint32_t>(Long(Descriptor() + 0x14));
    const unsigned char* list = Ptr(At(table) + entry * 4);
    const unsigned char header = list[0];
    const unsigned n = header & 0xFu;
    ++list;
    if ((header & 0xC0) == 0) {
        if (Field_MemberCount != n) return (table & 0xFFFFFF00u) | 1u;
        for (unsigned i = 0; i < n; ++i) At(kPickFree)[i] = At(kPartyList)[i];
        for (unsigned i = 0; i < n; ++i) {
            const unsigned char wanted = list[i];
            const unsigned members = Field_MemberCount;
            unsigned j = 0;
            if (wanted != 0x80) {
                while (j < members && Record(j)[kRecMember] != wanted) ++j;
                if (j == members) return 1;
            } else {
                while (j < members && At(kPickFree)[j] == 0xFF) ++j;
                if (j == members) continue;
            }
            At(kPickColumn)[i] = Record(j)[kRecColumn];
            At(kPickSlot)[i] = static_cast<unsigned char>(j);
            At(kPickFree)[j] = 0xFF;
        }
        for (unsigned i = 0; i < n; ++i) At(kPartyList)[i] = Record(At(kPickSlot)[i])[kRecMember];
        for (unsigned s = 0; static_cast<int>(s) < static_cast<int>(n) - 1; ++s) {
            const unsigned char there = At(kPickSlot)[s];
            if (s == there) continue;
            g.swap_members(s, there, 0);
            for (unsigned t = s; t < n; ++t) {
                if (At(kPickSlot)[t] == s) {
                    At(kPickSlot)[t] = At(kPickSlot)[s];
                    break;
                }
            }
        }
        list += n;
    } else if ((header & 0xC0) == 0x80 && Field_MemberCount < n) {
        return (table & 0xFFFFFF00u) | 1u;
    }
    g.set_up(Field_MemberCount, list);
    list += n;
    // Byte-wide, as the original: bit 3 and bit 8 of Field_ScriptFlags, bit
    // 15 of Field_ScriptFlags2.
    if (header & 0x20) At(Address(&Field_ScriptFlags))[0] |= 8;
    if ((header & 0x30) == 0x30) g.kind2_place(list[0]);
    At(Address(&Field_ScriptFlags) + 1)[0] |= 1;
    At(Address(&Field_ScriptFlags2) + 1)[0] |= 0x80;
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
    unsigned char* record = ObjTrio;
    for (unsigned i = 0; i < count; ++i, record += kRecord) {
        Sprite_Current = record;
        record[0] &= 0xDF;
        Sprite_Current[0x5D] = 0;
        Sprite_Current[0x5E] = 0;
        Sprite_Current[0x5F] = 0;
        Sprite_Current[0x5C] = 2;
        g.member_clear(i);
        Sprite_Current[1] = 2;
        Sprite_Current[7] = 0;
        Sprite_Current[9] = 0;
        SetLong(record + kRecScript, Long(Ptr(Descriptor() + 0x18) + list[i] * 4u));
        g.context_reset(record + kRecContext);
        g.load_palette(reinterpret_cast<unsigned short*>(At(kPalettes + Sprite_Current[kRecPalette] * 64u)), 0);
    }
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF8);
}

// original 0x5322D0 (PSX 0x801BF730, call-anchored; read, the same): party
// records a and b trade places - ObjTrio_SwapFields(a, b, keep), then for a
// and then b: the record made Field_State and Sprite_Current,
// Field_MemberSprite(its member, its slot), its character block +0x80
// (0xA4 bytes) copied from the character record MoveScript_EffectState
// gives for its member, and +0x148 set to that table's byte again.
//
// As the original has it: the second lookup reads +0x89 AFTER the copy,
// which has just overwritten it with the character record's +9 - so +0x148
// is the table's byte for that, not for the member (the PSX's +0x13C the
// same). Field_State is re-read after Field_MemberSprite and after the copy;
// the member is passed with a stale upper half (Field_MemberSprite masks).
extern "C" void __cdecl Party_SwapMembers(unsigned a, unsigned b, unsigned keep) {
    g.swap_fields(a, b, keep);
    const unsigned slots[2] = {a, b};
    for (const unsigned slot : slots) {
        unsigned char* const record = Record(slot & 0xFFu);
        Field_State = record;
        Sprite_Current = record;
        g.member_sprite(record[kRecMember], slot);
        unsigned char* state = Field_State;
        const unsigned char character = MoveScript_EffectState[state[kRecMember]];
        const unsigned char* const from = At(kCharacterRecords + character * kCharacterBytes);
        for (unsigned k = 0; k < kCharacterBytes; k += 4) SetLong(state + kRecCharacter + k, Long(from + k));
        state = Field_State;
        state[kRecCharacterIndex] = MoveScript_EffectState[state[kRecMember]];
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
        const std::int32_t va = Long(ra + at), vb = Long(rb + at);
        SetLong(ra + at, vb);
        SetLong(rb + at, va);
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
    BOF3_INJECT(Area_LinkAt);
    BOF3_INJECT(Party_DropIn);
    BOF3_INJECT(Party_SetUpMembers);
    BOF3_INJECT(Party_SwapMembers);
    BOF3_INJECT(ObjTrio_SwapFields);
}
