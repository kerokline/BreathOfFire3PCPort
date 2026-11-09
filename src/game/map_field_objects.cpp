// Group DD of the eighth takeover round (docs/map_field_objects.md): sixteen
// functions the shop and world-map routes reach, each read to its last
// instruction with capstone against bof3/BOF3.exe on 2026-09-25, and every
// site that holds its address read for what the caller does with the call.
// Faithful: no divergence. The start-up fuzz is map_field_objects_fuzz.cpp.
//
//   MapCell_DrawUprights       0x570210..0x570520 (0x311)  MapCell_Handlers 1..3, 5..0xF, 0x31..0x33, 0x35..0x3E
//   MapCell_DrawDiagonalWall   0x570530..0x570654 (0x125)  MapCell_Handlers 4, 0x34
//   MapCell_DrawFlatFaces      0x570A00..0x570AA5 (0xA6)   MapCell_Handlers 0x11..0x17, 0x19..0x1F
//   MapCell_PatchThenStep      0x571090..0x5710C3 (0x34)   MapCell_Handlers 0x24
//   MapCell_PatchThenStop      0x5710D0..0x571103 (0x34)   MapCell_Handlers 0x25
//   MapCell_DrawAnimated       0x5712E0..0x5714F7 (0x218)  MapCell_Handlers 0x27 (PSX 0x80158464, gap4)
//   AreaMap_SetupTint          0x571880..0x5718EE (0x6F)   AreaMap_SetupHandlers 0 (PSX 0x80158C70, gap4)
//   AreaMap_SetupFlatColours   0x571A30..0x571A6E (0x3F)   AreaMap_SetupHandlers 1 (PSX 0x80158E78, gap4)
//   AreaMap_SetupTexture       0x571A70..0x571AE0 (0x71)   AreaMap_SetupHandlers 2 (PSX 0x80158EE4, gap4)
//   Menu_DrawMemberStatus      0x573560..0x573891 (0x332)  5 E8 callers (PSX 0x801D8270, call-anchored)
//   Menu_DrawCell8             0x5744B0..0x574525 (0x76)   8 E8 callers (PSX 0x801DA0D4, gap)
//   Menu_DrawPlayTime          0x5746C0..0x5747C7 (0x108)  1 E8 caller, 0x599DE1
//   EventScript_SkipSwitch     0x579CF0..0x579D3F (0x50) + its 11-entry jump table 0x579D40 (PSX 0x801A584C)
//   AreaMap_SetByte            0x579F00..0x579F26 (0x27)   ~420 E8 callers (PSX 0x801A5BC8)
//   EventOp_8x                 0x57A3A0..0x57A5D8 (0x239)  EventScript_Op's table (PSX 0x801A6408)
//   ScriptFlags_Clear40        0x57C7A0..0x57C7B5 (0x16)   ~390 E8 callers, two tail jmps (PSX 0x8015CA48)
//
// The nine map-layer handlers are called only through their tables, by
// DrawLayer_Open and AreaMap_SetupEntries (both ours, both calls typed void),
// and so return nothing. The menu draws hand their last callee's eax on, as
// the originals do (0x59BEE9 and 0x599C17 return it to their own callers).
#include "game/map_field_objects.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/map_field_objects_callees.h"
#include "game/map_rim.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace map_field_objects {

namespace {
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
}  // namespace

