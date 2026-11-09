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

unsigned char* Ptr(const unsigned char* at) { return move_script::At(static_cast<std::uint32_t>(move_script::Long(at))); }
unsigned char* Descriptor() { return Area_Descriptors[Game_AreaNumber]; }

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
    if (move_script::Word(link + 2) != 0xFFFF) {
        AreaW0b_Place = move_script::Word(link + 2);
        kPendingX = static_cast<std::int32_t>((move_script::Word(link + 4) & 0xFF80u) << 8);
        AreaEntry_PendingZ = static_cast<std::int32_t>((move_script::Word(link + 6) & 0xFF80u) << 8);
        flags = link[0xA];
    } else {
        const unsigned index = move_script::Word(link + 4);
        const unsigned count = Ptr(descriptor + 0x28)[index];
        const unsigned char* choice = Ptr(Ptr(descriptor + 0x24) + index * 4);
        for (unsigned i = 0; i < count; ++i, choice += 10)
            if (area_entry::g.flags_test(Cond_Flags + choice[6] * 8u, choice[7]) != 0) break;
        AreaW0b_Place = move_script::Word(choice);
        kPendingX = static_cast<std::int32_t>((move_script::Word(choice + 2) & 0xFF80u) << 8);
        AreaEntry_PendingZ = static_cast<std::int32_t>((move_script::Word(choice + 4) & 0xFF80u) << 8);
        flags = choice[8];
    }
    AreaEntry_PendingFlags = static_cast<unsigned char>(flags & 0x8F);
    kLinkHigh = static_cast<unsigned char>((flags >> 4) & 7);
    area_entry::g.classify();
    area_entry::g.pick_music(static_cast<unsigned>(kPendingX >> 16), static_cast<unsigned>(static_cast<std::int32_t>(AreaEntry_PendingZ) >> 16),
                 AreaW0b_Place);
    return 0;
}

void AreaEntry_Inject() {
    if (bof3::WantsShadow("area_entry")) area_entry::SelfTest();
    BOF3_INJECT(Area_LinkAt);
}
