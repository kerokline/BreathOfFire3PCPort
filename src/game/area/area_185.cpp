// Moved here by tools/move_funcs.py from area_w4d.cpp (docs/source-layout-round.md).
#include "game/area/area_185.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4d.h"
#include "game/area_w4d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4d::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// The high word of a 16.16 position, as the step hook reads it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

// The focus object's trigger id cleared (0xFF): the focus read before the
// message store, as the originals do.
void DropFocusTrigger(unsigned message) {
    unsigned char* const focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    SetMessage(message);
    focus[0x86] = 0xFF;
}

// The CRT's _ftol after `fild qword` of a dword and `fadd dword` of a float:
// the sum in double (x87 at 53 bits; every value here is small and exact),
// truncated toward zero to 64 bits, of which the caller stores the low word.
std::uint16_t FtolLow(U whole, U float_cell) {
    float f;
    std::memcpy(&f, Mem(float_cell), sizeof f);
    const double sum = static_cast<double>(whole) + static_cast<double>(f);
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(sum)));
}

// Areas 175..185's choice 27 (eleven copies): the message by the answer read
// signed - 0xFA, 0xFB, 0xFC, 0xFD for 0..3, 0xF9 for 4; anything else (a
// negative answer is above 4 unsigned) leaves the message word alone.
void ChoiceMessageByAnswer() {
    static const std::uint16_t kMessages[5] = {0xFA, 0xFB, 0xFC, 0xFD, 0xF9};
    const auto answer = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    if (answer > 4) return;
    SetMessage(kMessages[answer]);
}

// Field_ChangeArea to the return point (its area word, x, z) with flags 4.
void ChangeToReturnPoint() {
    AH_CALL(Field_ChangeArea)(Word(Mem(::bof3::Img(AreaW4d_ReturnArea_addr))), Long(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr))), Long(Mem(::bof3::Img(AreaW4d_ReturnZ_addr))), 4);
}

}  // namespace

extern "C" void __cdecl Area185_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }

// original 0x429D70 (choice 28 of all eleven): answer 0 or 1: message 0xFFFF
// and the facility byte +0 the answer; else 0x2A1 and the focus object's
// trigger id cleared.
extern "C" void __cdecl Area185_ChoiceSetFacility0(void) {
    const unsigned char answer = Answer();
    if (answer == 0 || answer == 1) {
        SetMessage(0xFFFF);
        B(::bof3::Img(Rest4b_BoardFlag_addr)) = answer;
        return;
    }
    DropFocusTrigger(0x2A1);
}

// original 0x429DC0 (Area_StepHook's case for areas 175..185): x's high word
// 0x41..0x44 and z's 0x14..0x17 (16-bit): if the arm byte is set,
// Sound_PlayEffect(0x20C), the byte cleared, Field_ChangeArea to the return
// point with flags 4; off the square the byte set. al 0 always.
extern "C" unsigned char __cdecl Area185_StepHook(long x, long z) {
    if (static_cast<std::uint16_t>(High(x) - 0x41) > 3 || static_cast<std::uint16_t>(High(z) - 0x14) > 3) {
        B(::bof3::Img(AreaW4d_HookArmed_addr)) = 1;
        return 0;
    }
    if (B(::bof3::Img(AreaW4d_HookArmed_addr)) == 0) return 0;
    B(::bof3::Img(AreaW4d_HookArmed_addr)) = 0;
    AH_CALL(Sound_PlayEffect)(0x20C);
    ChangeToReturnPoint();
    return 0;
}

// original 0x429E20 (EffectKind18_States[103], 0x654208; Sprite_Current an
// effect record; a gap of the tool): MapView_ScreenXY = (-12544, -14848); a
// draw mode (tpage 0xB5, dtd 1) linked at the record (size 0xC). Then 32 rows
// of 8 semi-transparent TILE_1s, the row counting r = 0, -8, .. -0xF8 and
// the column u = 0, 0x10, .. 0x1F0 with it; the shade s = (r -
// (Frame_Counter & 7) + 0x17F) >> 1 (blue s, red and green s / 4);
// for k = 0 .. 7 the angle a = k * 0x200 + 0x100 and m = (Frame_Counter &
// 0xF) + u (the counter read once, after the first sine): the vertex
// ((sin a * m * 5) >> 16 + screen x, (cos a * m * 5) >> 16 + screen y, -0x340
// - sin(m * 5) >> 5), each of the first two through x87 and _ftol; the vertex
// through Gte_RotTransPers into the tile, the depth by Gte_StoreDepthF at
// +0x10, linked at the record (size 0x14). A draw mode (tpage 0xB5) linked
// last.
extern "C" void __cdecl Area185_DrawTileField(void) {
    unsigned char* const first = Gfx_PacketNext;
    SetLong(Mem(::bof3::Img(MapView_ScreenXY_addr)), static_cast<std::int32_t>(0xC6440000u));
    SetLong(Mem(::bof3::Img(MapView_ScreenXY_addr) + 4), static_cast<std::int32_t>(0xC6680000u));
    AH_CALL(Gpu_SetDrawMode)(first, 0, 1, 0xB5, 0);
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(rv::Current<records::EffectObject>()->x), static_cast<U>(rv::Current<records::EffectObject>()->z), 1, 0xC);
    U u = 0;
    std::int32_t row = 0;
    long depth = 0;
    do {
        for (std::int32_t k = 0; k < 0x8000; k += 0x1000) {
            unsigned char* const tile = Gfx_PacketNext;
            AH_CALL(Gpu_SetTile1)(tile);
            AH_CALL(Gpu_SetSemiTrans)(tile, 1);
            const U shade = (static_cast<U>(row) - (static_cast<U>(Frame_Counter) & 7) + 0x17F) >> 1;
            tile[6] = static_cast<unsigned char>(shade);
            const auto quarter = static_cast<unsigned char>(static_cast<std::int32_t>(shade) / 4);
            tile[4] = quarter;
            tile[5] = quarter;
            const std::int32_t angle = k / 8 + 0x100;
            const int sine = AH_CALL(Math_Sin)(angle);
            const U m = (static_cast<U>(Frame_Counter) & 0xF) + u;
            SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr)), FtolLow((static_cast<U>(sine) * m * 5u) >> 16, ::bof3::Img(MapView_ScreenXY_addr)));
            const int cosine = AH_CALL(Math_Cos)(angle);
            SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + 2), FtolLow((static_cast<U>(cosine) * m * 5u) >> 16, ::bof3::Img(MapView_ScreenXY_addr) + 4));
            const int lift = AH_CALL(Math_Sin)(static_cast<int>(m * 5u));
            SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + 4), static_cast<U>(-0x340 - (lift >> 5)));
            AH_CALL(Gte_RotTransPers)(reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr))), reinterpret_cast<unsigned long*>(tile + 8), &depth);
            AH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(tile + 0x10));
            AH_CALL(MapView_LinkPrimAt)(static_cast<U>(rv::Current<records::EffectObject>()->x), static_cast<U>(rv::Current<records::EffectObject>()->z), 1, 0x14);
        }
        row -= 8;
        u += 0x10;
    } while (row > -0x100);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(rv::Current<records::EffectObject>()->x), static_cast<U>(rv::Current<records::EffectObject>()->z), 1, 0xC);
}

void Area185_Inject() {
    BOF3_INJECT(Area185_ChoiceMessageByAnswer);
    BOF3_INJECT(Area185_ChoiceSetFacility0);
    BOF3_INJECT(Area185_StepHook);
    BOF3_INJECT(Area185_DrawTileField);
}
