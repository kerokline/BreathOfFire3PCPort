// Moved here by tools/move_funcs.py from effect_3d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_81.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_3d.h"
#include "game/effect_3d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

unsigned char& B(U a) { return *At(a); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

void SetBits(unsigned char* to, U bits) { std::memcpy(to, &bits, 4); }

// fld dword [a]; fsub dword [b]; fstp dword [to]
void FldSub(unsigned char* to, const unsigned char* a, const unsigned char* b) {
    __asm__ volatile("flds %1\n\tfsubs %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(to))
                     : "m"(*reinterpret_cast<const float*>(a)), "m"(*reinterpret_cast<const float*>(b)));
}

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, data or the next kind's states.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3d.md section 7)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

void NextState() { S()[1] = static_cast<unsigned char>(S()[1] + 1); }

}  // namespace

// original 0x487890 (Effect_KindHandlers[0x81], hidden in 0x4875C0): jmp
// [EffectKind81_States + +1 * 4].
extern "C" void __cdecl EffectKind81_Run(void) {
    Dispatch("EffectKind81_Run", AddressOf(EffectKind81_States), EffectKind81_States_count, 1);
}

// original 0x4878B0 (state 0): the record's point the leader's (ObjTrio +0x34,
// +0x38, +0x3C); the drops cleared, the sources placed; +1 up.
extern "C" void __cdecl EffectKind81_Start(void) {
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(ObjTrio[0].x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(ObjTrio[0].z));
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<std::uint32_t>(ObjTrio[0].height));
    SH_CALL(EffectKind81_ClearDrops)();
    SH_CALL(EffectKind81_PlaceSources)();
    NextState();
}

// original 0x4878F0 (state 1): +1 up when the counter 0x903848 is 12; the
// sources emit; a tail jump to EffectKind81_MoveDrops (its answer unread).
extern "C" void __cdecl EffectKind81_Pour(void) {
    if (B(::bof3::Img(at::kCounter.v)) == 0xC) NextState();
    SH_CALL(EffectKind81_Emit)();
    SH_CALL(EffectKind81_MoveDrops)();
}

// original 0x487910 (state 2): the drops moved; when none was in use, a tail
// jump to Effect_Release.
extern "C" void __cdecl EffectKind81_Drain(void) {
    if (SH_CALL(EffectKind81_MoveDrops)() != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x487920 (cdecl; also called by 0x492530, a catalog row): bytes 0..2
// of the 256 drop records at 0x92BF80 (0x18 each) cleared.
extern "C" void __cdecl EffectKind81_ClearDrops(void) {
    for (U i = 0; i < at::kDropCount; ++i) {
        unsigned char* const d = P(::bof3::Img(EffectKind30_Shards_addr) + at::kDropStride * i);
        d[0] = 0;
        d[1] = 0;
        d[2] = 0;
    }
}

// original 0x487940 (cdecl): the map camera; the moving count 0x67627C 0; each
// drop in use counted, its speed +0x14 0x20000 more and its height +0xC moved
// by the new speed, its blink bit +3 flipped, drawn, its life +2
// down - at 0 its +0 and +1 cleared. al 1 when any was in use, else 0.
extern "C" unsigned char __cdecl EffectKind81_MoveDrops(void) {
    unsigned char any = 0;
    Effect3d_DropsMoving = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < at::kDropCount; ++i) {
        unsigned char* const d = P(::bof3::Img(EffectKind30_Shards_addr) + at::kDropStride * i);
        if (d[0] == 0) continue;
        const U speed = UL(d + 0x14) + 0x20000u;
        const U height = UL(d + 0xC);
        Effect3d_DropsMoving = static_cast<std::uint32_t>(Effect3d_DropsMoving) + 1u;
        any = 1;
        const auto blink = static_cast<unsigned char>(d[3] ^ 1u);
        SetUL(d + 0x14, speed);
        SetUL(d + 0xC, height + speed);
        d[3] = blink;
        SH_CALL(EffectKind81_DrawDrop)(d);
        d[2] = static_cast<unsigned char>(d[2] - 1);
        if (d[2] == 0) {
            d[0] = 0;
            d[1] = 0;
        }
    }
    return any;
}

// original 0x4879C0 (cdecl): a drop (+4 x, z, height) drawn when its blink bit
// +3 is set: a black one-pixel tile at its projection (MapView_LinkPrimAt at
// its x, z, dy 0, 0x14), a draw mode (page Gpu_GetTPage(0, 2, 0x3C0, 0)) linked
// (0xC), and a 3 by 3 tile of 8 one up and left of the projection,
// semi-transparent (0x1C).
extern "C" void __cdecl EffectKind81_DrawDrop(unsigned char* drop) {
    if (drop[3] == 0) return;
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(drop + 4), reinterpret_cast<float*>(p + 8));
    p[6] = 0;
    p[5] = 0;
    p[4] = 0;
    SH_CALL(MapView_LinkPrimAt)(UL(drop + 4), UL(drop + 8), 0, 0x14);
    const U page = SH_CALL(Gpu_GetTPage)(0, 2, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(drop + 4), UL(drop + 8), 0, 0xC);
    unsigned char* const t = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(t);
    SH_CALL(Gpu_SetSemiTrans)(t, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(drop + 4), reinterpret_cast<float*>(t + 8));
    FldSub(t + 8, t + 8, At(at::kOne));
    SetBits(t + 0x14, 0x40400000u);   // 3.0f
    SetBits(t + 0x18, 0x40400000u);
    FldSub(t + 0xC, t + 0xC, At(at::kOne));
    t[6] = 8;
    t[5] = 8;
    t[4] = 8;
    SH_CALL(MapView_LinkPrimAt)(UL(drop + 4), UL(drop + 8), 0, 0x1C);
}

