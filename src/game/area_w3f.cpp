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

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3f_callees.h"
#include "game/move_script_bytes.h"
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
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(U v) { return static_cast<std::uint16_t>(v >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
unsigned char Answer() { return B(at::kChoiceAnswer); }
unsigned char* StoryFlags() { return Mem(at::kStoryFlags); }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(at::kFlagRow); }
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
    B(at::kTailKind) = kind;
    B(at::kTailState) = 0;
    B(at::kTailSub) = sub;
    return 0;
}

// The leader's pose is one the hooks accept: 0, 7 or 6.
bool PoseStanding() {
    const unsigned char pose = B(at::kLeaderPose);
    return pose == 0 || pose == 7 || pose == 6;
}
// The step hooks' drop-in: counter 0 cleared, then Party_DropIn(entry), al 1.
unsigned char DropIn(unsigned entry) {
    B(at::kCounter0) = 0;
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
            if (B(at::kLeader89 + j * at::kPartyStride) != B(keys + i)) continue;
            AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(Mem(messages + i * 2))));
            Field_Request = 2;
            return;
        }
    }
}

// Area 143's handler 3 (0x4209F0, also area 197's handler 9) and area 144's
// handlers 2 and 3 (0x421000, 0x421020): a byte equal to `value`, the script
// position + 3.
void SkipIf(U cell, unsigned char value) {
    if (B(cell) == value) ScriptStep(3);
}

// ---- the glow cylinder (0x420B50, area 143's; 0x4220D0, area 146's) ----

using ProjectFn = void (__cdecl*)(const std::int32_t*, std::uint32_t*);
void Project(const std::int32_t* v, std::uint32_t* out) { AH_AT(ProjectFn, at::kProjectPoint)(v, out); }
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
    AH_AT(void (__cdecl*)(), at::kSetMapCamera)();
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
    const long point[3] = {Long(cur + 0x34), Long(cur + 0x38), Long(cur + 0x3C)};
    cylinder(point);
}

// ---- area 144's walks (0x420FA0, 0x421040) ----

// (target - the running object's z) >> 15, 32-bit, arithmetic.
std::int32_t StepsToZ(U target) { return static_cast<std::int32_t>(target - static_cast<U>(Long(Sprite_Current + 0x38))) >> 15; }
unsigned char Magnitude(std::int32_t d) { return static_cast<unsigned char>(d < 0 ? 0u - static_cast<U>(d) : static_cast<U>(d)); }

// ---- area 145's trails (0x421B20 .. 0x421EA0) ----

// A leg: four bytes (a direction, a length, the next leg's table index, pad).
// The position `n` steps along the leg's direction from (x0, z0): the cell
// deltas (two signed bytes a direction) << 16, times n, 32-bit.
U AlongX(unsigned char dir, U n, U x0) {
    return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kCellDelta + dir * 2u))) << 16) * n + x0;
}
U AlongZ(unsigned char dir, U n, U z0) {
    return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kCellDelta + dir * 2u + 1))) << 16) * n + z0;
}
// Sprite_ObjectsExtra record k's dword +0x6C, bits 9..10: which of a leg
// table's four records.
unsigned Quarter(unsigned k) { return static_cast<unsigned>(Long(Mem(at::kExtraObjects + k * at::kObjectStride + 0x6C)) >> 9) & 3; }
// min(d, len) as the originals take it: 16-bit unsigned compare.
U Upto(std::uint16_t d, unsigned char len) { return d < len ? d : len; }

// Legs 1 and 2 (0x421C90, 0x421DB0): the same body with the bias `back`
// (5, 10), the tables for record 1 and for any other, and what follows (leg
// 2: none).
unsigned char TrailLeg(unsigned t, unsigned kk, unsigned back, U table1, U table_other, bool last) {
    const unsigned k = kk & 0xFF;
    const U off = k * at::kObjectStride;
    const unsigned q = Quarter(k);
    const U rec = (k == 1 ? table1 : table_other) + q * 4;
    const U d32 = t + (0x10000u - back);
    const auto d = static_cast<std::uint16_t>(d32);
    const unsigned char dir = B(rec);
    const U n = Upto(d, B(rec + 1));
    const U ox = static_cast<U>(Long(Mem(at::kExtraObjects + 0x34 + off)));
    const U x = AlongX(dir, n, ox);
    const U oz = static_cast<U>(Long(Mem(at::kExtraObjects + 0x38 + off)));
    const U z = AlongZ(dir, n, oz);
    AH_CALL(Area145_SpawnTrail)(static_cast<long>(ox), static_cast<long>(oz), static_cast<long>(x), static_cast<long>(z));
    const unsigned char len = B(rec + 1);
    if (d < len) return 1;
    if (last) return B(rec + 2);
    const unsigned char next = B(rec + 2);
    if (next == 0) return 1;
    if (d == len) AH_CALL(Sound_PlayEffect)(0x206);
    return AH_CALL(Area145_TrailLeg2)(d32, next);
}

// Area 145's effect spawn at DamageScratch: the slot byte stored there, read
// back as the originals do (a dword, masked).
unsigned char SpawnSlot() {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(at::kSlotScratch) = slot;
    return slot;
}
unsigned char* ScratchRecord() { return EffectAt(static_cast<U>(Long(Mem(at::kSlotScratch))) & 0xFF); }
// Tail states 0xA and 0x14: an effect of kind 0x13 at (+0x64 `x`, +0x68 the
// camera's angle 1 (s16), +0x6C `y`), +9 0x30.
void Kind13(unsigned char* e, U x, std::int32_t angle, U y) {
    e[0] = 1;
    e[5] = 0x13;
    SetLong(e + 0x64, static_cast<std::int32_t>(x));
    SetLong(e + 0x68, angle);
    SetLong(e + 0x6C, static_cast<std::int32_t>(y));
    e[9] = 0x30;
}
std::int32_t CameraAngle1() { return static_cast<std::int16_t>(Word(Mem(at::kCameraAngle1))); }
void Disarm() {
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}
// The tail's word timer: not 0, one less and false; 0, true.
bool TimerOut() {
    const std::uint16_t t = Word(Mem(at::kTailTimer));
    if (t != 0) {
        SetWord(Mem(at::kTailTimer), t - 1u);
        return false;
    }
    return true;
}

}  // namespace

