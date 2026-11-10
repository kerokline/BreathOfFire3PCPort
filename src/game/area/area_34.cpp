// Moved here by tools/move_funcs.py from area_w0c.cpp (docs/source-layout-round.md).
#include "game/area/area_34.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// Choice handlers read the message box's answer; most set the message word
// to 0xFFFF (no new message) first.
signed char Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// Areas 28 and 34's effects at a fixed cell (0x403750, 0x404E00): +0x38 z,
// +0x34 x, +0 = 1, +5 the kind, (area 34) +1 = 6, and +0x3C the ground's
// elevation there, as a word, in the high half.
void SpawnAtCell(unsigned slot, std::int32_t x, std::int32_t z, unsigned char kind, bool state6) {
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    tE->z = z;
    tE->x = x;
    e[0] = 1;
    e[5] = kind;
    if (state6) e[1] = 6;
    const auto ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(tE->x, tE->z));
    tE->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(ground)) << 16);
}

}  // namespace

// original 0x404D50 (Area34_Choices[0]): message 0xFFFF; answer 0 or 1 arms
// the mode tail kind 5 with sub-kind 5 or 4 and sets story flag 0x1E. Then,
// the answer (re-read) not 2, Cond_ByteFA 8 and flag 3 of the chapter's row
// (0x929ED0) clear: the tail disarmed, ScriptFlags_Set40, Party_DropIn(2),
// MoveScript_Var7 = 2 with step 0.
extern "C" void __cdecl Area34_ChoiceTail5(void) {
    const signed char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0 || answer == 1) {
        B(::bof3::Img(AreaW0c_TailSub_addr)) = answer == 0 ? 5 : 4;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 5;
        AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1E);
    }
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) == 2) return;
    if (static_cast<unsigned char>(Cond_ByteFA) != 8) return;
    const auto row = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Chapter_FlagRow_addr))))));
    if (AH_CALL(Flags_Test)(row, 3) != 0) return;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    AH_CALL(ScriptFlags_Set40)();
    AH_CALL(Party_DropIn)(2);
    MoveScript_Var7 = 2;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0;
}

// original 0x404DE0 (Area34_Choices[1] = Area34_Handlers[0]; PSX
// 0x801F2CDC): unless the leader's +0x89 is 7, the running script's position
// word (MoveScript_Object +0xA) moves on 0x15.
extern "C" void __cdecl Area34_SkipScript(void) {
    if (Field_State[0x89] == 7) return;
    AddWord(MoveScript_Object + 0xA, 0x15);
}

// original 0x404E00 (Area34_Choices[2] = Area34_Handlers[1]; PSX
// 0x801F2D1C): the slot to Sprite_Current[0xB]; an effect of kind 0x51,
// state 6, at cell (0x36, 0x42) on the ground.
extern "C" void __cdecl Area34_SpawnEffect51(void) {
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    const unsigned char slot = Sprite_Current[0xB];
    if (slot == 0xFF) return;
    SpawnAtCell(slot, 0x360000, 0x420000, 0x51, true);
}

// original 0x404E60 (Field_ObjectTriggers[51], (object, flags) ignored):
// story flag 0x6D set; when flags 0x6D..0x70 are all set, flag 0x71 too.
// Answers 0 in al.
extern "C" unsigned char __cdecl Area34_Trigger51(unsigned char*, unsigned char*) {
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x6D);
    for (unsigned flag = 0x6D; flag <= 0x70; ++flag)
        if (!AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), flag)) return 0;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x71);
    return 0;
}

void Area34_Inject() {
    BOF3_INJECT(Area34_ChoiceTail5);
    BOF3_INJECT(Area34_SkipScript);
    BOF3_INJECT(Area34_SpawnEffect51);
    BOF3_INJECT(Area34_Trigger51);
}