// original 0x487AB0 (cdecl): the sixteen sources at 0x92D780 (0x14 each): +0 1,
// +1 0, +2 (Rand & 0xF) - 0x10, +3 Rand & 0x1F (the first wait), +4 / +8 the
// cell 0x654C3C gives (s8 x, z << 16), +0xC the ground's height there
// (AreaMap_Elevation's low word << 16).
extern "C" void __cdecl EffectKind81_PlaceSources(void) {
    for (U i = 0; i < at::kSourceCount; ++i) {
        unsigned char* const src = P(::bof3::Img(at::kSources.v) + at::kSourceStride * i);
        src[0] = 1;
        src[1] = 0;
        src[2] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0xFu) - 0x10u);
        const U r = static_cast<U>(SH_CALL(Rand)());
        const U z = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kSourceCells.v) + 2 * i + 1)))) << 16;
        src[3] = static_cast<unsigned char>(r & 0x1Fu);
        const U x = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kSourceCells.v) + 2 * i)))) << 16;
        SetUL(src + 4, x);
        SetUL(src + 8, z);
        const long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        SetUL(src + 0xC, static_cast<U>(S16(static_cast<U>(ground))) << 16);
    }
}

// original 0x487B20 (cdecl): each source on (+0): its wait +3 down; when it was
// 0, two drops from free records (EffectKind81_FindFreeDrop; none, none) - +0 1, +2
// life 0x20 + (Rand & 0x1F), +4 / +8 the source's x / z less 0x10000 and plus
// Rand's low byte << 8, +0xC its height, +0x14 0, +3 Rand & 1 - and the wait
// Rand & 7. al 1 when any source was on, else 0.
extern "C" unsigned char __cdecl EffectKind81_Emit(void) {
    unsigned char any = 0;
    for (U i = 0; i < at::kSourceCount; ++i) {
        unsigned char* const src = P(::bof3::Img(at::kSources.v) + at::kSourceStride * i);
        if (src[0] == 0) continue;
        const unsigned wait = src[3];
        any = 1;
        src[3] = static_cast<unsigned char>(wait - 1);
        if (wait != 0) continue;
        for (int n = 2; n != 0; --n) {
            unsigned char* const d = SH_CALL(EffectKind81_FindFreeDrop)();
            if (d == nullptr) continue;
            d[0] = 1;
            d[2] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0x1Fu) + 0x20u);
            const U rx = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
            SetUL(d + 4, ((rx - 0x100u) << 8) + UL(src + 4));
            const U rz = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
            SetUL(d + 8, ((rz - 0x100u) << 8) + UL(src + 8));
            SetUL(d + 0xC, UL(src + 0xC));
            SetUL(d + 0x14, 0);
            d[3] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 1u);
        }
        src[3] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 7u);
    }
    return any;
}

// original 0x487BF0 (cdecl; also called by 0x492CF0, a catalog row): the first
// of the 256 drop records whose +0 is 0, else null (eax).
extern "C" unsigned char* __cdecl EffectKind81_FindFreeDrop(void) {
    for (U i = 0; i < at::kDropCount; ++i) {
        unsigned char* const d = P(::bof3::Img(EffectKind30_Shards_addr) + at::kDropStride * i);
        if (d[0] == 0) return d;
    }
    return nullptr;
}

void EffectKind81_Inject() {
    BOF3_INJECT(EffectKind81_Run);
    BOF3_INJECT(EffectKind81_Start);
    BOF3_INJECT(EffectKind81_Pour);
    BOF3_INJECT(EffectKind81_Drain);
    BOF3_INJECT(EffectKind81_ClearDrops);
    BOF3_INJECT(EffectKind81_MoveDrops);
    BOF3_INJECT(EffectKind81_DrawDrop);
    BOF3_INJECT(EffectKind81_PlaceSources);
    BOF3_INJECT(EffectKind81_Emit);
    BOF3_INJECT(EffectKind81_FindFreeDrop);
}
