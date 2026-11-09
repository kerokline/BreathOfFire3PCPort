// Moved here by tools/move_funcs.py from area_w3f.cpp (docs/source-layout-round.md).
#include "game/area/area_143.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3f.h"
#include "game/area_w3f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3f::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(U v) { return static_cast<std::uint16_t>(v >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned index) {
    if (index >= at::kStateCount)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, at::kStateCount, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The leader's pose is one the hooks accept: 0, 7 or 6.
bool PoseStanding() {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    return pose == 0 || pose == 7 || pose == 6;
}

// The step hooks' drop-in: counter 0 cleared, then Party_DropIn(entry), al 1.
unsigned char DropIn(unsigned entry) {
    B(::bof3::Img(at::kCounter0.v)) = 0;
    AH_CALL(Party_DropIn)(entry);
    return 1;
}

// Area 143's handler 2 and area 145's handlers 4..6 (0x420960, 0x421370,
// 0x421400, 0x421490, one body with its own two tables each): for each of
// the four keys in turn, each of Field_MemberCount's party records (the
// count read once, compared unsigned): the first record whose +0x89 is the
// key opens the key's message (Msg_OpenScript) and sets Field_Request 2.
// None: nothing. The records are not bounded by three (the count is).
void MessageByMember(U keys, U messages) {
    const unsigned char count = Field_MemberCount;
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned char j = 0; j < count; ++j) {
            if (B(::bof3::Img(ObjTrio_addr) + 0x89 + j * at::kPartyStride) != B(keys + i)) continue;
            AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(Mem(messages + i * 2))));
            Field_Request = 2;
            return;
        }
    }
}

using ProjectFn = void (__cdecl*)(const std::int32_t*, std::uint32_t*);

void Project(const std::int32_t* v, std::uint32_t* out) { AH_AT(ProjectFn, at::kProjectPoint.v)(v, out); }

// The radius of the ring: Math_Cos / Math_Sin's answer << 9, then >> 4
// arithmetic (32 world units a unit of the trig table; the shift wraps as
// the original's).
std::int32_t Radius(int trig) { return static_cast<std::int32_t>(static_cast<U>(trig) << 9) >> 4; }

// A draw-mode packet (abr 1 on tpage 0x3C0, dtd as given) at Gfx_PacketNext,
// linked at (x, z) with size 0xC.
void DrawModeAt(int dtd, U x, U z) {
    const unsigned tpage = AH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0) & 0xFFFFu;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
    AH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
}

// The two copies, instruction for instruction the same but for their stack
// frames (docs/area_w3f.md section 1): the map camera; a ring of sixteen
// semi-transparent Gouraud quads about `p` (x, z, y dwords) of radius 32 x
// the trig table, each between y and y + 0x8000000 (two points projected by
// 0x494110 at each 0x100 of angle, the previous pair and the new pair the
// quad's four vertices), its colour grey (0x20 or 0x24 by Frame_Counter's
// bit 0) stepping by 4 each quad: down over quads 4..11, up over the rest.
// Each quad is linked (0x44) between a draw-mode packet with dtd 1 and one
// with dtd 0, all three at the new pair's (x, z).
void GlowCylinder(const std::int32_t* p) {
    AH_AT(void (__cdecl*)(), at::kSetMapCamera.v)();
    std::int32_t v[3];
    std::uint32_t a[3], b[3];
    // the point's words read after each trig call, as the originals do
    auto around = [&](U angle) {
        const int c = AH_CALL(Math_Cos)(static_cast<int>(angle));
        v[0] = static_cast<std::int32_t>(static_cast<U>(p[0]) + static_cast<U>(Radius(c)));
        const int s = AH_CALL(Math_Sin)(static_cast<int>(angle));
        v[1] = static_cast<std::int32_t>(static_cast<U>(p[1]) + static_cast<U>(Radius(s)));
        v[2] = p[2];
    };
    around(0);
    Project(v, a);
    v[2] = static_cast<std::int32_t>(static_cast<U>(p[2]) + 0x8000000u);
    Project(v, b);
    auto shade = static_cast<unsigned char>(((Frame_Counter & 1) + 8) << 2);
    U angle = 0;
    for (unsigned i = 0; i < 16; ++i) {
        const std::uint32_t pa[3] = {a[0], a[1], a[2]};
        const std::uint32_t pb[3] = {b[0], b[1], b[2]};
        angle += 0x100;
        around(angle & 0xFFFF);
        Project(v, a);
        v[2] = static_cast<std::int32_t>(static_cast<U>(p[2]) + 0x8000000u);
        Project(v, b);
        const U x = static_cast<U>(v[0]), z = static_cast<U>(v[1]);
        DrawModeAt(1, x, z);
        unsigned char* const prim = Gfx_PacketNext;
        AH_CALL(Gpu_SetPolyG4)(prim);
        AH_CALL(Gpu_SetSemiTrans)(prim, 1);
        for (unsigned k = 0; k < 3; ++k) {
            SetLong(prim + 8 + k * 4, static_cast<std::int32_t>(pa[k]));
            SetLong(prim + 0x18 + k * 4, static_cast<std::int32_t>(pb[k]));
            SetLong(prim + 0x28 + k * 4, static_cast<std::int32_t>(a[k]));
            SetLong(prim + 0x38 + k * 4, static_cast<std::int32_t>(b[k]));
        }
        prim[4] = prim[5] = prim[6] = shade;
        prim[0x14] = prim[0x15] = prim[0x16] = shade;
        shade = static_cast<unsigned char>(i >= 4 && i < 12 ? shade - 4 : shade + 4);
        prim[0x24] = prim[0x25] = prim[0x26] = shade;
        prim[0x34] = prim[0x35] = prim[0x36] = shade;
        AH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
        DrawModeAt(0, x, z);
    }
}

