// Moved here by tools/move_funcs.py from area_w1c.cpp (docs/source-layout-round.md).
#include "game/area/area_51.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1c.h"
#include "game/area_w1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

}  // namespace

// original 0x409CE0 (Area51_Choices[1] = Area51_Handlers[0]; PSX 0x801F2C04):
// Party_MemberAt(the running object's +0x34, +0x38, +0x70); a member (not
// 0xFF), with Field_ScriptFlags2 bit 0x1000 clear and Field_Request 0:
// 0x57C8A0(member, 1).
extern "C" void __cdecl Area51_MemberAtObject(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    const unsigned char member =
        AH_CALL(Party_MemberAt)(tCur->x, tCur->z, static_cast<unsigned>(static_cast<std::int32_t>(tCur->reach)));
    if (member == 0xFF) return;
    if (Field_ScriptFlags2 & 0x1000) return;
    if (Field_Request != 0) return;
    AH_AT(void (__cdecl*)(unsigned, unsigned), at::kMemberSetState.v)(member, 1);
}

// original 0x409D30 (Area51_Choices[2] = Area51_Handlers[1]; PSX 0x801F2C7C):
// the running object put at the position (+0x34, +0x38) of the object ten
// records (0x668 bytes) before the active member, and its word +0x3E the
// ground's elevation there less 0x40. Sprite_Current read again for each
// store, as the original's.
extern "C" void __cdecl Area51_PlaceAtMember(void) {
    const unsigned char* const from = Field_ActiveMember - 0x668;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, Long(from + 0x34));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, Long(from + 0x38));
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    const std::int32_t ground = AH_CALL(AreaMap_Elevation)(tCur->x, tCur->z);
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<unsigned>(ground - 0x40));
}

// original 0x409D80 (Area51_Choices[0]): message 1 for an answer not 0, 0x14
// for 0.
extern "C" void __cdecl Area51_ChoiceMessage(void) { SetMessage(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0 ? 1 : 0x14); }

// original 0x409DA0 (area 51's init, the descriptor's +0x40; PSX
// 0x801F2D00): with Cond_Flags row 14's flag 0x13 set and 0x14 clear, the
// first header entry of kind 0x81 from AreaMap_EntryBase gets both colour
// dwords, +0xC then +8, 0x04214E73 (Area11_DimBackdrop's walk with its own
// colour). A zero dword ends the walk, the byte +2 is the step in dwords; as
// the original, a step of 0 on an entry of another kind never ends (read,
// not run: the fuzz keeps the steps above 0).
extern "C" void __cdecl Area51_TintBackdrop(void) {
    if (!AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x70), 0x13)) return;
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x70), 0x14)) return;
    unsigned char* entry = AreaMap_Header + static_cast<std::uint32_t>(AreaMap_EntryBase) * 4u;
    for (auto e = static_cast<std::uint32_t>(Long(entry)); e != 0; e = static_cast<std::uint32_t>(Long(entry))) {
        if ((e & 0xFF000000u) == 0x81000000u) {
            SetLong(entry + 12, 0x04214E73);
            SetLong(entry + 8, 0x04214E73);
            return;
        }
        entry += ((e >> 16) & 0xFFu) * 4u;
    }
}

void Area51_Inject() {
    BOF3_INJECT(Area51_MemberAtObject);
    BOF3_INJECT(Area51_PlaceAtMember);
    BOF3_INJECT(Area51_ChoiceMessage);
    BOF3_INJECT(Area51_TintBackdrop);
    {
        // DIVERGENCE DIV-0176: Sunder's animation at Mt. Myrneg's entry (area
        // 51, the +0x10 movement script 3 of the bank-0x44 object) is the
        // looping cut 0x0F, not the once-and-hold cut 0x10 - the script's
        // first op, `0E 10 01 01` at 0x5FD000 in BOF3.exe's .data, asks for
        // 0x0F. One byte; the original in both hosts under
        // BOF3X_ORIGINAL=SunderLoop. docs/cut-content.md 4.
        static const std::uint8_t was = 0x10, is = 0x0F;
        const bool on = bof3::PatchBytes("SunderLoop", 0x5FD001, &was, &is, 1);
        bof3::Log("DIV-0176    Sunder's animation at Mt. Myrneg's entry loops (0x0F) until the scene reaches him: %s",
                  on ? "on" : "off");
    }
}