// ===========================================================================
// Area 143 (descriptor 0x630C80; PSX 0x801F52B8): thirteen choices (choices
// 9..12 are handlers 0..3; 1, 2, 7, 8 and 10 other blocks' or shared), four
// handlers, a step hook, object trigger 50, effect kind 0xB3 and its
// cylinder, and a CLUT shift chapter 12's code calls.
// ===========================================================================

// original 0x420800 (area 143 +0x34[0]): answer not 0: message 0x54 and the
// mark 0x9398CF 6. 0: the dword 0x904650 equal to 0x3FFFF, message 0x52;
// else message 0x53 and the mark 6.
extern "C" void __cdecl Area143_ChoiceAsk52(void) {
    if (Answer() != 0) {
        SetMessage(0x54);
        B(at::kAnswerMark) = 6;
        return;
    }
    if (static_cast<U>(Long(Mem(at::kFlags904650))) == 0x3FFFFu) {
        SetMessage(0x52);
        return;
    }
    SetMessage(0x53);
    B(at::kAnswerMark) = 6;
}

// original 0x420850 (area 143 +0x34[1]; also areas 3, 37, 41, 50, 55, 59,
// 61, 68, 74, 91, 98, 113 and 116's): answer not 0: the byte 0x9398D1 = 3;
// message 0xFFFF either way.
extern "C" void __cdecl Area143_ChoiceYesMark3(void) {
    if (Answer() != 0) B(at::kAnswerMarkD1) = 3;
    SetMessage(0xFFFF);
}

// original 0x420870 (area 143 +0x34[2]; the same fourteen areas): answer not
// 0: 0x9398D1 = 5; message 0xFFFF either way.
extern "C" void __cdecl Area143_ChoiceYesMark5(void) {
    if (Answer() != 0) B(at::kAnswerMarkD1) = 5;
    SetMessage(0xFFFF);
}

// original 0x420890 (area 143 +0x34[3], [4]): answer not 0: message 0x54
// and the mark 6; 0: message 0xFFFF.
extern "C" void __cdecl Area143_ChoiceAsk54(void) {
    if (Answer() != 0) {
        SetMessage(0x54);
        B(at::kAnswerMark) = 6;
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
    const bool asked = Word(Mem(at::kMessage)) == 0x44;
    MoveScript_Var7 = 8;
    if (asked) {
        B(at::kVar7Step) = 8;
        unsigned char* const focus = Ptr(at::kFocusObject);
        SetMessage(0x4B);
        AddWord(focus + 0x8A, 1);
        return;
    }
    B(at::kVar7Step) = 0xA;
    SetMessage(0xFFFF);
}

// original 0x420940 (area 143 +0x3C[0] = +0x34[9]; area 145 +0x3C[2] =
// +0x34[3]; area 146 +0x3C[0]; PSX 0x801F3C10 / 0x801F3FD8 / 0x801F2C04):
// Field_ChangeArea(0x70, 0xC8000, 0x580000, 0x81).
extern "C" void __cdecl Area143_ChangeArea70(void) { AH_CALL(Field_ChangeArea)(0x70, 0xC8000, 0x580000, 0x81); }

// original 0x420960 (area 143 +0x3C[2] = +0x34[11]; PSX 0x801F3C74): the
// first of Area143_MemberKeys some member's +0x89 holds opens its message
// from Area143_MemberMessages, Field_Request 2 (MessageByMember).
extern "C" void __cdecl Area143_MessageByMember(void) { MessageByMember(at::kArea143MemberKeys, at::kArea143MemberMessages); }

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
    B(at::kTailKind) = 4;
    B(at::kTailSub) = 0xF;
    if (fe == 0) {
        Cond_ByteFE = 1;
        B(at::kTailState) = 5;
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
    U at = at::kClutSourceRows;
    do {
        for (unsigned i = 0; i < 0x100; ++i, at += 2) {
            const U w = Word(Mem(at));
            const U r = ((w >> 10) & 0x1F) >> s;
            const U g = ((w >> 5) & 0x1F) >> s;
            const U b = (w & 0x1F) >> s;
            SetWord(Mem(at + at::kClutToLive), ((r << 5 | g) << 5 | b) | (w & 0x8000));
        }
    } while (static_cast<std::int32_t>(at) < static_cast<std::int32_t>(at::kClutSourceEnd));
    Gfx_ClutStripDirty = 1;
    return 0;
}

// original 0x420B00 (Effect_KindHandlers[0xB3]): Area143_EffectStates by the
// running record's +1 (unchecked there; ours aborts past its two).
extern "C" void __cdecl Area143_EffectB3Run(void) {
    StateEntry("Area143_EffectB3Run", at::kArea143EffectStates, Sprite_Current[1])();
}

// original 0x420B20 (Area143_EffectStates[1]; state 0 is Area59_EffectGround
// 0x40B4F0): the running record's (x, z, y) to the stack, handed to
// Area143_DrawGlowCylinder.
extern "C" void __cdecl Area143_EffectB3Cylinder(void) { CylinderAtCurrent(AH_CALL(Area143_DrawGlowCylinder)); }

// original 0x420B50 (called by Area143_EffectB3Cylinder only): GlowCylinder,
// area 143's copy.
extern "C" void __cdecl Area143_DrawGlowCylinder(const long* point) { GlowCylinder(reinterpret_cast<const std::int32_t*>(point)); }

// ===========================================================================
// Area 144 (descriptor 0x631770; PSX 0x801F491C): nine handlers (7 and 8 its
// two choices).
// ===========================================================================

// original 0x420DD0 (area 144 +0x3C[0]; PSX 0x801F3818): by the chapter
// row's flags 0x17..0x1B (the first clear) and the leader's +0x89, the script
// object's +3 = a scene (2..0xD), a flag of the row set on three of them;
// the script position = 0xFFFE (stored, not added) every way out.
extern "C" void __cdecl Area144_SceneByRowFlags(void) {
    auto scene = [](unsigned char n) { MoveScript_Object[3] = n; };
    auto rewind = [] { SetWord(MoveScript_Object + 0xA, 0xFFFE); };
    auto set_row = [](unsigned index) { AH_CALL(Flags_Set)(FlagRow(), index); };
    if (AH_CALL(Flags_Test)(FlagRow(), 0x17) == 0) {
        scene(2);
        rewind();
        rewind();
        return;
    }
    if (AH_CALL(Flags_Test)(FlagRow(), 0x18) == 0) {
        if (B(at::kLeader89) == 2) {
            scene(4);
            set_row(0x18);
        } else {
            scene(3);
        }
        rewind();
        return;
    }
    if (AH_CALL(Flags_Test)(FlagRow(), 0x19) == 0) {
        const unsigned char who = B(at::kLeader89);
        if (who == 8) {
            scene(7);
            set_row(0x19);
        } else {
            scene(who == 5 ? 6 : 5);
        }
        rewind();
        return;
    }
    if (AH_CALL(Flags_Test)(FlagRow(), 0x1A) == 0) {
        const unsigned char who = B(at::kLeader89);
        scene(who == 7 ? 0xA : who == 8 ? 9 : 8);
        rewind();
        return;
    }
    if (AH_CALL(Flags_Test)(FlagRow(), 0x1B) == 0) {
        if (B(at::kLeader89) == 7) {
            scene(0xD);
            set_row(0x1B);
        } else {
            scene(0xC);
        }
    }
    rewind();
}

// original 0x420FA0 (area 144 +0x3C[1]; PSX 0x801F3A20): d = (0x1D8000 -
// the running object's z) >> 15; not 0: the script object's +7 = |d|, the
// running object's direction 1 (d negative) or 5, MoveCmd_Move(the script
// object, that direction).
extern "C" void __cdecl Area144_WalkToZ1D8(void) {
    const std::int32_t d = StepsToZ(0x1D8000);
    if (d == 0) return;
    MoveScript_Object[7] = Magnitude(d);
    Sprite_Current[8] = d < 0 ? 1 : 5;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, Sprite_Current[8]);
}