// The effect state that hands the running record's point (+0x34, +0x38,
// +0x3C) to a cylinder (0x420B20, 0x4220A0).
void CylinderAtCurrent(void (__cdecl* cylinder)(const long*)) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::EffectObject>(cur);
    const long point[3] = {tCur->x, tCur->z, tCur->height};
    cylinder(point);
}

}  // namespace

// original 0x420800 (area 143 +0x34[0]): answer not 0: message 0x54 and the
// mark 0x9398CF 6. 0: the dword 0x904650 equal to 0x3FFFF, message 0x52;
// else message 0x53 and the mark 6.
extern "C" void __cdecl Area143_ChoiceAsk52(void) {
    if (Answer() != 0) {
        SetMessage(0x54);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    if (static_cast<U>(Long(Mem(::bof3::Img(Gene_Bits_addr)))) == 0x3FFFFu) {
        SetMessage(0x52);
        return;
    }
    SetMessage(0x53);
    B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
}

// original 0x420850 (area 143 +0x34[1]; also areas 3, 37, 41, 50, 55, 59,
// 61, 68, 74, 91, 98, 113 and 116's): answer not 0: the byte 0x9398D1 = 3;
// message 0xFFFF either way.
extern "C" void __cdecl Area143_ChoiceYesMark3(void) {
    if (Answer() != 0) B(::bof3::Img(AreaW3f_AnswerMarkD1_addr)) = 3;
    SetMessage(0xFFFF);
}

// original 0x420870 (area 143 +0x34[2]; the same fourteen areas): answer not
// 0: 0x9398D1 = 5; message 0xFFFF either way.
extern "C" void __cdecl Area143_ChoiceYesMark5(void) {
    if (Answer() != 0) B(::bof3::Img(AreaW3f_AnswerMarkD1_addr)) = 5;
    SetMessage(0xFFFF);
}

// original 0x420890 (area 143 +0x34[3], [4]): answer not 0: message 0x54
// and the mark 6; 0: message 0xFFFF.
extern "C" void __cdecl Area143_ChoiceAsk54(void) {
    if (Answer() != 0) {
        SetMessage(0x54);
        B(::bof3::Img(AreaW0c_AnswerMark_addr)) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x4208C0 (area 143 +0x34[5]): message 0x49, or 0x4A for an answer
// not 0.
extern "C" void __cdecl Area143_ChoiceMessage49(void) { SetMessage(Answer() != 0 ? 0x4Au : 0x49u); }

// original 0x4208E0 (area 143 +0x34[6]): answer not 0: message 0x4C. 0:
// ScriptFlags_Set40; then (the message word read after the call) 0x44:
// MoveScript_Var7 8, the step 0x8034E5 8, message 0x4B and the focus
// object's word +0x8A + 1; else Var7 8, the step 0xA, message 0xFFFF.
extern "C" void __cdecl Area143_ChoiceRun8(void) {
    if (Answer() != 0) {
        SetMessage(0x4C);
        return;
    }
    AH_CALL(ScriptFlags_Set40)();
    const bool asked = Word(Mem(::bof3::Img(at::kMessage.v))) == 0x44;
    MoveScript_Var7 = 8;
    if (asked) {
        B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 8;
        unsigned char* const focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
        SetMessage(0x4B);
        AddWord(focus + 0x8A, 1);
        return;
    }
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0xA;
    SetMessage(0xFFFF);
}

// original 0x420940 (area 143 +0x3C[0] = +0x34[9]; area 145 +0x3C[2] =
// +0x34[3]; area 146 +0x3C[0]; PSX 0x801F3C10 / 0x801F3FD8 / 0x801F2C04):
// Field_ChangeArea(0x70, 0xC8000, 0x580000, 0x81).
extern "C" void __cdecl Area143_ChangeArea70(void) { AH_CALL(Field_ChangeArea)(0x70, 0xC8000, 0x580000, 0x81); }

// original 0x420960 (area 143 +0x3C[2] = +0x34[11]; PSX 0x801F3C74): the
// first of Area143_MemberKeys some member's +0x89 holds opens its message
// from Area143_MemberMessages, Field_Request 2 (MessageByMember).
extern "C" void __cdecl Area143_MessageByMember(void) { MessageByMember(::bof3::Img(Area143_MemberKeys_addr), ::bof3::Img(Area143_MemberMessages_addr)); }

// original 0x4209F0 (area 143 +0x3C[3] = +0x34[12]; also area 197 +0x3C[9];
// PSX 0x801F3D38): the leader's +0x89 7, the script position + 3.
extern "C" void __cdecl Area143_SkipIfLeader89Is7(void) {
    if (Field_State[0x89] == 7) ScriptStep(3);
}

// original 0x420A10 (Area_StepHook's case for area 0x8F): Cond_ByteFD 1,
// x's high word 0x37..0x39 and z's 0x64..0x66 (16-bit compares), the
// leader's pose 0, 7 or 6: counter 0 = 0, Party_DropIn(0), al 1. Else al 0.
extern "C" unsigned char __cdecl Area143_StepHook(long x, long z) {
    if (Cond_ByteFD != 1) return 0;
    if (static_cast<std::uint16_t>(High(static_cast<U>(x)) - 0x37) >= 3) return 0;
    if (static_cast<std::uint16_t>(High(static_cast<U>(z)) - 0x64) >= 3) return 0;
    if (!PoseStanding()) return 0;
    return DropIn(0);
}

// original 0x420A60 (Field_ObjectTriggers[50]): ScriptFlags_Set40; tail kind
// 4 (engine) with sub-kind 0xF; Cond_ByteFE (read after the call) 0: set to
// 1 and the tail's state 5 (else the state is left). al 0.
extern "C" unsigned char __cdecl Area143_Trigger50(void) {
    AH_CALL(ScriptFlags_Set40)();
    const unsigned char fe = Cond_ByteFE;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 4;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = 0xF;
    if (fe == 0) {
        Cond_ByteFE = 1;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 5;
    }
    return 0;
}

// original 0x420A90 (called by chapter 12's code at 0x562027 and 0x5620BA,
// with 1): each of the 256 x 11 CLUT words of rows 3..13 as loaded
// (Gfx_ClutStripSource + 0x600) with each 5-bit channel shifted right by
// `shift` (its low byte, as x86 masks it: & 0x1F), bit 15 kept, to the live
// strip 0x4000 on (Gfx_ClutStrip + 0x600); then Gfx_ClutStripDirty 1. al 0.
extern "C" unsigned char __cdecl Area143_ClutShiftRight(unsigned shift) {
    const unsigned s = shift & 0x1F;
    U at = ::bof3::Img(Gfx_ClutStripSource_addr) + 0x600;
    do {
        for (unsigned i = 0; i < 0x100; ++i, at += 2) {
            const U w = Word(Mem(at));
            const U r = ((w >> 10) & 0x1F) >> s;
            const U g = ((w >> 5) & 0x1F) >> s;
            const U b = (w & 0x1F) >> s;
            SetWord(Mem(at + at::kClutToLive), ((r << 5 | g) << 5 | b) | (w & 0x8000));
        }
    } while (static_cast<std::int32_t>(at) < static_cast<std::int32_t>(::bof3::Img(at::kClutSourceEnd.v)));
    Gfx_ClutStripDirty = 1;
    return 0;
}

// original 0x420B00 (Effect_KindHandlers[0xB3]): Area143_EffectStates by the
// running record's +1 (unchecked there; ours aborts past its two).
extern "C" void __cdecl Area143_EffectB3Run(void) {
    StateEntry("Area143_EffectB3Run", ::bof3::Img(Area143_EffectStates_addr), Sprite_Current[1])();
}

// original 0x420B20 (Area143_EffectStates[1]; state 0 is Area59_EffectGround
// 0x40B4F0): the running record's (x, z, y) to the stack, handed to
// Area143_DrawGlowCylinder.
extern "C" void __cdecl Area143_EffectB3Cylinder(void) { CylinderAtCurrent(AH_CALL(Area143_DrawGlowCylinder)); }

// original 0x420B50 (called by Area143_EffectB3Cylinder only): GlowCylinder,
// area 143's copy.
extern "C" void __cdecl Area143_DrawGlowCylinder(const long* point) { GlowCylinder(reinterpret_cast<const std::int32_t*>(point)); }

void Area143_Inject() {
    BOF3_INJECT(Area143_ChoiceAsk52);
    BOF3_INJECT(Area143_ChoiceYesMark3);
    BOF3_INJECT(Area143_ChoiceYesMark5);
    BOF3_INJECT(Area143_ChoiceAsk54);
    BOF3_INJECT(Area143_ChoiceMessage49);
    BOF3_INJECT(Area143_ChoiceRun8);
    BOF3_INJECT(Area143_ChangeArea70);
    BOF3_INJECT(Area143_MessageByMember);
    BOF3_INJECT(Area143_SkipIfLeader89Is7);
    BOF3_INJECT(Area143_StepHook);
    BOF3_INJECT(Area143_Trigger50);
    BOF3_INJECT(Area143_ClutShiftRight);
    BOF3_INJECT(Area143_EffectB3Run);
    BOF3_INJECT(Area143_EffectB3Cylinder);
    BOF3_INJECT(Area143_DrawGlowCylinder);
}
