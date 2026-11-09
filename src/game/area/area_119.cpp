// Moved here by tools/move_funcs.py from area_w3a.cpp (docs/source-layout-round.md).
#include "game/area/area_119.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3a.h"
#include "game/area_w3a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::SetWord;

unsigned char* PartyRecord(U index) { return At(::bof3::Img(ObjTrio_addr) + index * at::kPartyStride); }

void SetMessage(unsigned id) { SetWord(At(::bof3::Img(at::kMessage.v)), id); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }

// Party record `member`'s words +0x30 (z) and +0x2E (x), the member's id from
// the first party list; Sprite_Current made that record; Effect_Spawn(kind, 0,
// the table's byte by the id (unchecked, as the original's), x, z); an answer
// other than 0xFF stored to Sprite_Current[0xB], Sprite_Current read again
// after the call (area 11's Area11_SpawnEffect is the same shape).
void SpawnAtMember(U member, U table, unsigned char kind) {
    const unsigned char* const rec = PartyRecord(member);
    const auto* const tRec = rv::AsConst<records::PartyMember>(rec);
    const auto z = static_cast<short>(static_cast<std::uint16_t>(tRec->screen_y));
    const auto x = static_cast<short>(static_cast<std::uint16_t>(tRec->screen_x));
    const unsigned id = At(::bof3::Img(Cond_Flags_addr) + 0xd2 + member)[0];
    Sprite_Current = PartyRecord(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(At(table + id)[0]), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

}  // namespace

// original 0x41A730 .. 0x41A910 (area 119's handlers 0, 4..9 = choices 1, 5..10,
// Area119_Handlers 0x622AE8; PSX 0x801F4138, 0x801F4224, 0x801F42A4,
// 0x801F4324, 0x801F43A4, 0x801F4424, 0x801F44A4).
extern "C" void __cdecl Area119_Spawn1AtLeader(void) { SpawnAtMember(0, ::bof3::Img(Area119_SpawnKindsA_addr), 1); }

extern "C" void __cdecl Area119_Spawn4AtLeader(void) { SpawnAtMember(0, ::bof3::Img(Area119_SpawnKindsA_addr), 4); }

extern "C" void __cdecl Area119_Spawn1AtLeaderB(void) { SpawnAtMember(0, ::bof3::Img(Area119_SpawnKindsB_addr), 1); }

extern "C" void __cdecl Area119_Spawn1AtMember1(void) { SpawnAtMember(1, ::bof3::Img(Area119_SpawnKindsB_addr), 1); }

extern "C" void __cdecl Area119_Spawn1AtMember2(void) { SpawnAtMember(2, ::bof3::Img(Area119_SpawnKindsB_addr), 1); }

extern "C" void __cdecl Area119_Spawn3AtMember2(void) { SpawnAtMember(2, ::bof3::Img(Area119_SpawnKindsB_addr), 3); }

extern "C" void __cdecl Area119_Spawn4AtMember2(void) { SpawnAtMember(2, ::bof3::Img(Area119_SpawnKindsA_addr), 4); }

// original 0x41A960 (handler 10 / choice 11; PSX 0x801F4524): flag 0x3C of the
// chapter's row set, Sound_PlayEffect(0x203).
extern "C" void __cdecl Area119_SetRowFlag3C(void) {
    AH_CALL(Flags_Set)(FlagRow(), 0x3C);
    AH_CALL(Sound_PlayEffect)(0x203);
}

// original 0x41A980 (handler 11 / choice 12; PSX 0x801F4554): the same flag cleared.
extern "C" void __cdecl Area119_ClearRowFlag3C(void) {
    AH_CALL(Flags_Clear)(FlagRow(), 0x3C);
    AH_CALL(Sound_PlayEffect)(0x203);
}

// original 0x41A9A0 (choice 0; PSX 0x801F4584 by the table's anchor): no new
// message; the cursor (s8) 0: counter 0 = 0xA, 1: 0x14, else nothing.
extern "C" void __cdecl Area119_ChoiceCounter0(void) {
    const auto cursor = AreaW0c_ChoiceAnswer;
    SetMessage(0xFFFF);
    if (cursor == 0) At(::bof3::Img(at::kCounter0.v))[0] = 0xA;
    else if (cursor == 1) At(::bof3::Img(at::kCounter0.v))[0] = 0x14;
}

void Area119_Inject() {
    BOF3_INJECT(Area119_Spawn1AtLeader);
    BOF3_INJECT(Area119_Spawn4AtLeader);
    BOF3_INJECT(Area119_Spawn1AtLeaderB);
    BOF3_INJECT(Area119_Spawn1AtMember1);
    BOF3_INJECT(Area119_Spawn1AtMember2);
    BOF3_INJECT(Area119_Spawn3AtMember2);
    BOF3_INJECT(Area119_Spawn4AtMember2);
    BOF3_INJECT(Area119_SetRowFlag3C);
    BOF3_INJECT(Area119_ClearRowFlag3C);
    BOF3_INJECT(Area119_ChoiceCounter0);
}