// original 0x421000 (area 144 +0x3C[2]; PSX 0x801F3AB4): the third party
// record's +0x89 4, the script position + 3.
extern "C" void __cdecl Area144_SkipIfMember3Is4(void) { SkipIf(at::kMember3_89, 4); }

// original 0x421020 (area 144 +0x3C[3]; PSX 0x801F3AEC): the third record's
// +0x89 2, the script position + 3.
extern "C" void __cdecl Area144_SkipIfMember3Is2(void) { SkipIf(at::kMember3_89, 2); }

// original 0x421040 (area 144 +0x3C[4]; PSX 0x801F3B24): d = (0x1C0000 -
// the running object's z) >> 15; not 0: the running object's direction 1,
// the script object's +7 = |d|, MoveCmd_Move(the script object, direction).
// Field_ScriptFlags' low byte | 8 either way.
extern "C" void __cdecl Area144_WalkToZ1C0(void) {
    unsigned char* const cur = Sprite_Current;
    const std::int32_t d = StepsToZ(0x1C0000);
    if (d != 0) {
        cur[8] = 1;
        MoveScript_Object[7] = Magnitude(d);
        AH_CALL(MoveCmd_Move)(MoveScript_Object, Sprite_Current[8]);
    }
    B(at::kScriptFlagsLow) = static_cast<unsigned char>(B(at::kScriptFlagsLow) | 8);
}

// original 0x421090 (area 144 +0x3C[5]; PSX 0x801F3BAC): Effect_FindFree;
// none: the script position - 2. A slot: kind 0x85, +6 the active member's
// index in Sprite_Objects ((pointer - 0x7DEE80) / 0xA4, signed, truncated).
extern "C" void __cdecl Area144_SpawnEffect85(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) {
        ScriptStep(-2);
        return;
    }
    const auto index = static_cast<std::int32_t>(Key(Field_ActiveMember) - 0x7DEE80u) / 0xA4;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x85;
    e[6] = static_cast<unsigned char>(index);
}

// original 0x4210F0 (area 144 +0x3C[6]; PSX 0x801F3C78): the kind-2 object
// (Sprite_Kind2) placed at the running object's (x, z, y), its +0x84 the
// script object's +4, its +0x87 1; MoveCmd_MoveKind2(3).
extern "C" void __cdecl Area144_MoveKind2Here(void) {
    const unsigned char* const cur = Sprite_Current;
    unsigned char* const k2 = Mem(at::kKind2);
    SetLong(k2 + 0x34, Long(cur + 0x34));
    const std::int32_t z = Long(cur + 0x38);
    const unsigned char* const so = MoveScript_Object;
    SetLong(k2 + 0x38, z);
    SetLong(k2 + 0x3C, Long(cur + 0x3C));
    k2[0x84] = so[4];
    k2[0x87] = 1;
    AH_CALL(MoveCmd_MoveKind2)(3);
}

