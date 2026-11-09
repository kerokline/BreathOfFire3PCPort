// The field's mode handlers (docs/field-modes.md). Field_ModeDispatch
// 0x56D690 is slot 0 of the current scenario chapter's vtable - the PSX's
// thunk 0x801A8834 - and the two tail dispatchers after it; scenario 16, the
// attract sequence's demo (PSX SCENA16.EMI), is a state machine of eleven
// handlers under its slot 0; Scenario_CallA 0x5341A0 is the chapter's call
// table's thunk (PSX 0x801C2DE8). Every call goes through field_modes::g, so
// that the start-up fuzz (field_modes_fuzz.cpp) tests each function alone.
#include "game/field_modes.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/field_modes_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_modes {

using move_script::At;
using move_script::Long;

const Callees kOriginals = {
    Scenario_CallA,
    reinterpret_cast<void (__cdecl*)(int, int, int, int)>(::bof3::Code(kChangeArea.v)),
    reinterpret_cast<void (__cdecl*)(int)>(::bof3::Code(kStartMusic.v)),
    File_LoadDone,
    Task_Sleep,
    reinterpret_cast<void (__cdecl*)()>(::bof3::Code(kTaskEnd.v)),
    ObjTrio_SetBit40,
    Flags_Test,
    Flags_Set,
    ScriptFlags_Set40,
    MapView_SetElevation,
    Kind2_Place,
    reinterpret_cast<unsigned char (__cdecl*)()>(::bof3::Code(kEffectAlloc.v)),
    reinterpret_cast<void (__cdecl*)(int)>(::bof3::Code(kPlaceParty.v)),
    Sound_PlayEffect,
    Music_FadeOutStop,
    Transition_Start,
    Music_Play,
    Text_DrawAt,
    reinterpret_cast<void (__cdecl*)(unsigned short)>(::bof3::Code(kOpenScript.v)),
    reinterpret_cast<void (__cdecl*)()>(::bof3::Code(kViewShift.v)),
    ClutStrip_FadeTo,
    ClutStrip_Restore,
    Scena16_Area1F,
    Scena16_Area04,
    Scena16_Area02,
    Field_ModeTail,
};
Callees g = kOriginals;

}  // namespace field_modes

namespace {

using namespace field_modes;
using VoidFn = void (__cdecl*)();

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
VoidFn Entry(std::uint32_t table, int index) {
    return reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(field_modes::Long(field_modes::At(table + 4u * static_cast<std::uint32_t>(index))))));
}

}  // namespace

void FieldModes_Inject() {
    if (bof3::WantsShadow("field_modes")) field_modes::SelfTest();
}
