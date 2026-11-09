// World 3's areas 143..146: the PSX's BIN/WORLD03/AREA143..146.EMI compiled
// into the exe at 0x420800..0x4223A0 (Area_Descriptors entries 143..146;
// 147 has no code here). Round ten, group AR3F: the band's 53 functions, none
// ours before, each read to its last instruction with capstone (2026-09-28)
// and taken through the area harness (area_harness.h). docs/area_w3f.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through the areas' three two-state tables abort past the table
// (what follows is data, which the original would jump into); every other
// unchecked read stays inside .data or the field frame and is kept
// (docs/area_w3f.md section 6). Every call goes through the harness (AH_CALL
// / AH_AT), so the start-up fuzz can stand recorders in for ours as for the
// originals' copies; the callees of the group's own are called the same way,
// so each function is fuzzed alone.
#include "game/area_w3f.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3f::at;
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
U Key(const void* p) { return Low32(p); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }
// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(U v) { return static_cast<std::uint16_t>(v >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }
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

// ---- the glow cylinder (0x420B50, area 143's; 0x4220D0, area 146's) ----

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

// ---- area 144's walks (0x420FA0, 0x421040) ----

// ---- area 145's trails (0x421B20 .. 0x421EA0) ----

}  // namespace

// ===========================================================================
// Area 143 (descriptor 0x630C80; PSX 0x801F52B8): thirteen choices (choices
// 9..12 are handlers 0..3; 1, 2, 7, 8 and 10 other blocks' or shared), four
// handlers, a step hook, object trigger 50, effect kind 0xB3 and its
// cylinder, and a CLUT shift chapter 12's code calls.
// ===========================================================================

// ===========================================================================
// Area 144 (descriptor 0x631770; PSX 0x801F491C): nine handlers (7 and 8 its
// two choices).
// ===========================================================================

// ===========================================================================
// Area 145 (descriptor 0x6336A0; PSX 0x801F5C50): twelve choices (choices
// 1..11 are handlers 0..10; 3, 4, 10, 11 shared), a step hook, a cell hook,
// tail kind 20 with its trails, object trigger 40, the drop's two states, and
// EffectKind18_States entry 96. Its init is the bare ret 0x437CC0 (the PSX
// has one, 0x801F5324).
// ===========================================================================

// ===========================================================================
// Area 146 (descriptor 0x6340D0; PSX 0x801F3ABC): three handlers (two
// shared), a step hook, object trigger 32, effect kind 0xB4 and its
// cylinder (the copy the engine and five other areas call), and two bodies
// areas 148, 149 and 167 name.
// ===========================================================================

void AreaW3f_Inject() {
    if (bof3::WantsShadow("area_w3f")) area_w3f::SelfTest();
}