// original 0x421130 (area 144 +0x34[0] = +0x3C[7]; PSX 0x801F3CE4): message
// 0xFFFF; counter 0 = 0x23 for an answer, else 0x1E.
extern "C" void __cdecl Area144_ChoiceCounter1E(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    B(at::kCounter0) = answer != 0 ? 0x23 : 0x1E;
}

// original 0x421150 (area 144 +0x34[1] = +0x3C[8]; PSX 0x801F3D14): message
// 0xFFFF; answer 0: counter 0 = 5, Party_DropIn(6), MoveScript_Var7 7, the
// step 0x14; else Party_DropIn(5).
extern "C" void __cdecl Area144_ChoiceDropIn(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0) {
        AH_CALL(Party_DropIn)(5);
        return;
    }
    B(at::kCounter0) = 5;
    AH_CALL(Party_DropIn)(6);
    MoveScript_Var7 = 7;
    B(at::kVar7Step) = 0x14;
}

// ===========================================================================
// Area 145 (descriptor 0x6336A0; PSX 0x801F5C50): twelve choices (choices
// 1..11 are handlers 0..10; 3, 4, 10, 11 shared), a step hook, a cell hook,
// tail kind 20 with its trails, object trigger 40, the drop's two states, and
// EffectKind18_States entry 96. Its init is the bare ret 0x437CC0 (the PSX
// has one, 0x801F5324).
// ===========================================================================

// original 0x421190 (area 145 +0x34[0]): message 0xFFFF; an answer: counter
// 0 = 8; 0: MoveScript_Var7 3, counter 0 = 0xA, the step 0xA.
extern "C" void __cdecl Area145_ChoiceRun3(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0) {
        B(at::kCounter0) = 8;
        return;
    }
    MoveScript_Var7 = 3;
    B(at::kCounter0) = 0xA;
    B(at::kVar7Step) = 0xA;
}

// original 0x4211C0 (area 145 +0x3C[0] = +0x34[1]; PSX 0x801F3CE4): i = the
// active member's index in Sprite_ObjectsExtra ((pointer - 0x802000) / 0xA4,
// signed, truncated) as a byte, to 0x903851; MoveScript_PartyRecords record
// i: +2 = 0x10, +1 = 0, dword +0xC = 0x200 / (s8)+2 (read back: 0x20); the
// script object's +0 | 8.
extern "C" void __cdecl Area145_PartyRecord16(void) {
    const auto index = static_cast<unsigned char>(static_cast<std::int32_t>(Key(Field_ActiveMember) - at::kExtraObjects) / 0xA4);
    B(at::kRecordIndex) = index;
    unsigned char* const rec = Mem(at::kPartyRecords + index * 16u);
    rec[2] = 0x10;
    rec[1] = 0;
    SetLong(rec + 0xC, 0x200 / static_cast<signed char>(rec[2]));
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 8);
}

// original 0x421220 (area 145 +0x3C[1] = +0x34[2]; PSX 0x801F3DCC):
// Area145_DropStates by the running object's +4 (unchecked there; ours
// aborts past its two).
extern "C" void __cdecl Area145_RunDrop(void) { StateEntry("Area145_RunDrop", at::kArea145DropStates, Sprite_Current[4])(); }

// original 0x421240 (Area145_DropStates[0]): the running object's bit 0x40
// cleared; its word +0x3E the ground at it (MapView_GroundAt) + 0x7D0; +0xC,
// +0x10, +0x14 0; +0x20 -8; state 1; Sprite_SetAnimation(0x3B); the script
// object's bit 0x40, its +1 = Field_State's party index (/ 0x14C) x 0x14,
// the script position - 2. Sprite_Current read again for every store.
extern "C" void __cdecl Area145_DropStart(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    const long ground = AH_CALL(MapView_GroundAt)(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<U>(ground) + 0x7D0u);
    SetLong(Sprite_Current + 0xC, 0);
    SetLong(Sprite_Current + 0x10, 0);
    SetLong(Sprite_Current + 0x14, 0);
    SetLong(Sprite_Current + 0x20, -8);
    Sprite_Current[4] = 1;
    AH_CALL(Sprite_SetAnimation)(0x3B);
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    const auto index = static_cast<std::int32_t>(Key(Field_State) - 0x802D40u) / 0x14C;
    MoveScript_Object[1] = static_cast<unsigned char>(static_cast<U>(index) * 0x14u);
    ScriptStep(-2);
}

