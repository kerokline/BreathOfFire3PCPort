// Moved here by tools/move_funcs.py from area_w3f.cpp (docs/source-layout-round.md).
#include "game/area/area_146.h"

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

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(U v) { return static_cast<std::uint16_t>(v >> 16); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned index) {
    if (index >= at::kStateCount)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, at::kStateCount, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The tail armed: ScriptFlags_Set40, then kind `kind`, state 0, sub-kind
// `sub` (the three object triggers, in that order).
unsigned char ArmTail(unsigned char kind, unsigned char sub) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = sub;
    return 0;
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

// original 0x421FD0 (area 146 +0x3C[2]; PSX 0x801F2C68): the leader's
// +0x138 bit 0 cleared; the running object's +0x29 = Draw_OtSlot.
extern "C" void __cdecl Area146_LeaderBit138OtSlot(void) {
    unsigned char* const leader = Field_State;
    leader[0x138] = static_cast<unsigned char>(leader[0x138] & 0xFE);
    Sprite_Current[0x29] = Draw_OtSlot;
}

// original 0x422000 (Area_StepHook's case for area 0x92): Cond_ByteFD 1,
// story flag 0x33 set, x's high word 0x44..0x46 and z's 0x2A..0x2C, the
// leader's pose 0, 7 or 6: counter 0 = 0, Party_DropIn(0), al 1. Else al 0.
extern "C" unsigned char __cdecl Area146_StepHook(long x, long z) {
    if (Cond_ByteFD != 1) return 0;
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x33) == 0) return 0;
    if (static_cast<std::uint16_t>(High(static_cast<U>(x)) - 0x44) >= 3) return 0;
    if (static_cast<std::uint16_t>(High(static_cast<U>(z)) - 0x2A) >= 3) return 0;
    if (!PoseStanding()) return 0;
    return DropIn(0);
}

// original 0x422060 (Field_ObjectTriggers[32]): ScriptFlags_Set40, tail kind
// 0x2C (engine) with state 0 and sub-kind 4. al 0.
extern "C" unsigned char __cdecl Area146_Trigger32(void) { return ArmTail(0x2C, 4); }

// original 0x422080 (Effect_KindHandlers[0xB4]): Area146_EffectStates by the
// running record's +1 (unchecked there; ours aborts past its two).
extern "C" void __cdecl Area146_EffectB4Run(void) {
    StateEntry("Area146_EffectB4Run", ::bof3::Img(Area146_EffectStates_addr), Sprite_Current[1])();
}

// original 0x4220A0 (Area146_EffectStates[1]; state 0 is Area59_EffectGround
// 0x40B4F0): the running record's (x, z, y) handed to
// Area146_DrawGlowCylinder.
extern "C" void __cdecl Area146_EffectB4Cylinder(void) { CylinderAtCurrent(AH_CALL(Area146_DrawGlowCylinder)); }

// original 0x4220D0 (called by Area146_EffectB4Cylinder, by the effect states
// of areas 36, 59, 100, 112 and 116 - 0x405022, 0x40B542, 0x4144D2, ... - and
// by engine effect states at 0x475CE2 and 0x47EF32): GlowCylinder.
extern "C" void __cdecl Area146_DrawGlowCylinder(const long* point) { GlowCylinder(reinterpret_cast<const std::int32_t*>(point)); }

void Area146_Inject() {
    BOF3_INJECT(Area146_LeaderBit138OtSlot);
    BOF3_INJECT(Area146_StepHook);
    BOF3_INJECT(Area146_Trigger32);
    BOF3_INJECT(Area146_EffectB4Run);
    BOF3_INJECT(Area146_EffectB4Cylinder);
    BOF3_INJECT(Area146_DrawGlowCylinder);
}