const Callees kOriginals = {
    As<long (__cdecl*)(long, long)>(AreaMap_Elevation),
    As<long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*)>(Gte_RotTransPers3),
    As<void (__cdecl*)(float*, float*, float*)>(Gte_StoreDepthF3),
    As<void (__cdecl*)(unsigned char*)>(Gpu_SetPolyFT4),
    As<void (__cdecl*)(unsigned char*, unsigned)>(Gpu_SetShadeTex),
    As<U (__cdecl*)(unsigned, unsigned)>(Gfx_CommitPrim),
    As<long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*, long*)>(
        Gte_RotTransPers4),
    As<void (__cdecl*)(float*, float*, float*, float*)>(Gte_StoreDepthF4),
    As<void (__cdecl*)(void*)>(Gte_PrimDepths4_10),
    As<void (__cdecl*)(unsigned long, unsigned char*, int)>(Prim_SetTexture),
    As<void (__cdecl*)(int, unsigned)>(MapCell_FlatOverlay),
    As<void (__cdecl*)(const unsigned char*)>(AreaMap_ApplyPatch),
    As<unsigned char (__cdecl*)(unsigned long)>(Area_TestCondition),
    As<long (__cdecl*)(const short*, unsigned long*, long*)>(Gte_RotTransPers),
    As<void (__cdecl*)(void*)>(Gte_PrimDepthFlat4_10),
    As<long (__cdecl*)(int, int, int, int, int)>(Gfx_ClutAdjust),
    As<void (__cdecl*)(int, int, int, int, int, int)>(Menu_DrawBox),
    As<void (__cdecl*)(int, int, int, int)>(Menu_DrawItemIcon),
    As<const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*)>(Text_DrawAt),
    As<int (__cdecl*)(char*, const char*, unsigned)>(Crt_sprintf),
    As<void (__cdecl*)(int, int, int, const unsigned char*)>(Text_DrawFont8),
    reinterpret_cast<const unsigned char* (__cdecl*)(int, int, unsigned, unsigned, const unsigned char*)>(
        static_cast<std::uintptr_t>(::bof3::Code(kTinyFont.v))),
    As<U (__cdecl*)(int, int, const unsigned char*, int)>(Menu_DrawPieces),
    As<U (__cdecl*)(int, int, unsigned, unsigned, unsigned)>(Menu_DrawExpBar),
    As<void (__cdecl*)(unsigned char*)>(Gpu_SetSprt8),
    As<void (__cdecl*)(unsigned char*, unsigned)>(Gpu_SetSemiTrans),
    As<void (__cdecl*)(int, int, int, const unsigned char*)>(Text_DrawFont12),
    As<const unsigned char* (__cdecl*)(const unsigned char*)>(EventScript_SkipControl),
    As<void (__cdecl*)()>(EventObj_Reset),
    As<U (__cdecl*)(U)>(Sprite_SetAnimationBank),
    As<void (__cdecl*)(const unsigned char*)>(EventObj_SetFlags),
    As<void (__cdecl*)(U)>(Sprite_SetAnimation),
    As<U (__cdecl*)(U, U, U)>(AreaMap_SetByte),
};
Callees g = kOriginals;

}  // namespace map_field_objects

namespace {

using namespace map_field_objects;

unsigned char* AtMapFieldObjects(map_field_objects::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
map_field_objects::U Byte(map_field_objects::U address) { return AtMapFieldObjects(address)[0]; }
map_field_objects::U WordMapFieldObjects(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
map_field_objects::U DwordMapFieldObjects(const unsigned char* p) {
    map_field_objects::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetWordMapFieldObjects(unsigned char* p, map_field_objects::U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void SetDwordMapFieldObjects(unsigned char* p, map_field_objects::U v) { std::memcpy(p, &v, sizeof v); }
void SetFloatMapFieldObjects(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }
map_field_objects::U S16(map_field_objects::U v) { return static_cast<map_field_objects::U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))); }

unsigned char* PacketNext() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(DwordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::PacketNextAt())))); }

// EventOp_8x's object, Sprite(), is a Sprite_Objects record.

}  // namespace

// --- the map-cell handlers (MapCell_Handlers, called by DrawLayer_Open) -----------------
// Each gets (record, b1, b0): the cell's record and the run head's bytes 1 and
// 0, the cell's map x and y (DrawLayer_Open).

// original 0x571090 / 0x5710D0: kinds 0x24 and 0x25 - the record's kind byte
// becomes 0x25 (then 0x23, a bare ret) before the patch list entry at dword
// (record & 0xFFFF) + AreaMap_PatchBase of the area block goes to
// AreaMap_ApplyPatch. So an 0x24 record applies its patch on two draws of the
// cell, and then never again. The original pops the argument slot
// AreaMap_ApplyPatch overwrote into ecx; DrawLayer_Open reloads ecx.
// --- the area set-up list's handlers (AreaMap_SetupHandlers, AreaMap_SetupEntries) --------

// --- the menu draws -------------------------------------------------------------------------

// --- the event script ------------------------------------------------------------------------

// original 0x57C7A0 (PSX 0x8015CA48): Field_StatusBits &= ~0x40 and
// Field_ScriptFlags &= ~0x100 (the blocking flags, docs/menu-screens.md). The
// byte is read first and stored last. eax on the way out is the caller's own
// with the new byte in al - two callers tail-jump here (0x417D49, 0x543A5D), so
// the entry passes eax to the body and the body returns it so.
extern "C" unsigned long __cdecl MapFieldObjects_Clear40Body(unsigned long eax) {
    const U bits = Byte(at::StatusBitsAt());
    SetWordMapFieldObjects(AtMapFieldObjects(at::ScriptFlagsAt()), WordMapFieldObjects(AtMapFieldObjects(at::ScriptFlagsAt())) & 0xFEFFu);
    const auto cleared = static_cast<unsigned char>(bits & 0xBFu);
    AtMapFieldObjects(at::StatusBitsAt())[0] = cleared;
    return (static_cast<U>(eax) & 0xFFFFFF00u) | cleared;
}

void MapFieldObjects_Inject() {
    if (bof3::WantsShadow("map_field_objects")) map_field_objects::SelfTest();
}