// original 0x4212F0 (Area145_DropStates[1]): +0x14 += +0x20;
// Field_LeaderStepTick; the ground at the running object above its word
// +0x3E (signed 16-bit): landed - +0x3E the ground, +0x14 and +0x20 0, state
// 2, Sprite_SetAnimation(0x39), the script object's +1 = 4. Else the script
// position - 2.
extern "C" void __cdecl Area145_DropFall(void) {
    SetLong(Sprite_Current + 0x14, static_cast<std::int32_t>(static_cast<U>(Long(Sprite_Current + 0x14)) + static_cast<U>(Long(Sprite_Current + 0x20))));
    AH_CALL(Field_LeaderStepTick)();
    const long ground = AH_CALL(MapView_GroundAt)(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
    unsigned char* const cur = Sprite_Current;
    if (static_cast<std::int16_t>(ground) <= static_cast<std::int16_t>(Word(cur + 0x3E))) {
        ScriptStep(-2);
        return;
    }
    SetWord(cur + 0x3E, static_cast<U>(ground));
    SetLong(Sprite_Current + 0x14, 0);
    SetLong(Sprite_Current + 0x20, 0);
    Sprite_Current[4] = 2;
    AH_CALL(Sprite_SetAnimation)(0x39);
    MoveScript_Object[1] = 4;
}

// original 0x421370 (area 145 +0x3C[4] = +0x34[5]; PSX 0x801F403C):
// MessageByMember with Area145_MemberKeys1 / Messages1.
extern "C" void __cdecl Area145_MessageByMember1(void) { MessageByMember(at::kArea145MemberKeys1, at::kArea145MemberMessages1); }

// original 0x421400 (area 145 +0x3C[5] = +0x34[6]; PSX 0x801F4100): with
// Area145_MemberKeys5 / Messages5.
extern "C" void __cdecl Area145_MessageByMember5(void) { MessageByMember(at::kArea145MemberKeys5, at::kArea145MemberMessages5); }

// original 0x421490 (area 145 +0x3C[6] = +0x34[7]; PSX 0x801F41C4): with
// Area145_MemberKeysB / MessagesB.
extern "C" void __cdecl Area145_MessageByMemberB(void) { MessageByMember(at::kArea145MemberKeysB, at::kArea145MemberMessagesB); }

// original 0x421520 (area 145 +0x3C[7] = +0x34[8]; PSX 0x801F4288): the 16
// bytes of Item_NamePtr(4, 0xB) (key item 0xB's name) to Text_Records, dword
// by dword; KeyItem_Add(0xB); Msg_OpenSystem(2); Field_Request 2.
extern "C" void __cdecl Area145_GiveKeyItemB(void) {
    const unsigned char* const name = AH_CALL(Item_NamePtr)(4, 0xB);
    unsigned char* const text = Mem(at::kTextRecords);
    for (unsigned i = 0; i < 16; i += 4) SetLong(text + i, Long(name + i));
    AH_CALL(KeyItem_Add)(0xB);
    AH_CALL(Msg_OpenSystem)(2);
    Field_Request = 2;
}

// original 0x421570 (area 145 +0x3C[8] = +0x34[9]; PSX 0x801F4304): key item
// 0xB held, the script position + 3.
extern "C" void __cdecl Area145_SkipIfKeyItemB(void) {
    if (AH_CALL(KeyItem_Has)(0xB) != 0) ScriptStep(3);
}

// original 0x421590 (Field_ObjectTriggers[40]): ScriptFlags_Set40, tail kind
// 0x2C (engine) with state 0 and sub-kind 0xF. al 0.
extern "C" unsigned char __cdecl Area145_Trigger40(void) { return ArmTail(0x2C, 0xF); }

// original 0x4215B0 (Area_StepHook's case for area 0x91): Cond_ByteFD 0: z
// exactly 0x98000, x's high word 0x1B..0x1C and the leader's too (16-bit
// compares): ScriptFlags_Set40, tail kind 20 at state 0x19, al 1.
// Cond_ByteFD 4: story flag 0x2D set, x's high word 9..0xB and z's
// 0x62..0x64, the leader's pose 0, 7 or 6: counter 0 = 0, Party_DropIn(3),
// al 1. Else al 0.
extern "C" unsigned char __cdecl Area145_StepHook(long x, long z) {
    const unsigned char fd = Cond_ByteFD;
    if (fd == 0) {
        if (static_cast<U>(z) != 0x98000u) return 0;
        if (static_cast<std::uint16_t>(High(static_cast<U>(x)) - 0x1B) >= 2) return 0;
        if (static_cast<std::uint16_t>(Word(Mem(at::kLeaderXHigh)) - 0x1B) >= 2) return 0;
        AH_CALL(ScriptFlags_Set40)();
        B(at::kTailKind) = 0x14;
        B(at::kTailState) = 0x19;
        return 1;
    }
    if (fd != 4) return 0;
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x2D) == 0) return 0;
    if (static_cast<std::uint16_t>(High(static_cast<U>(x)) - 9) >= 3) return 0;
    if (static_cast<std::uint16_t>(High(static_cast<U>(z)) - 0x62) >= 3) return 0;
    if (!PoseStanding()) return 0;
    return DropIn(3);
}

// original 0x421660 (Area_CellHooks' record for area 0x91): the first of
// Area145_CellRecords whose x and z bytes are the cell's (low bytes) and
// whose low nibble of +2 is the leader's pose (read once, whole): with
// Cond_Flags row 13's flag 9, ScriptFlags_Set40, the tail's state = the
// record's +3 and kind 20, al 1. None, or the flag clear: al 0.
extern "C" unsigned char __cdecl Area145_CellHook(long x, long z) {
    const auto zb = static_cast<unsigned char>(z);
    const unsigned char pose = B(at::kLeaderPose);
    const auto xb = static_cast<unsigned char>(x);
    unsigned i = 0;
    for (; i < at::kArea145CellRecordCount; ++i) {
        const U rec = at::kArea145CellRecords + i * 4;
        if (B(rec) == xb && B(rec + 1) == zb && (B(rec + 2) & 0xF) == pose) break;
    }
    if (i == at::kArea145CellRecordCount) return 0;
    if (AH_CALL(Flags_Test)(Mem(at::kCondRow13), 9) == 0) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailState) = B(at::kArea145CellRecords + i * 4 + 3);
    B(at::kTailKind) = 0x14;
    return 1;
}

