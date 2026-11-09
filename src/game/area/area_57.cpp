// Moved here by tools/move_funcs.py from area_w1d.cpp (docs/source-layout-round.md).
#include "game/area/area_57.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

// The message box's answer, and the message word a choice leaves.
unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The byte a table indexed by the s8 answer holds (`movsx eax, answer`).
std::uint32_t BySignedAnswer(std::uint32_t table, unsigned stride, unsigned offset) {
    const auto answer = static_cast<signed char>(Answer());
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(table) + answer * static_cast<int>(stride) + static_cast<int>(offset));
}

// Areas 56 and 57's object triggers 52 / 53 (0x40B1D0, 0x40B2F0): story flag
// `flag` set; when flags 0x6D..0x70 are all set, flag 0x71 too (area 34's
// trigger 51 is the same with 0x6D). Answers 0 in al.
unsigned char FlagsGather(unsigned flag) {
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), flag);
    for (unsigned f = 0x6D; f <= 0x70; ++f)
        if (!AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), f)) return 0;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x71);
    return 0;
}

// Area 57's handlers 0 and 1 (0x40B230, 0x40B280): Effect_Spawn(kind, 0,
// kinds[the first party list byte], the leader's words +0x2E, +0x30) with
// Sprite_Current made the leader's record (and left so); an answer other than
// 0xFF to Sprite_Current[0xB], read again after the call. The list byte
// indexes the table unchecked, as the original's.
void SpawnAtLeader(unsigned char kind, std::uint32_t kinds) {
    const auto z = static_cast<short>(Word(Mem(::bof3::Img(ObjTrio_addr) + 0x30)));
    const auto x = static_cast<short>(Word(Mem(::bof3::Img(ObjTrio_addr) + 0x2E)));
    const unsigned char list = B(::bof3::Img(Cond_Flags_addr) + 0xd2);
    Sprite_Current = rv::Bytes(ObjTrio);
    const unsigned char effect = B(kinds + list);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

}  // namespace

// original 0x40B210 (Area57_Choices[0] = Area57_Handlers[4]; PSX 0x801F2C04):
// the message word Area57_ChoiceMessages[the s8 answer] (unchecked).
extern "C" void __cdecl Area57_ChoiceMessage(void) { SetMessage(Word(Mem(BySignedAnswer(::bof3::Img(Area57_ChoiceMessages_addr), 2, 0)))); }

// original 0x40B230 (Area57_Handlers[0]; PSX 0x801F2C30): SpawnAtLeader, kind 2.
extern "C" void __cdecl Area57_SpawnKind2AtLeader(void) { SpawnAtLeader(2, ::bof3::Img(Area57_EffectKinds2_addr)); }

// original 0x40B280 (Area57_Handlers[1]; PSX 0x801F2CB0): SpawnAtLeader, kind 4.
extern "C" void __cdecl Area57_SpawnKind4AtLeader(void) { SpawnAtLeader(4, ::bof3::Img(Area57_EffectKinds4_addr)); }

// original 0x40B2D0 (Area57_Handlers[2]; also area 39's handler 9, area 108's
// choice 16 / handler 13, area 145's choice 10 / handler 9; PSX 0x801F2D30):
// a 5-byte `jmp Sound_StopMusic`.
extern "C" void __cdecl Area57_StopMusic(void) { AH_CALL(Sound_StopMusic)(); }

// original 0x40B2E0 (Area57_Handlers[3]; also area 108's choice 17 / handler
// 14, area 145's choice 11 / handler 10; PSX 0x801F2D58): a 5-byte `jmp
// Sound_ResumeAll`.
extern "C" void __cdecl Area57_ResumeSound(void) { AH_CALL(Sound_ResumeAll)(); }

// original 0x40B2F0 (Field_ObjectTriggers id 53, (object, flags) ignored; in
// area 57's block by address): FlagsGather(0x6F).
extern "C" unsigned char __cdecl Area57_Trigger53(unsigned char*, unsigned char*) { return FlagsGather(0x6F); }

void Area57_Inject() {
    BOF3_INJECT(Area57_ChoiceMessage);
    BOF3_INJECT(Area57_SpawnKind2AtLeader);
    BOF3_INJECT(Area57_SpawnKind4AtLeader);
    BOF3_INJECT(Area57_StopMusic);
    BOF3_INJECT(Area57_ResumeSound);
    BOF3_INJECT(Area57_Trigger53);
}
