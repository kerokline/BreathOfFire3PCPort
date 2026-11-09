// Moved here by tools/move_funcs.py from area_w2a.cpp (docs/source-layout-round.md).
#include "game/area/area_82.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2a.h"
#include "game/area_w2a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Chapter_FlagRow_addr))))));
}

}  // namespace

// original 0x40F610 (Area82_Handlers[0]; PSX 0x801F5590): by Field_State's
// byte +0x89: 8 nothing; 4 the script's position word +0xA on 0xA; any other
// on 0x16.
extern "C" void __cdecl Area82_SkipByPose(void) {
    const unsigned char pose = Field_State[0x89];
    if (pose == 8) return;
    AddWord(MoveScript_Object + 0xA, pose == 4 ? 0xA : 0x16);
}

// original 0x40F640 (Area82_Handlers[1]; PSX 0x801F55F0): the running
// object's dword +0x70 (the margin Party_MemberAt reads, docs/area_w1c.md) 5.
extern "C" void __cdecl Area82_SetMargin5(void) { rv::Current<records::SpriteObject>()->reach = 5; }

// original 0x40F650 (Area82_Handlers[2]; PSX 0x801F5604): Field_State's byte
// +0x89 tested three times, read again each time: 5 the script's position on
// 3, 4 on 6, 6 on 9.
extern "C" void __cdecl Area82_SkipByPose456(void) {
    if (Field_State[0x89] == 5) AddWord(MoveScript_Object + 0xA, 3);
    if (Field_State[0x89] == 4) AddWord(MoveScript_Object + 0xA, 6);
    if (Field_State[0x89] == 6) AddWord(MoveScript_Object + 0xA, 9);
}

// original 0x40F6A0 (Area82_Handlers[3]; PSX 0x801F56E8): Msg_OpenScript(8)
// with flag 4 of the chapter's row set, else (3); Field_Request 2.
extern "C" void __cdecl Area82_OpenMessageByRowFlag4(void) {
    AH_CALL(Msg_OpenScript)(AH_CALL(Flags_Test)(FlagRow(), 4) ? 8 : 3);
    Field_Request = 2;
}

void Area82_Inject() {
    BOF3_INJECT(Area82_SkipByPose);
    BOF3_INJECT(Area82_SetMargin5);
    BOF3_INJECT(Area82_SkipByPose456);
    BOF3_INJECT(Area82_OpenMessageByRowFlag4);
}