// original 0x4216D0 (Field_ModeTailKinds[20]; a jump table of 18 through a
// byte table of 35, by the s8 state - 2): the tail its cell hook arms at
// states 2, 4, 6 and 0xA and its step hook at 0x19. 2, 4, 6: counter 3 =
// the state, state 8, which waits for counter 3 to be 0 and disarms. 0xA:
// story flag 0x2C, sound 0x200, Kind2_Place(0) and an effect of kind 0x13
// (none: stays), then 0xB waits for counter 3 = 0x40 (timer 0, sound 0x204),
// 0xC runs the trails a frame at a time (Area145_TrailStart by the timer + 1)
// until they answer 0 (flag 0x2D clear: sounds 0x205, 0x207 and state 0x1E;
// set: as 2) or 2 (sound 0x205, state 0x14); 0x14 another kind 0x13 effect
// with the kind-2 target at the leader; 0x15 waits for Field_Kind2Hold, then
// 0x16 clears flag 0x2C and disarms. 0x19..0x1A: the leader's +1..+3, a
// 0x17-frame wait, the area 0x91 change and flag 0x2E (sound 0x20C the first
// time). 0x1E..0x24: transitions 8 and 9, an effect of kind 0x3E, waits,
// flag 0x2D, two area 0x91 changes, back to 0x16. Every other state: nothing.
extern "C" void __cdecl Area145_Tail20(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    switch (state) {
    case 2: case 4: case 6:
        B(at::kCounter3) = static_cast<unsigned char>(state);
        B(at::kTailState) = 8;
        return;
    case 8:
        if (B(at::kCounter3) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0xA: {
        AH_CALL(Flags_Set)(StoryFlags(), 0x2C);
        AH_CALL(Sound_PlayEffect)(0x200);
        AH_CALL(Kind2_Place)(0);
        if (SpawnSlot() == 0xFF) return;
        unsigned char* const e = ScratchRecord();
        B(at::kTailState) = 0xB;
        Kind13(e, 0xFFFFFCE6u, CameraAngle1(), 0x260);
        return;
    }
    case 0xB:
        if (B(at::kCounter3) != 0x40) return;
        B(at::kTailState) = 0xC;
        SetWord(Mem(at::kTailTimer), 0);
        AH_CALL(Sound_PlayEffect)(0x204);
        return;
    case 0xC: {
        const auto t = static_cast<std::uint16_t>(Word(Mem(at::kTailTimer)) + 1);
        SetWord(Mem(at::kTailTimer), t);
        const unsigned char r = AH_CALL(Area145_TrailStart)(t);
        if (r == 1) return;
        if (r == 0 && AH_CALL(Flags_Test)(StoryFlags(), 0x2D) == 0) {
            AH_CALL(Sound_PlayEffect)(0x205);
            AH_CALL(Sound_PlayEffect)(0x207);
            B(at::kTailState) = 0x1E;
            return;
        }
        AH_CALL(Sound_PlayEffect)(0x205);
        B(at::kTailState) = 0x14;
        return;
    }
    case 0x14: {
        if (SpawnSlot() == 0xFF) return;
        unsigned char* const e = ScratchRecord();
        const std::int32_t leader_z = Long(Mem(at::kLeaderZ));
        const std::int32_t angle = CameraAngle1();
        Field_Kind2Z = leader_z;
        B(at::kTailState) = 0x15;
        Kind13(e, 0xFFFFFD56u, angle, 0x200);
        Field_Kind2X = Long(Mem(at::kLeaderX));
        return;
    }
    case 0x15:
        if (Field_Kind2Hold != 0) return;
        B(at::kCounter3) = 0;
        B(at::kTailState) = 0x16;
        return;
    case 0x16:
        AH_CALL(Flags_Clear)(StoryFlags(), 0x2C);
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0x19:
        B(at::kLeader1) = 2;
        B(at::kLeader1 + 1) = 3;
        B(at::kLeader1 + 2) = 0;
        SetWord(Mem(at::kTailTimer), 0x17);
        B(at::kTailState) = 0x1A;
        return;
    case 0x1A:
        if (!TimerOut()) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0x91, 0x3B8000, 0x5D0000, 0x82);
        if (AH_CALL(Flags_Test)(StoryFlags(), 0x2E) == 0) {
            AH_CALL(Sound_PlayEffect)(0x20C);
            AH_CALL(Flags_Set)(StoryFlags(), 0x2E);
        }
        Disarm();
        return;
    case 0x1E:
        AH_CALL(Transition_Start)(8);
        Field_Kind2X = 0x308000;
        Field_Kind2Z = 0x638000;
        B(at::kTailState) = 0x1F;
        return;
    case 0x1F:
        if (MoveScript_WaitWordDA != 0) return;
        AH_CALL(Transition_Start)(9);
        if (SpawnSlot() != 0xFF) {
            unsigned char* const e = ScratchRecord();
            e[0] = 1;
            e[5] = 0x3E;
        }
        B(at::kTailState) = 0x20;
        return;
    case 0x20:
        if (Field_Kind2Hold != 0) return;
        SetWord(Mem(at::kTailTimer), 0x1E);
        B(at::kTailState) = 0x21;
        return;
    case 0x21:
        if (!TimerOut()) return;
        B(at::kTailState) = 0x22;
        return;
    case 0x22:
        AH_CALL(Flags_Set)(StoryFlags(), 0x2D);
        AH_CALL(Field_ChangeArea)(0x91, 0xA0000, 0x630000, 0x80);
        B(at::kTailState) = 0x23;
        return;
    case 0x23:
        if (MoveScript_WaitWordDA != 0) return;
        SetWord(Mem(at::kTailTimer), 0x5A);
        B(at::kTailState) = 0x24;
        return;
    case 0x24:
        if (!TimerOut()) return;
        AH_CALL(Field_ChangeArea)(0x91, 0x3E0000, 0x5F0000, 1);
        B(at::kTailState) = 0x16;
        return;
    default:
        return;
    }
}

// original 0x421B20 (called by Area145_Tail20's state 0xC with the timer):
// t (16-bit) below 5: the start trail (0x428000, 0x638000) to (0x428000 -
// t << 16, 0x638000) and al 1. From 5: that trail at length 5, at 5 sound
// 0x206, then Area145_TrailLeg0(t); al its answer up to t 0x37, 2 past it.
extern "C" unsigned char __cdecl Area145_TrailStart(unsigned t) {
    const auto s = static_cast<std::uint16_t>(t);
    const U n = s < 5 ? s : 5;
    AH_CALL(Area145_SpawnTrail)(0x428000, 0x638000, static_cast<long>(0x428000u - (n << 16)), 0x638000);
    if (s < 5) return 1;
    if (s == 5) AH_CALL(Sound_PlayEffect)(0x206);
    const unsigned char r = AH_CALL(Area145_TrailLeg0)(t);
    return s <= 0x37 ? r : 2;
}

// original 0x421B90: the first leg, Area145_TrailLegs0's record by
// Sprite_ObjectsExtra record 0's quarter: d = t - 5 (16-bit), n = min(d, its
// length); the trail from (0x3D8000, 0x638000) to record 0's (x, z) + n
// cells along its direction. d below the length, or no next leg: al 1. Else
// (sound 0x206 at d equal to the length) Area145_TrailLeg1(t - 5, next).
extern "C" unsigned char __cdecl Area145_TrailLeg0(unsigned t) {
    const U rec = at::kArea145Legs0 + Quarter(0) * 4;
    const U d32 = t + 0xFFFBu;
    const auto d = static_cast<std::uint16_t>(d32);
    const unsigned char dir = B(rec);
    const U n = Upto(d, B(rec + 1));
    const U x = AlongX(dir, n, static_cast<U>(Long(Mem(at::kExtraObjects + 0x34))));
    const U z = AlongZ(dir, n, static_cast<U>(Long(Mem(at::kExtraObjects + 0x38))));
    AH_CALL(Area145_SpawnTrail)(0x3D8000, 0x638000, static_cast<long>(x), static_cast<long>(z));
    const unsigned char len = B(rec + 1);
    if (d < len) return 1;
    const unsigned char next = B(rec + 2);
    if (next == 0) return 1;
    if (d == len) AH_CALL(Sound_PlayEffect)(0x206);
    return AH_CALL(Area145_TrailLeg1)(d32, next);
}

// original 0x421C90: the second leg of record k (its low byte): the table
// Area145_TrailLegs1A for record 1, 1B for any other, by record k's quarter;
// d = t - 5; the trail from record k's (x, z) to it + min(d, length) cells;
// d below the length or no next leg: al 1; else (sound 0x206 at d equal to
// the length) Area145_TrailLeg2(t - 5, next).
extern "C" unsigned char __cdecl Area145_TrailLeg1(unsigned t, unsigned k) {
    return TrailLeg(t, k, 5, at::kArea145Legs1A, at::kArea145Legs1B, false);
}

// original 0x421DB0: the third leg: tables Area145_TrailLegs2A / 2B, d = t -
// 10; the trail as the second's; d below the length: al 1, else al the
// record's +2 (0 or 2).
extern "C" unsigned char __cdecl Area145_TrailLeg2(unsigned t, unsigned k) {
    return TrailLeg(t, k, 10, at::kArea145Legs2A, at::kArea145Legs2B, true);
}

// original 0x421EA0: Effect_FindFree to DamageScratch; none: al 1. A slot:
// kind 0x3F from (x0, z0) (+0xC, +0x10) to (x1, z1) (+0x18, +0x1C), +0x20 and
// +0x14 0x500000; al 0.
extern "C" unsigned char __cdecl Area145_SpawnTrail(long x0, long z0, long x1, long z1) {
    if (SpawnSlot() == 0xFF) return 1;
    unsigned char* const e = ScratchRecord();
    e[0] = 1;
    e[5] = 0x3F;
    SetLong(e + 0xC, static_cast<std::int32_t>(x0));
    SetLong(e + 0x10, static_cast<std::int32_t>(z0));
    SetLong(e + 0x18, static_cast<std::int32_t>(x1));
    SetLong(e + 0x1C, static_cast<std::int32_t>(z1));
    SetLong(e + 0x20, 0x500000);
    SetLong(e + 0x14, 0x500000);
    return 0;
}

// original 0x421F10 (EffectKind18_States[96]): while Cond_ByteFE: a
// draw-mode packet (dtd 1, tpage 0x95) committed to slot 7 (0xC), then a
// full-screen POLY_G4 (0, 0)..(320, 240) as floats, not semi-transparent,
// colour (0, 0xC8, 0xFF) at the top and (0, 0, 0x20) at the bottom,
// committed to slot 7 (0x44).
extern "C" void __cdecl Area145_DrawGradient(void) {
    if (Cond_ByteFE == 0) return;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    AH_CALL(Gfx_CommitPrim)(7, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyG4)(prim);
    AH_CALL(Gpu_SetSemiTrans)(prim, 0);
    constexpr std::int32_t k240 = 0x43700000, k320 = 0x43A00000;   // 240.0f, 320.0f
    SetLong(prim + 0x2C, k240);
    SetLong(prim + 0x3C, k240);
    prim[0x15] = prim[5] = 0xC8;
    prim[0x16] = prim[6] = 0xFF;
    SetLong(prim + 8, 0);
    SetLong(prim + 0xC, 0);
    SetLong(prim + 0x18, k320);
    SetLong(prim + 0x1C, 0);
    SetLong(prim + 0x28, 0);
    SetLong(prim + 0x38, k320);
    prim[0x14] = prim[4] = 0;
    prim[0x34] = prim[0x24] = 0;
    prim[0x35] = prim[0x25] = 0;
    prim[0x36] = prim[0x26] = 0x20;
    AH_CALL(Gfx_CommitPrim)(7, 0x44);
}

// original 0x421FB0 (area 145 +0x3C[3] = +0x34[4]; also areas 36, 59, 100,
// 112, 116, 143 and 146's handler 1; PSX 0x801F4008): the running object's
// +0x48 = 0, Sprite_ReleaseTint(it).
extern "C" void __cdecl Area145_ClearTint(void) {
    Sprite_Current[0x48] = 0;
    AH_CALL(Sprite_ReleaseTint)(Sprite_Current);
}

// ===========================================================================
// Area 146 (descriptor 0x6340D0; PSX 0x801F3ABC): three handlers (two
// shared), a step hook, object trigger 32, effect kind 0xB4 and its
// cylinder (the copy the engine and five other areas call), and two bodies
// areas 148, 149 and 167 name.
// ===========================================================================

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
    StateEntry("Area146_EffectB4Run", at::kArea146EffectStates, Sprite_Current[1])();
}

// original 0x4220A0 (Area146_EffectStates[1]; state 0 is Area59_EffectGround
// 0x40B4F0): the running record's (x, z, y) handed to
// Area146_DrawGlowCylinder.
extern "C" void __cdecl Area146_EffectB4Cylinder(void) { CylinderAtCurrent(AH_CALL(Area146_DrawGlowCylinder)); }

// original 0x4220D0 (called by Area146_EffectB4Cylinder, by the effect states
// of areas 36, 59, 100, 112 and 116 - 0x405022, 0x40B542, 0x4144D2, ... - and
// by engine effect states at 0x475CE2 and 0x47EF32): GlowCylinder.
extern "C" void __cdecl Area146_DrawGlowCylinder(const long* point) { GlowCylinder(reinterpret_cast<const std::int32_t*>(point)); }

// original 0x422350 (area 148 +0x3C[1] = +0x34[3], area 167 +0x3C[1] =
// +0x34[6]; in area 146's block): story flag 0x46 cleared, Cond_ByteFE 0.
extern "C" void __cdecl Area146_ClearFlag46(void) {
    AH_CALL(Flags_Clear)(StoryFlags(), 0x46);
    Cond_ByteFE = 0;
}

// original 0x422370 (area 148 +0x3C[2] = +0x34[4], area 149 +0x3C[0], area
// 167 +0x3C[8] = +0x34[13]; in area 146's block): the running object's x, z,
// y = Sprite_ObjectsExtra record 0's, Sprite_Current read again for each.
extern "C" void __cdecl Area146_ToExtraObject0(void) {
    SetLong(Sprite_Current + 0x34, Long(Mem(at::kExtraObjects + 0x34)));
    SetLong(Sprite_Current + 0x38, Long(Mem(at::kExtraObjects + 0x38)));
    SetLong(Sprite_Current + 0x3C, Long(Mem(at::kExtraObjects + 0x3C)));
}

void AreaW3f_Inject() {
    if (bof3::WantsShadow("area_w3f")) area_w3f::SelfTest();
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
    BOF3_INJECT(Area144_SceneByRowFlags);
    BOF3_INJECT(Area144_WalkToZ1D8);
    BOF3_INJECT(Area144_SkipIfMember3Is4);
    BOF3_INJECT(Area144_SkipIfMember3Is2);
    BOF3_INJECT(Area144_WalkToZ1C0);
    BOF3_INJECT(Area144_SpawnEffect85);
    BOF3_INJECT(Area144_MoveKind2Here);
    BOF3_INJECT(Area144_ChoiceCounter1E);
    BOF3_INJECT(Area144_ChoiceDropIn);
    BOF3_INJECT(Area145_ChoiceRun3);
    BOF3_INJECT(Area145_PartyRecord16);
    BOF3_INJECT(Area145_RunDrop);
    BOF3_INJECT(Area145_DropStart);
    BOF3_INJECT(Area145_DropFall);
    BOF3_INJECT(Area145_MessageByMember1);
    BOF3_INJECT(Area145_MessageByMember5);
    BOF3_INJECT(Area145_MessageByMemberB);
    BOF3_INJECT(Area145_GiveKeyItemB);
    BOF3_INJECT(Area145_SkipIfKeyItemB);
    BOF3_INJECT(Area145_Trigger40);
    BOF3_INJECT(Area145_StepHook);
    BOF3_INJECT(Area145_CellHook);
    BOF3_INJECT(Area145_Tail20);
    BOF3_INJECT(Area145_TrailStart);
    BOF3_INJECT(Area145_TrailLeg0);
    BOF3_INJECT(Area145_TrailLeg1);
    BOF3_INJECT(Area145_TrailLeg2);
    BOF3_INJECT(Area145_SpawnTrail);
    BOF3_INJECT(Area145_DrawGradient);
    BOF3_INJECT(Area145_ClearTint);
    BOF3_INJECT(Area146_LeaderBit138OtSlot);
    BOF3_INJECT(Area146_StepHook);
    BOF3_INJECT(Area146_Trigger32);
    BOF3_INJECT(Area146_EffectB4Run);
    BOF3_INJECT(Area146_EffectB4Cylinder);
    BOF3_INJECT(Area146_DrawGlowCylinder);
    BOF3_INJECT(Area146_ClearFlag46);
    BOF3_INJECT(Area146_ToExtraObject0);
}
