// Group S23 of the spell round (docs/magic_s23.md): four overlays of
// Magic_Rows compiled into the exe at 0x4CC970..0x4D04A2, and the child
// effects compiled with them.
//
//   MAGIC100, row 13 (Cyclone, read one id down): the task, its start and
//     its wait; the kind-1 effect 0x0A (FxFunnel_*) - eight funnels that
//     close on the target and rise, each drawn as a band of G4 quads with a
//     ground ring. The funnel's code is reached from MAGIC096..100 too
//     (tools/magic_rows.py): taking it takes it for them.
//   MAGIC101, row 26 (Typhoon): the task, its start, grow and fade, and a fan
//     of G3s; the kind-1 effect 0x1B (FxSpiral_*), sixteen spiral bands,
//     reached from MAGIC096..101.
//   MAGIC102, row 69 (Quake): the map heaved - its cells' polygon codes
//     switched, the corner heights of a 16 x 14 block lifted and dropped by a
//     sine, every actor and object kept on the ground, the camera shaken - and
//     put back.
//   MAGIC103, row 57 (Simoon): three kinds of the kind-1 effect 0x2D (a dome
//     of GT4s, a dust sprite, a fan of G3s) and a CLUT made semi-transparent.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase / a .data
// cell read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. No divergence: each function is a faithful
// replacement, except that a phase past one of the four stack tables aborts
// (the project's precedent, docs/magic_fx_reached.md section 3), and that
// Quake_Heave aborts on a divisor of 0 where the original takes a divide
// fault (a facing byte of 4 or more; docs/magic_s23.md section 5).
#include "game/magic_s23.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s23_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_s23::at;
namespace hat = magic_harness::at;
using magic_harness::EnemyOf;
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::PartyOf;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(hat::kOwner); }
void Bump(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }
short Get(std::uint32_t address) { return static_cast<short>(Word(Mem(address))); }
void Put(std::uint32_t address, int v) { SetWord(Mem(address), static_cast<unsigned>(v)); }
std::uint8_t Byte(std::uint32_t address) { return Mem(address)[0]; }
// 32-bit imul and shl, wrapping as the original's do.
std::int32_t Imul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
std::int32_t Shl(std::int32_t a, int n) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << n); }
void SetPointerAt(unsigned char* cell, const void* p) {
    SetLong(cell, static_cast<std::int32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p))));
}
unsigned char* Slot(unsigned index) { return Mem(hat::kTasks + index * hat::kTaskStride); }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }
// sin / cos of the angle times the s16 at `radius` (read after the call), >> 12.
int SinOf(int angle, std::uint32_t radius) { const int s = Sin(angle); return Imul(s, Get(radius)) >> 12; }
int CosOf(int angle, std::uint32_t radius) { const int c = Cos(angle); return Imul(c, Get(radius)) >> 12; }

const short* Vertex(std::uint32_t address) { return reinterpret_cast<const short*>(Mem(address)); }
float* At(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }
// The four vertices at Prim_VertexScratch projected into the primitive's
// screen points; the depth answer and the flag word are the caller's locals.
void Project4(unsigned char* prim, unsigned a, unsigned b, unsigned c, unsigned d) {
    long depth = 0;
    MH_CALL(Gte_RotTransPers4)(Vertex(at::kV0), Vertex(at::kV1), Vertex(at::kV2), Vertex(at::kV3), At(prim, a), At(prim, b),
                               At(prim, c), At(prim, d), &depth);
}
void Project3(unsigned char* prim) {
    long depth = 0;
    MH_CALL(Gte_RotTransPers3)(Vertex(at::kV0), Vertex(at::kV1), Vertex(at::kV2), At(prim, 8), At(prim, 0x18), At(prim, 0x28),
                               &depth);
}
// Three bytes of a vertex's colour, from the low byte of a scratch word.
void Shade(unsigned char* prim, unsigned offset, std::uint32_t word) {
    prim[offset] = Byte(word);
    prim[offset + 1] = Byte(word);
    prim[offset + 2] = Byte(word);
}
void Shade(unsigned char* prim, unsigned offset, unsigned char v) {
    prim[offset] = v;
    prim[offset + 1] = v;
    prim[offset + 2] = v;
}

// The actor's matrix for an effect that carries its own: the SVECTOR
// `vector` through Gte_RotTrans into a MATRIX's translation, the rotation of
// `angles` into it, composed with the camera's, and loaded - as
// MagicFx_PushActorMatrix does after its own Gte_PushMatrix.
void LoadMatrix(const short* angles, const short* vector) {
    alignas(4) unsigned char m[0x20];
    MH_CALL(Gte_RotTrans)(vector, reinterpret_cast<long*>(m + 0x14));
    MH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(m));
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<const short*>(m), reinterpret_cast<short*>(m));
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(m));
}
// The translation the three matrices share: the task's position in map units
// (+0x34 / +0x38 sar 9, less 0x4000) and minus half its height.
void Position(const unsigned char* sc, int height, short* v) {
    v[0] = static_cast<short>((Long(sc + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(sc + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(height / 2));
}

// A phase through a stack table the original builds, by +1: aborts past it.
void Dispatch(const char* who, const std::uint32_t* table, unsigned n) {
    const unsigned phase = Sc()[1];
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(table[phase])();
}
// A .data table of handlers, read in place, the index unchecked.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Four task slots of kind 1 made for Sprite_Current: the slot's owner +0x80,
// then the fields the caller sets (the original checks no 0xFF).
unsigned char* Child(unsigned parameter, unsigned char*& sc) {
    const unsigned slot = MH_CALL(BattleTask_Create)(1, parameter);
    sc = Sc();
    unsigned char* const t = Slot(slot);
    SetPointerAt(t + 0x80, sc);
    return t;
}

// The target's record: a party member below 3, else an enemy (unchecked).
unsigned char* TargetRecord() {
    const unsigned char target = Mem(hat::kTarget)[0];
    return target < 3 ? PartyOf(target) : EnemyOf(target);
}

// The acting side's facing: the actor sprite's +8, turned by 2 when the
// target's bit 0x40 and the actor's side disagree (bit 0x40 and a party
// actor, or neither and an enemy).
unsigned char Facing() {
    const bool bit = (Mem(hat::kTarget)[0] & 0x40) != 0;
    const bool party = Mem(hat::kActor)[0] < 3;
    const unsigned char f = Pointer(at::kActorSprite)[8];
    return bit == party ? static_cast<unsigned char>(f ^ 2) : f;
}

// Quake's walk over MapView_Cells: every non-zero cell names a run of
// AreaMap_Header dwords (the cell plus AreaMap_CellBase's low word; the
// dword before it holds the run's length in its high word), stepped by each
// dword's byte +2; `map` switches the top byte (the polygon code) of each.
// A run whose steps do not land on its end is not checked, as the original.
void Walk(void (*map)(unsigned char*)) {
    for (std::uint32_t cell = at::kMapCells; cell < at::kMapCellsEnd; cell += 2) {
        const unsigned c = Word(Mem(cell));
        if (c == 0) continue;
        const std::uint32_t index = c + (static_cast<std::uint32_t>(Long(Mem(at::kAreaCellBase))) & 0xFFFF);
        const std::uint32_t n = static_cast<std::uint32_t>(Long(Mem(at::kArea - 4 + index * 4))) >> 16;
        std::uint32_t p = at::kArea + index * 4;
        const std::uint32_t end = p + n * 4 - 4;
        while (p != end) {
            map(Mem(p));
            p += 4u * Mem(p)[2];
        }
    }
}
void Recode(unsigned char* e, std::uint32_t from, std::uint32_t to, bool keep_low) {
    const std::uint32_t v = static_cast<std::uint32_t>(Long(e));
    if ((v & 0xFF000000u) != from) return;
    SetLong(e, static_cast<std::int32_t>(keep_low ? (v & 0xFFFFFFu) | to : v & 0xFFFFFFu));
}
// At the start: code 0 becomes 0x28, 0x21 0x29, 0x22 0x2A, 0x27 0x2B.
void Semi(unsigned char* e) {
    Recode(e, 0, 0x28000000u, true);
    Recode(e, 0x21000000u, 0x29000000u, true);
    Recode(e, 0x22000000u, 0x2A000000u, true);
    Recode(e, 0x27000000u, 0x2B000000u, true);
}
// At the end: 0x28 becomes 0, 0x29 0x21, 0x2A 0x22, 0x2B 0x27.
void Opaque(unsigned char* e) {
    Recode(e, 0x28000000u, 0, false);
    Recode(e, 0x29000000u, 0x21000000u, true);
    Recode(e, 0x2A000000u, 0x22000000u, true);
    Recode(e, 0x2B000000u, 0x27000000u, true);
}

long Ground(const unsigned char* at_position) {
    return MH_CALL(AreaMap_Elevation)(Long(at_position + 0x34), Long(at_position + 0x38));
}

// Quake_Heave's idiv by a Quake_Divisors byte: the original faults on 0.
int Divide(int n, unsigned d) {
    if (d == 0) bof3::Fatal("Quake_Heave: a divisor of 0 (facing byte %u reads past Quake_Divisors)", Byte(at::kQuakeFacing));
    return n / static_cast<int>(d);
}
void AddByte(std::uint32_t address, int v) { Mem(address)[0] = static_cast<unsigned char>(Mem(address)[0] + v); }

}  // namespace

#define S23_EXPORT extern "C" __attribute__((disable_tail_calls))

// ============================================================================
// MAGIC100, row 13 (Cyclone)
// ============================================================================

// original 0x4CC970: the kind-2 task (Magic_Rows row 13). Its phase +1
// through a three-entry stack table: Cyclone_Start, Cyclone_Wait,
// BattleFx_Finish. Unchecked in the original; ours aborts past it.
S23_EXPORT void __cdecl Cyclone_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Cyclone_Start, bof3::addr::Cyclone_Wait,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch("Cyclone_Task", kPhases, 3);
}

// original 0x4CC9A0: the owner's actor byte and x / z, the source's +0x3C;
// +9 and +0xB 0; on; eight funnels (kind 1, 0x0A) owned by the task, each
// +0xB its index and +9 its delay 2i + 1, counted in the task's +0xB; sound
// 0x100.
S23_EXPORT void __cdecl Cyclone_Start(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Pointer(hat::kSource) + 0x3C));
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    Bump(Sc()[1]);
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* sc;
        unsigned char* const t = Child(0xA, sc);
        t[1] = 0;
        t[0xB] = static_cast<unsigned char>(i);
        t[9] = static_cast<unsigned char>(2 * i + 1);
        Bump(sc[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4CCA70: once a funnel has counted +9 up, the target's flags
// 0x10, on.
S23_EXPORT void __cdecl Cyclone_Wait(void) {
    if (Sc()[9] == 0) return;
    MH_CALL(Battle_SetTargetFlags)(Mem(hat::kTarget)[0], 0x10);
    Bump(Sc()[1]);
}

// original 0x4CCAA0: the funnel (kind 1, 0x0A): jmp [FxFunnel_Types + 4 * +1],
// read in place, unchecked.
S23_EXPORT void __cdecl FxFunnel_Dispatch(void) { Entry(at::kFunnelTypes, Sc()[1])(); }

// original 0x4CCAC0: FxFunnel_Types[0]. Its phase +2 through FxFunnel_Phases
// (read in place, unchecked); then, alive and past phase 0, the funnel under
// its own matrix and the ground ring under a second.
S23_EXPORT void __cdecl FxFunnel_Task(void) {
    Entry(at::kFunnelPhases, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(FxFunnel_PushMatrix)();
    MH_CALL(FxFunnel_Draw)();
    MH_CALL(Gte_PopMatrix)();
    MH_CALL(FxFunnel_PushGroundMatrix)();
    MH_CALL(FxFunnel_DrawGround)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CCB10: FxFunnel_Phases[0]. +9 down; at 0: the owner's actor
// byte; the offset (0x60000, 0) turned by 0x446770 by the facing and added to
// the owner's x / z - then the owner's whole position copied over it (the
// offset is lost, as in the original); the heading from the owner by
// Math_Ratan2, less 0x800; the drift +0x18 = -4 - (Rand & 3) for an odd
// +0xB, else 4 + (Rand & 3); the rise +0x1C = 0x50 + (Rand & 0x30); +0xA 0;
// on.
S23_EXPORT void __cdecl FxFunnel_Wait(void) {
    Bump(Sc()[9], -1);
    unsigned char* const sc = Sc();
    if (sc[9] != 0) return;
    sc[8] = Owner()[8];
    SetLong(Sc() + 0xC, 0x60000);
    SetLong(Sc() + 0x10, 0);
    MH_AT(magic_s23::TurnFn, magic_s23::kTurnByFacing)(Sc());
    {
        const unsigned char* const owner = Owner();
        unsigned char* const s = Sc();
        SetLong(s + 0x34, Long(owner + 0x34) + Long(s + 0xC));
    }
    {
        const unsigned char* const owner = Owner();
        unsigned char* const s = Sc();
        SetLong(s + 0x38, Long(owner + 0x38) + Long(s + 0x10));
    }
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    {
        const unsigned char* const owner = Owner();
        const unsigned char* const s = Sc();
        const std::int32_t dz = Long(owner + 0x38) - Long(s + 0x38);
        const std::int32_t dx = Long(owner + 0x34) - Long(s + 0x34);
        const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dz));
        SetLong(Sc() + 0x20, (heading - 0x800) & 0xFFF);
    }
    if (Sc()[0xB] & 1) {
        const int r = MH_CALL(Rand)();
        SetLong(Sc() + 0x18, -4 - (r & 3));
    } else {
        const int r = MH_CALL(Rand)();
        SetLong(Sc() + 0x18, (r & 3) + 4);
    }
    const int r = MH_CALL(Rand)();
    SetLong(Sc() + 0x1C, (r & 0x30) + 0x50);
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// The funnel's spin: +0x20 up 0x200 for an odd +0xB, down for an even, in
// 0..0xFFF.
void Spin(unsigned char* sc) {
    const std::int32_t a = Long(sc + 0x20);
    SetLong(sc + 0x20, ((sc[0xB] & 1) ? a + 0x200 : a - 0x200) & 0xFFF);
}

// original 0x4CCC90: FxFunnel_Phases[1]. Spin; 0x4FBB40 moves the funnel
// round the target's record by the drift (+0x18 << 6) at radius 0xC000 and
// answers the angle, kept in +0x14; +0xA up 4, at 0x10 on.
S23_EXPORT void __cdecl FxFunnel_Approach(void) {
    unsigned char* const record = TargetRecord();
    Spin(Sc());
    const int angle = MH_AT(magic_s23::OrbitFn, magic_s23::kOrbitRecord)(record, 0xC000, Shl(Long(Sc() + 0x18), 6));
    SetLong(Sc() + 0x14, angle);
    Bump(Sc()[0xA], 4);
    if (Sc()[0xA] == 0x10) Bump(Sc()[2]);
}

// original 0x4CCD30: FxFunnel_Phases[2]. Spin; the last angle to +0x10, the
// orbit as above to +0x14; when 0x4FBC30 says the funnel is within 0xC000 of
// the target, or the angle moved by more than 0x400 and less than 0xC00
// (|last & 0xFFF - new & 0xFFF|, kept in +0x10): the owner's +9 up and on.
S23_EXPORT void __cdecl FxFunnel_Orbit(void) {
    unsigned char* const record = TargetRecord();
    Spin(Sc());
    {
        unsigned char* const sc = Sc();
        SetLong(sc + 0x10, Long(sc + 0x14));
    }
    const int angle = MH_AT(magic_s23::OrbitFn, magic_s23::kOrbitRecord)(record, 0xC000, Shl(Long(Sc() + 0x18), 6));
    SetLong(Sc() + 0x14, angle);
    if (MH_AT(magic_s23::NearFn, magic_s23::kNearRecord)(record, 0xC000) == 0) {
        {
            unsigned char* const sc = Sc();
            SetLong(sc + 0x10, (Long(sc + 0x10) & 0xFFF) - (Long(sc + 0x14) & 0xFFF));
        }
        unsigned char* sc = Sc();
        const std::int32_t d = Long(sc + 0x10);
        if (d < 0) {
            SetLong(sc + 0x10, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(d)));
            sc = Sc();
        }
        const std::int32_t moved = Long(sc + 0x10);
        if (moved <= 0x400 || moved >= 0xC00) return;
    }
    Bump(Owner()[9]);
    Bump(Sc()[2]);
}

// original 0x4CCE40: FxFunnel_Phases[3]. The offset (0x1000, 0) turned by
// the facing (0x446770) and added to x / z; the height +0x3E up 0x40; spin;
// +0xA down, at 0 the owner's count +0xB down and the task freed.
S23_EXPORT void __cdecl FxFunnel_Rise(void) {
    SetLong(Sc() + 0xC, 0x1000);
    SetLong(Sc() + 0x10, 0);
    MH_AT(magic_s23::TurnFn, magic_s23::kTurnByFacing)(Sc());
    {
        unsigned char* const sc = Sc();
        SetLong(sc + 0x34, Long(sc + 0x34) + Long(sc + 0xC));
    }
    {
        unsigned char* const sc = Sc();
        SetLong(sc + 0x38, Long(sc + 0x38) + Long(sc + 0x10));
    }
    SetWord(Sc() + 0x3E, Word(Sc() + 0x3E) + 0x40u);
    Spin(Sc());
    Bump(Sc()[0xA], -1);
    if (Sc()[0xA] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CCEF0: the funnel's matrix: Gte_PushMatrix, then no rotation
// and the translation at the task's position, 0x100 below half its height.
S23_EXPORT void __cdecl FxFunnel_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[3] = {0, 0, 0};
    short v[3];
    Position(Sc(), static_cast<short>(Word(Sc() + 0x3E)) + 0x100, v);
    LoadMatrix(angles, v);
}

// original 0x4CCFA0: the funnel. A draw-mode primitive (0x35) linked at the
// task's position (row offset 2); the radius from +0x1C (inner) and inner +
// sin(0) * inner / 2 (outer, the same); +0xA * 12 and +0xA the two colours;
// the angle index +0x20 / 64; then 31 G4 quads round the circle, each from
// the last pair of points to the next - the outer radius swelling by
// sin(0x40 i) * inner / 2 - semi-transparent, projected, linked at the
// position with 0x44 bytes.
S23_EXPORT void __cdecl FxFunnel_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    {
        const unsigned char* const sc = Sc();
        MH_CALL(MapView_LinkPrimAt)(Long(sc + 0x34), Long(sc + 0x38), 2, 0xC);
    }
    {
        const unsigned char* const r = Sc() + 0x1C;
        const int s = Sin(0);
        Put(at::kR, (Imul(s, Long(r) / 2) >> 12) + Word(r));
        Put(at::kR2, Word(r));
    }
    {
        const unsigned char* const sc = Sc();
        Put(at::kW8, (Long(sc + 0x20) / 64) & 0x3F);
        Put(at::kW4, sc[0xA] * 12);
        Put(at::kW6, sc[0xA]);
        const int first = (Byte(at::kW8) & 0x3F) << 6;
        Put(at::kWA, first);
        Put(at::kV1, SinOf(first, at::kR));
    }
    Put(at::kV1 + 2, CosOf(Get(at::kWA), at::kR));
    Put(at::kV3, SinOf(Get(at::kWA), at::kR2));
    Put(at::kV3 + 2, CosOf(Get(at::kWA), at::kR2));
    for (int i = 1, angle = 0x40; angle < 0x800; ++i, angle += 0x40) {
        const int a = ((Byte(at::kW8) + i) & 0x3F) << 6;
        const unsigned char* const r = Sc() + 0x1C;
        Put(at::kWA, a);
        const int s = Sin(angle);
        const int outer = (Imul(s, Long(r) / 2) >> 12) + Word(r);
        Put(at::kV0, Get(at::kV1));
        Put(at::kV0 + 2, Get(at::kV1 + 2));
        Put(at::kR, outer);
        Put(at::kV0 + 4, 0);
        Put(at::kV1, SinOf(Get(at::kWA), at::kR));
        Put(at::kV1 + 2, CosOf(Get(at::kWA), at::kR));
        Put(at::kV1 + 4, 0);
        Put(at::kV2, Get(at::kV3));
        Put(at::kV2 + 2, Get(at::kV3 + 2));
        Put(at::kV2 + 4, 0);
        Put(at::kV3, SinOf(Get(at::kWA), at::kR2));
        Put(at::kV3 + 2, CosOf(Get(at::kWA), at::kR2));
        unsigned char* const prim = Gfx_PacketNext;
        Put(at::kV3 + 4, 0);
        MH_CALL(Gpu_SetPolyG4)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Shade(prim, 4, at::kW4);
        Shade(prim, 0x14, at::kW4);
        Shade(prim, 0x24, at::kW6);
        Shade(prim, 0x34, at::kW6);
        Project4(prim, 8, 0x18, 0x28, 0x38);
        MH_CALL(Gte_PrimDepths4_10B)(prim);
        const unsigned char* const sc = Sc();
        MH_CALL(MapView_LinkPrimAt)(Long(sc + 0x34), Long(sc + 0x38), 2, 0x44);
    }
}

// original 0x4CD2D0: the ground ring's matrix: Gte_PushMatrix, no rotation,
// the translation at the task's x / z and half its height - from phase 3 on,
// half the ground's (AreaMap_Elevation) instead.
S23_EXPORT void __cdecl FxFunnel_PushGroundMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short angles[3] = {0, 0, 0};
    const unsigned char* const sc = Sc();
    const int height = sc[2] < 3 ? static_cast<short>(Word(sc + 0x3E)) : static_cast<short>(Ground(sc));
    short v[3];
    Position(sc, height, v);
    LoadMatrix(angles, v);
}

// original 0x4CD390: the ground ring. A draw-mode primitive (0x55)
// committed to slot 5; inner radius +0x1C - 0x20, outer inner + sin(0) *
// inner / 2; colour +0xA * 2; 31 G4 quads as the funnel's, the outer radius
// swelling by sin(0x40 i) * inner / 2, the inner pair black-ish (1),
// committed with 0x44 bytes; a closing draw mode (0x15).
S23_EXPORT void __cdecl FxFunnel_DrawGround(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    Put(at::kR2, Word(Sc() + 0x1C) - 0x20);
    {
        const int s = Sin(0);
        const short inner = Get(at::kR2);
        Put(at::kR, (Imul(s, inner / 2) >> 12) + inner);
    }
    {
        const unsigned char* const sc = Sc();
        Put(at::kW8, (Long(sc + 0x20) / 64) & 0x3F);
        Put(at::kW4, sc[0xA] * 2);
        const int first = (Byte(at::kW8) & 0x3F) << 6;
        Put(at::kWA, first);
        Put(at::kV1, SinOf(first, at::kR));
    }
    Put(at::kV1 + 2, CosOf(Get(at::kWA), at::kR));
    Put(at::kV3, SinOf(Get(at::kWA), at::kR2));
    Put(at::kV3 + 2, CosOf(Get(at::kWA), at::kR2));
    for (int i = 1, angle = 0x40; angle < 0x800; ++i, angle += 0x40) {
        Put(at::kWA, ((Byte(at::kW8) + i) & 0x3F) << 6);
        const int s = Sin(angle);
        const short inner = Get(at::kR2);
        Put(at::kV0 + 4, 0);
        const int outer = (Imul(s, inner / 2) >> 12) + inner;
        Put(at::kV0, Get(at::kV1));
        Put(at::kV0 + 2, Get(at::kV1 + 2));
        Put(at::kR, outer);
        Put(at::kV1, SinOf(Get(at::kWA), at::kR));
        Put(at::kV1 + 2, CosOf(Get(at::kWA), at::kR));
        Put(at::kV1 + 4, 0);
        Put(at::kV2, Get(at::kV3));
        Put(at::kV2 + 2, Get(at::kV3 + 2));
        Put(at::kV2 + 4, 0);
        Put(at::kV3, SinOf(Get(at::kWA), at::kR2));
        Put(at::kV3 + 2, CosOf(Get(at::kWA), at::kR2));
        unsigned char* const prim = Gfx_PacketNext;
        Put(at::kV3 + 4, 0);
        MH_CALL(Gpu_SetPolyG4)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        Shade(prim, 4, at::kW4);
        Shade(prim, 0x14, at::kW4);
        Shade(prim, 0x24, static_cast<unsigned char>(1));
        Shade(prim, 0x34, static_cast<unsigned char>(1));
        Project4(prim, 8, 0x18, 0x28, 0x38);
        MH_CALL(Gte_PrimDepths4_10B)(prim);
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// ============================================================================
// MAGIC101, row 26 (Typhoon)
// ============================================================================

// original 0x4CD6B0: the kind-2 task (Magic_Rows row 26). Its phase +1
// through a five-entry stack table - Typhoon_Start, Typhoon_Grow, 0x4DA3B0
// (MAGIC118's, not ours), Typhoon_Fade, BattleFx_Finish - unchecked in the
// original, ours aborts past it; then, while +0xA and +0 are not 0, the fan
// under the actor's matrix.
S23_EXPORT void __cdecl Typhoon_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Typhoon_Start, bof3::addr::Typhoon_Grow,
                                                 magic_s23::kTyphoonPhase2, bof3::addr::Typhoon_Fade,
                                                 bof3::addr::BattleFx_Finish};
    Dispatch("Typhoon_Task", kPhases, 5);
    const unsigned char* const sc = Sc();
    if (sc[0xA] == 0 || sc[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(Typhoon_DrawFan)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CD710: the facing to +8; the side's centre (0x4FC0E0); +0xB 0,
// +9 0x30, +0xA 0; on; sixteen spirals (kind 1, 0x1B) owned by the task,
// each +4 its index and +9 its delay (i / 4) * 12 + 1, counted in +0xB; sound
// 0x100.
S23_EXPORT void __cdecl Typhoon_Start(void) {
    const unsigned char facing = Facing();
    Sc()[8] = facing;
    MH_AT(magic_s23::VoidFn, magic_s23::kSideCentre)();
    Sc()[0xB] = 0;
    Sc()[9] = 0x30;
    Sc()[0xA] = 0;
    Bump(Sc()[1]);
    for (unsigned i = 0; i < 16; ++i) {
        unsigned char* sc;
        unsigned char* const t = Child(0x1B, sc);
        t[1] = 0;
        t[4] = static_cast<unsigned char>(i);
        t[9] = static_cast<unsigned char>((i >> 2) * 12 + 1);
        Bump(sc[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4CD7E0: +0xA up; at 0x10 the target's flags 0x10, on.
S23_EXPORT void __cdecl Typhoon_Grow(void) {
    Bump(Sc()[0xA]);
    if (Sc()[0xA] != 0x10) return;
    MH_CALL(Battle_SetTargetFlags)(Mem(hat::kTarget)[0], 0x10);
    Bump(Sc()[1]);
}

// original 0x4CD820: +0xA down; at 0 on.
S23_EXPORT void __cdecl Typhoon_Fade(void) {
    Bump(Sc()[0xA], -1);
    if (Sc()[0xA] == 0) Bump(Sc()[1]);
}

// original 0x4CD840: the fan. A draw-mode primitive (0x55) committed to slot
// 5; the radius 0x80 + sin((Frame_Counter & 0x1F) << 7) / 256; colour
// +0xA * 6; eight G3 triangles from the centre, 0x200 apart, the outer
// corners colour 1, committed with 0x34 bytes; a closing draw mode (0x15).
S23_EXPORT void __cdecl Typhoon_DrawFan(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const int s = Sin(static_cast<int>((Frame_Counter & 0x1F) << 7));
        const unsigned char* const sc = Sc();
        Put(at::kR, (Shl(s, 4) >> 12) + 0x80);
        Put(at::kW8, sc[0xA] * 6);
    }
    Put(at::kV2, SinOf(0, at::kR));
    Put(at::kV2 + 2, CosOf(0, at::kR));
    for (int angle = 0x200; angle < 0x1200; angle += 0x200) {
        unsigned char* const prim = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        const short x = Get(at::kV2), y = Get(at::kV2 + 2);
        Put(at::kV0, 0);
        Put(at::kV0 + 2, 0);
        Put(at::kV1, x);
        Put(at::kV1 + 2, y);
        Put(at::kV2, SinOf(angle, at::kR));
        Put(at::kV2 + 2, CosOf(angle, at::kR));
        Put(at::kV2 + 4, 0);
        Put(at::kV1 + 4, 0);
        Put(at::kV0 + 4, 0);
        Project3(prim);
        MH_CALL(Gte_PrimDepths3_10B)(prim);
        Shade(prim, 4, at::kW8);
        Shade(prim, 0x14, static_cast<unsigned char>(1));
        Shade(prim, 0x24, static_cast<unsigned char>(1));
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4CDA00: the spiral (kind 1, 0x1B): jmp [FxSpiral_Types + 4 * +1],
// read in place, unchecked.
S23_EXPORT void __cdecl FxSpiral_Dispatch(void) { Entry(at::kSpiralTypes, Sc()[1])(); }

// original 0x4CDA20: FxSpiral_Types[0]. Its phase +2 through FxSpiral_Phases
// (read in place, unchecked); then, alive and past phase 0, the band under
// its own matrix.
S23_EXPORT void __cdecl FxSpiral_Task(void) {
    Entry(at::kSpiralPhases, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(FxSpiral_PushMatrix)();
    MH_CALL(FxSpiral_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CDA60: FxSpiral_Phases[0]. +9 down; at 0: the owner's actor
// byte and position, 0x180 map units above it (+0x3C + 0x1800000); the
// angle index +0xB = (+4 << 2) & 0x3F; the tilt +0xC = FxSpiral_Turns[+4]
// << 8 (+4 unbounded); the radius +0x10 = 0x60, its scale +0x14 = 0x20; +9
// = (+4 << 2) + 1; +0xA 0; on.
S23_EXPORT void __cdecl FxSpiral_Wait(void) {
    Bump(Sc()[9], -1);
    unsigned char* const sc = Sc();
    if (sc[9] != 0) return;
    sc[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C) + 0x1800000);
    {
        unsigned char* const s = Sc();
        s[0xB] = static_cast<unsigned char>((s[4] << 2) & 0x3F);
    }
    {
        unsigned char* const s = Sc();
        SetLong(s + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Word(Mem(at::kSpiralTurns + 2u * s[4]))) << 8));
    }
    SetLong(Sc() + 0x10, 0x60);
    SetLong(Sc() + 0x14, 0x20);
    {
        unsigned char* const s = Sc();
        s[9] = static_cast<unsigned char>((s[4] << 2) + 1);
    }
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// The spiral's first two phases' common part: +0xB up 8, the height the
// owner's + 0x180 + sin((Frame_Counter & 0x1F) << 7) / 128.
void Bob() {
    Bump(Sc()[0xB], 8);
    const int s = Sin(static_cast<int>((Frame_Counter & 0x1F) << 7));
    const unsigned char* const owner = Owner();
    unsigned char* const sc = Sc();
    SetWord(sc + 0x3E, static_cast<unsigned>((Shl(s, 5) >> 12) + Word(owner + 0x3E) + 0x180));
}

// original 0x4CDB30: FxSpiral_Phases[1]. Bob; +0xA up 2, at 0x10 on.
S23_EXPORT void __cdecl FxSpiral_Grow(void) {
    Bob();
    Bump(Sc()[0xA], 2);
    if (Sc()[0xA] == 0x10) Bump(Sc()[2]);
}

// original 0x4CDB90: FxSpiral_Phases[2]. Bob; +9 down; at 0: sound 0x101 for
// spiral 0 only; +0xB = FxSpiral_Tilts[2 * +8] (the facing, unbounded); the
// radius and scale 0x80, the spread +0x1C 0x18; +9 0x10; on.
S23_EXPORT void __cdecl FxSpiral_Hold(void) {
    Bob();
    Bump(Sc()[9], -1);
    unsigned char* sc = Sc();
    if (sc[9] != 0) return;
    if (sc[4] == 0) {
        MH_CALL(Sound_PlayById)(0x101);
        sc = Sc();
    }
    sc[0xB] = Byte(at::kSpiralTilts + 2u * sc[8]);
    SetLong(Sc() + 0x10, 0x80);
    SetLong(Sc() + 0x14, 0x80);
    SetLong(Sc() + 0x1C, 0x18);
    Sc()[9] = 0x10;
    Bump(Sc()[2]);
}

// original 0x4CDC50: FxSpiral_Phases[3]. +0xB up 4, the radius up by the
// spread; +9 down, at 0 on.
S23_EXPORT void __cdecl FxSpiral_Spread(void) {
    Bump(Sc()[0xB], 4);
    {
        unsigned char* const sc = Sc();
        SetLong(sc + 0x10, Long(sc + 0x10) + Long(sc + 0x1C));
    }
    Bump(Sc()[9], -1);
    if (Sc()[9] == 0) Bump(Sc()[2]);
}

// original 0x4CDC90: FxSpiral_Phases[4]. +0xB up by +0xA / 2; the spread
// down 2 and the radius up by it; +0xA down, at 0 the owner's count +0xB down
// and the task freed.
S23_EXPORT void __cdecl FxSpiral_Fade(void) {
    {
        unsigned char* const sc = Sc();
        sc[0xB] = static_cast<unsigned char>(sc[0xB] + (sc[0xA] >> 1));
    }
    {
        unsigned char* const sc = Sc();
        SetLong(sc + 0x1C, Long(sc + 0x1C) - 2);
    }
    {
        unsigned char* const sc = Sc();
        SetLong(sc + 0x10, Long(sc + 0x10) + Long(sc + 0x1C));
    }
    Bump(Sc()[0xA], -1);
    if (Sc()[0xA] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CDCF0: the spiral's matrix: Gte_PushMatrix; the tilt +0xC
// about y for an owner facing with bit 0, about x otherwise; the translation
// at the task's position and half its height.
S23_EXPORT void __cdecl FxSpiral_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const bool about_y = (Owner()[8] & 1) != 0;
    const unsigned char* const sc = Sc();
    const short tilt = static_cast<short>(Word(sc + 0xC));
    const short angles[3] = {about_y ? static_cast<short>(0) : tilt, about_y ? tilt : static_cast<short>(0), 0};
    short v[3];
    Position(sc, static_cast<short>(Word(sc + 0x3E)), v);
    LoadMatrix(angles, v);
}

// original 0x4CDDC0: the spiral band. A draw-mode primitive (0x35) linked at
// the position (row offset 2); inner radius +0x10, outer +0x10 + sin(0) *
// +0x14 >> 12; the angle index +0xB; 31 G4 quads round the circle, the outer
// radius +0x10 + sin(0x40 i) * +0x14 >> 12, the colour +0xA * i for the
// first seven and +0xA * 8 after, the inner pair +0xA; linked with 0x44
// bytes.
S23_EXPORT void __cdecl FxSpiral_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    {
        const unsigned char* const sc = Sc();
        MH_CALL(MapView_LinkPrimAt)(Long(sc + 0x34), Long(sc + 0x38), 2, 0xC);
    }
    {
        const unsigned char* const r = Sc() + 0x10;
        const int s = Sin(0);
        const unsigned char* const sc = Sc();
        Put(at::kR, (Imul(s, Long(sc + 0x14)) >> 12) + Word(r));
        Put(at::kR2, Word(r));
        Put(at::kW4, sc[0xB]);
        const int first = (sc[0xB] & 0x3F) << 6;
        Put(at::kW6, first);
        Put(at::kV1, SinOf(first, at::kR));
    }
    Put(at::kV1 + 2, CosOf(Get(at::kW6), at::kR));
    Put(at::kV3, SinOf(Get(at::kW6), at::kR2));
    Put(at::kV3 + 2, CosOf(Get(at::kW6), at::kR2));
    for (int i = 1, angle = 0x40; angle < 0x800; ++i, angle += 0x40) {
        Put(at::kW6, ((Byte(at::kW4) + i) & 0x3F) << 6);
        const int s = Sin(angle);
        {
            const unsigned char* const sc = Sc();
            const int outer = (Imul(s, Long(sc + 0x14)) >> 12) + Word(sc + 0x10);
            Put(at::kV0, Get(at::kV1));
            Put(at::kV0 + 2, Get(at::kV1 + 2));
            Put(at::kR, outer);
        }
        Put(at::kV0 + 4, 0);
        Put(at::kV1, SinOf(Get(at::kW6), at::kR));
        Put(at::kV1 + 2, CosOf(Get(at::kW6), at::kR));
        Put(at::kV1 + 4, 0);
        Put(at::kV2, Get(at::kV3));
        Put(at::kV2 + 2, Get(at::kV3 + 2));
        Put(at::kV2 + 4, 0);
        Put(at::kV3, SinOf(Get(at::kW6), at::kR2));
        Put(at::kV3 + 2, CosOf(Get(at::kW6), at::kR2));
        unsigned char* const prim = Gfx_PacketNext;
        Put(at::kV3 + 4, 0);
        MH_CALL(Gpu_SetPolyG4)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        {
            const unsigned char* const sc = Sc();
            Put(at::kW8, angle < 0x200 ? sc[0xA] * i : sc[0xA] << 3);
            Put(at::kWA, sc[0xA]);
        }
        Shade(prim, 4, at::kW8);
        Shade(prim, 0x14, at::kW8);
        Shade(prim, 0x24, at::kWA);
        Shade(prim, 0x34, at::kWA);
        Project4(prim, 8, 0x18, 0x28, 0x38);
        MH_CALL(Gte_PrimDepths4_10B)(prim);
        const unsigned char* const sc = Sc();
        MH_CALL(MapView_LinkPrimAt)(Long(sc + 0x34), Long(sc + 0x38), 2, 0x44);
    }
}

// ============================================================================
// MAGIC102, row 69 (Quake)
// ============================================================================

// original 0x4CE100: the kind-2 task (Magic_Rows row 69). Its phase +1
// through a four-entry stack table - Quake_Start, Quake_Rumble, Quake_Heave,
// Quake_End - unchecked in the original, ours aborts past it.
S23_EXPORT void __cdecl Quake_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Quake_Start, bof3::addr::Quake_Rumble,
                                                 bof3::addr::Quake_Heave, bof3::addr::Quake_End};
    Dispatch("Quake_Task", kPhases, 4);
}

// original 0x4CE140: the facing to 0x6959CC; the block's first column / row
// the view's cell (0x905E66 / 0x905E62) plus Quake_FacingOffsets[facing]
// (signed bytes, the facing unbounded); +0xB and +9 0; how high each party
// member, each enemy still in (its +0x3E kept too), each Sprite_Objects /
// Sprite_ObjectsExtra object in use and each Effect_Objects object of kind
// 0x17 stands above the ground (AreaMap_Elevation); the map's polygon codes
// made semi-transparent (Walk, Semi); the lift table zeroed; the hover flag
// for event battles 0x1C, 0x23, 0x2F and 0x34 (a byte table of the
// original's jump); sound 0x100; on.
S23_EXPORT void __cdecl Quake_Start(void) {
    const unsigned char facing = Facing();
    Mem(at::kQuakeFacing)[0] = facing;
    {
        const unsigned f = Byte(at::kQuakeFacing);
        const int dx = static_cast<signed char>(Byte(at::kQuakeOffsets + 2 * f));
        const int dy = static_cast<signed char>(Byte(at::kQuakeOffsets + 2 * f + 1));
        Put(at::kQuakeX, dx + Word(Mem(at::kCameraCellX)));
        Put(at::kQuakeY, dy + Word(Mem(at::kCameraCellY)));
    }
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (PartyOf(static_cast<unsigned char>(i))[0] == 0) continue;
        const long ground = Ground(Slot(i));
        Put(at::kQuakePartyZ + 2 * i, Word(PartyOf(static_cast<unsigned char>(i)) + 0x3E) - static_cast<short>(ground));
    }
    for (unsigned i = 0; i < 8; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(i + 3) != 0) continue;
        const unsigned char* const slot = Slot(3 + i);
        const std::int32_t x = Long(slot + 0x34), z = Long(slot + 0x38);
        const std::uint16_t height = Word(EnemyOf(static_cast<unsigned char>(i + 3)) + 0x3E);
        const long ground = MH_CALL(AreaMap_Elevation)(x, z);
        Put(at::kQuakeEnemyZ + 2 * i, height);
        Put(at::kQuakeEnemyUp + 2 * i, height - static_cast<short>(ground));
    }
    for (unsigned i = 0; i < 30; ++i) {
        const unsigned char* const o = Mem(at::kSpriteObjects + 0xA4 * i);
        if (o[0] == 0) continue;
        const long ground = Ground(o);
        Put(at::kQuakeSpriteZ + 2 * i, Word(o + 0x3E) - static_cast<short>(ground));
    }
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char* const o = Mem(at::kSpriteExtra + 0xA4 * i);
        if (o[0] == 0) continue;
        const long ground = Ground(o);
        Put(at::kQuakeExtraZ + 2 * i, Word(o + 0x3E) - static_cast<short>(ground));
    }
    for (unsigned i = 0; i < 20; ++i) {
        const unsigned char* const o = Mem(at::kEffectObjects + 0x80 * i);
        if (o[0] == 0 || o[5] != 0x17) continue;
        const long ground = Ground(o);
        Put(at::kQuakeEffectZ + 2 * i, Word(o + 0x3E) - static_cast<short>(ground));
    }
    Walk(&Semi);
    std::memset(Mem(at::kQuakeLift), 0, 0xFF);
    switch (Byte(at::kEventBattle)) {
    case 0x1C: case 0x23: case 0x2F: case 0x34: Mem(at::kQuakeHover)[0] = 1; break;
    default: Mem(at::kQuakeHover)[0] = 0; break;
    }
    MH_CALL(Sound_PlayById)(0x100);
    Bump(Sc()[1]);
}

// original 0x4CE4A0: the rumble. +9 up; at 0x10 back to 0 and on; the view
// redrawn (MapView_Redraw 2) and the camera shifted +-(+9 / 2) by the frame's
// parity.
S23_EXPORT void __cdecl Quake_Rumble(void) {
    Bump(Sc()[9]);
    unsigned char* sc = Sc();
    if (sc[9] >= 0x10) {
        sc[9] = 0;
        Bump(Sc()[1]);
        sc = Sc();
    }
    const int odd = static_cast<int>(Frame_Counter & 1);
    MapView_Redraw = 2;
    Put(at::kShiftY, (2 * odd - 1) * (sc[9] >> 1));
}

// original 0x4CE500: the heave. For each of 16 columns i the lift
// (sin((+9 + 2i) << 7) - sin(((+9 + 2i) << 7) - 0x80)) / 256 (0 at i = 0)
// and the fall (sin((+9 + 2i + 2) << 7) - sin((+9 + 2i + 1) << 7)) / 256 (0 at
// i = 15); for each of 14 rows j inside the area (bytes 0 / 1 of
// AreaMap_Header), the cell's four AreaMap_Corners bytes raised by lift and
// fall over the Quake_Divisors pair of j (the row by the facing's bit 1;
// across the map, i and j swap and so do the middle two corners), and the
// lift of the first corner kept in the lift table (the last in 0x695A2C).
// Then every actor and object put back on the ground with its kept height -
// the party when the target has bit 0x80, else the enemies still in (in the
// hover battles, an enemy above the ground keeps its own height) - the view
// redrawn, the camera shifted -8 / +8 by the frame's parity, +9 up; when the
// lift table is all 0 and +9 is past 0x5A, on.
S23_EXPORT void __cdecl Quake_Heave(void) {
    for (int i = 0; i < 16; ++i) {
        const int s0 = Sin(((Sc()[9] + 2 * i) * 128 - 0x80) & 0xFFF);
        const int s1 = Sin(((Sc()[9] + 2 * i) * 128) & 0xFFF);
        const int lift = (i != 0 ? 1 : 0) * ((s1 >> 8) - (s0 >> 8));
        const int s2 = Sin(((Sc()[9] + 2 * i + 2) * 128) & 0xFFF);
        const int s3 = Sin(((Sc()[9] + 2 * i + 1) * 128) & 0xFFF);
        const int fall = (i != 15 ? 1 : 0) * ((s2 >> 8) - (s3 >> 8));
        const bool across = (Byte(at::kQuakeFacing) & 1) != 0;
        for (int j = 0; j < 14; ++j) {
            const std::uint32_t area = static_cast<std::uint32_t>(Long(Mem(at::kArea)));
            const int width = static_cast<int>(area & 0xFF), height = static_cast<int>((area >> 8) & 0xFF);
            const int first_row = Get(at::kQuakeY);
            const int row = first_row + (across ? j : i);
            if (row < 0 || row >= height) continue;
            const int col = Get(at::kQuakeX) + (across ? i : j);
            if (col < 0 || col >= width) continue;
            const unsigned row_of_divisors = (Byte(at::kQuakeFacing) >> 1) * 15u;
            const std::uint32_t corner =
                at::kAreaCorners + 4u * static_cast<std::uint32_t>(Imul(static_cast<int>(Long(Mem(at::kArea)) & 0xFF), row) + col);
            const unsigned d0 = Byte(at::kQuakeDivisors + row_of_divisors + static_cast<unsigned>(j));
            const int raised = Divide(lift, d0);
            AddByte(corner, raised);
            const unsigned d1 = Byte(at::kQuakeDivisors + row_of_divisors + static_cast<unsigned>(j) + 1);
            AddByte(corner + (across ? 2 : 1), Divide(lift, d1));
            AddByte(corner + (across ? 1 : 2), Divide(fall, d0));
            AddByte(corner + 3, Divide(fall, d1));
            const std::uint32_t k = static_cast<std::uint32_t>(i * 15 + j);
            const unsigned char before = Byte(at::kQuakeLift + k);
            Mem(at::kQuakeLast + k)[0] = before;
            Mem(at::kQuakeLift + k)[0] = static_cast<unsigned char>(before + raised);
        }
    }
    if (Mem(hat::kTarget)[0] & 0x80) {
        for (unsigned i = 0; i < 3; ++i) {
            if (PartyOf(static_cast<unsigned char>(i))[0] == 0) continue;
            const long ground = Ground(Slot(i));
            SetWord(PartyOf(static_cast<unsigned char>(i)) + 0x3E,
                    static_cast<unsigned>(static_cast<short>(ground) + Get(at::kQuakePartyZ + 2 * i)));
        }
    } else {
        for (unsigned i = 0; i < 8; ++i) {
            if (MH_CALL(Battle_ActorIsOut)(i + 3) != 0) continue;
            const unsigned char* const slot = Slot(3 + i);
            if (Byte(at::kQuakeHover) != 0) {
                const short kept = Get(at::kQuakeEnemyZ + 2 * i);
                const short ground = static_cast<short>(Ground(slot));
                if (kept > ground) {
                    SetWord(EnemyOf(static_cast<unsigned char>(i + 3)) + 0x3E, static_cast<std::uint16_t>(kept));
                    continue;
                }
            }
            const long ground = Ground(slot);
            SetWord(EnemyOf(static_cast<unsigned char>(i + 3)) + 0x3E,
                    static_cast<unsigned>(static_cast<short>(ground) + Get(at::kQuakeEnemyUp + 2 * i)));
        }
    }
    for (unsigned i = 0; i < 30; ++i) {
        unsigned char* const o = Mem(at::kSpriteObjects + 0xA4 * i);
        if (o[0] == 0) continue;
        const long ground = Ground(o);
        SetWord(o + 0x3E, static_cast<unsigned>(static_cast<short>(ground) + Get(at::kQuakeSpriteZ + 2 * i)));
    }
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const o = Mem(at::kSpriteExtra + 0xA4 * i);
        if (o[0] == 0) continue;
        const long ground = Ground(o);
        SetWord(o + 0x3E, static_cast<unsigned>(static_cast<short>(ground) + Get(at::kQuakeExtraZ + 2 * i)));
    }
    for (unsigned i = 0; i < 20; ++i) {
        unsigned char* const o = Mem(at::kEffectObjects + 0x80 * i);
        if (o[0] == 0 || o[5] != 0x17) continue;
        const long ground = Ground(o);
        SetWord(o + 0x3E, static_cast<unsigned>(static_cast<short>(ground) + Get(at::kQuakeEffectZ + 2 * i)));
    }
    const int odd = static_cast<int>(Frame_Counter & 1);
    unsigned char* const sc = Sc();
    MapView_Redraw = 2;
    Put(at::kShiftY, (odd << 4) - 8);
    Bump(sc[9]);
    for (std::uint32_t row = 0; row < 16; ++row)
        for (std::uint32_t j = 0; j < 14; ++j)
            if (Byte(at::kQuakeLift + row * 15 + j) != 0) return;
    if (sc[9] > 0x5A) Bump(sc[1]);
}

// original 0x4CEA20: the end. The map's polygon codes put back (Walk,
// Opaque: a code that was 0x28 before the start becomes 0 too); the camera
// shift 0; the target's flag 0x40; the done flag; the task freed; the view
// redrawn.
S23_EXPORT void __cdecl Quake_End(void) {
    Walk(&Opaque);
    Put(at::kShiftY, 0);
    MH_CALL(Battle_SetTargetFlag40)(Mem(hat::kTarget)[0]);
    Mem(hat::kFlags)[0] = static_cast<unsigned char>(Mem(hat::kFlags)[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
    MapView_Redraw = 2;
}

// ============================================================================
// MAGIC103, row 57 (Simoon)
// ============================================================================

// original 0x4CF5F0: the kind-2 task (Magic_Rows row 57). Its phase +1
// through a three-entry stack table - Simoon_Start, Simoon_Wait, 0x43FE80
// (group E's: the done flag and free) - unchecked in the original, ours
// aborts past it.
S23_EXPORT void __cdecl Simoon_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Simoon_Start, bof3::addr::Simoon_Wait,
                                                 magic_s23::kEnginePhase};
    Dispatch("Simoon_Task", kPhases, 3);
}

// original 0x4CF620: four effects of kind 1, 0x2D, owned by the task and
// counted in its +0xB: a fan (+1 = 2), two dust sprites (+1 = 1, +0xB their
// index, +9 its delay i + 1) and a dome (+1 = 0); then the CLUT strip's two
// rows at 0x812980 made semi-transparent - cells 1..15 of each copied from
// 0x4000 below with bit 15, cell 0 cleared - and marked dirty; sound 0x100;
// on.
S23_EXPORT void __cdecl Simoon_Start(void) {
    {
        unsigned char* sc;
        unsigned char* const t = Child(0x2D, sc);
        t[1] = 2;
        sc[0xB] = 1;
    }
    for (unsigned i = 0; i < 2; ++i) {
        unsigned char* sc;
        unsigned char* const t = Child(0x2D, sc);
        t[1] = 1;
        t[0xB] = static_cast<unsigned char>(i);
        t[9] = static_cast<unsigned char>(i + 1);
        Bump(sc[0xB]);
    }
    {
        unsigned char* sc;
        unsigned char* const t = Child(0x2D, sc);
        t[1] = 0;
        Bump(sc[0xB]);
    }
    for (std::uint32_t a = at::kClutRow + 2; a < at::kClutRow + 0x20; a += 2) {
        SetWord(Mem(a), Word(Mem(a - 0x4000)) | 0x8000u);
        SetWord(Mem(a + 0x20), Word(Mem(a + 0x20 - 0x4000)) | 0x8000u);
    }
    SetWord(Mem(at::kClutRow), 0);
    SetWord(Mem(at::kClutRow + 0x20), 0);
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Bump(Sc()[1]);
}

// original 0x4CF740: once the four are gone (+0xB 0), the target's flag
// 0x40, on.
S23_EXPORT void __cdecl Simoon_Wait(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(Mem(hat::kTarget)[0]);
    Bump(Sc()[1]);
}

// original 0x4CF770: the effect (kind 1, 0x2D): jmp [SimoonFx_Types + 4 *
// +1], read in place, unchecked.
S23_EXPORT void __cdecl SimoonFx_Dispatch(void) { Entry(at::kSimoonTypes, Sc()[1])(); }

// original 0x4CF790: SimoonFx_Types[0]. Its phase +2 through
// SimoonDome_Phases (read in place, unchecked); alive and past phase 0, the
// dome under the actor's matrix.
S23_EXPORT void __cdecl SimoonDome_Task(void) {
    Entry(at::kDomePhases, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(SimoonDome_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4CF7D0: SimoonDome_Phases[0]. The source's x / z and height;
// +9 and +0xA 0; on.
S23_EXPORT void __cdecl SimoonDome_Start(void) {
    const unsigned char* const src = Pointer(hat::kSource);
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetWord(Sc() + 0x3E, Word(src + 0x3E));
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// original 0x4CF820: SimoonDome_Phases[1]. At +9 = 4 the target's flags 0x10;
// the size +9 * 28; +9 up, at 8 on.
S23_EXPORT void __cdecl SimoonDome_Grow(void) {
    unsigned char* sc = Sc();
    if (sc[9] == 4) {
        MH_CALL(Battle_SetTargetFlags)(Mem(hat::kTarget)[0], 0x10);
        sc = Sc();
    }
    Put(at::kR, sc[9] * 28);
    Bump(sc[9]);
    if (Sc()[9] == 8) Bump(Sc()[2]);
}

// original 0x4CF870: SimoonDome_Phases[2]. The size 0xC0 + +9 * 4; +0xA and
// +9 up; at +9 = 0x28 the owner's count +0xB down and the task freed.
S23_EXPORT void __cdecl SimoonDome_Expand(void) {
    unsigned char* const sc = Sc();
    Put(at::kR, sc[9] * 4 + 0xC0);
    Bump(sc[0xA]);
    Bump(Sc()[9]);
    if (Sc()[9] != 0x28) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4CF8C0: the dome. Eight rings of 32 GT4 quads from latitude
// 0xC00 up in steps of 0x80 at the size 0x903850: each ring's two colours
// 0x81, or once +9 is past 0x10 plus the ring 0x81 - 8 d (1 for d of 0x11 or
// more); each quad a draw-mode primitive (0x35) and the quad, both linked at
// its outer corner's map position, textured from the page (0x340, 0x100)
// and the CLUT (0, 0x1FA), the ring's v rows 2 (0x14 + 0x18 - ring) + +0xA,
// times 4.
S23_EXPORT void __cdecl SimoonDome_Draw(void) {
    for (int ring = 1; ring <= 8; ++ring) {
        const int top = 0xC00 - 0x80 * (ring - 1);
        const int band = 0x18 - (ring - 1);
        {
            const unsigned char* const sc = Sc();
            Put(at::kW6, 0x81);
            Put(at::kW8, 0x81);
            if (ring - 1 < static_cast<int>(sc[9]) - 0x10) {
                const short d = static_cast<short>(sc[9] - ring - 0xF);
                Put(at::kW6, d < 0x11 ? 0x81 - (d << 3) : 1);
            }
            if (ring < static_cast<int>(sc[9]) - 0x10) {
                const short d = static_cast<short>(sc[9] - ring - 0x10);
                Put(at::kW8, d);
                Put(at::kW8, d < 0x11 ? 0x81 - (Get(at::kW8) << 3) : 1);
            }
        }
        const int lower = top - 0x80;
        Put(at::kR2, CosOf(lower, at::kR));
        Put(at::kW4, CosOf(top, at::kR));
        Put(at::kV1, SinOf(0, at::kR2));
        Put(at::kV1 + 2, CosOf(0, at::kR2));
        {
            const int z = SinOf(lower, at::kR);
            Put(at::kV0 + 4, z);
            Put(at::kV1 + 4, z);
        }
        Put(at::kV3, SinOf(0, at::kW4));
        Put(at::kV3 + 2, CosOf(0, at::kW4));
        {
            const int z = SinOf(top, at::kR);
            Put(at::kV2 + 4, z);
            Put(at::kV3 + 4, z);
        }
        const unsigned char v_low = static_cast<unsigned char>((band + 0x14) << 1);
        const unsigned char v_high = static_cast<unsigned char>((band + 0x15) << 1);
        for (int j = 1, a = 0x80; a < 0x1080; ++j, a += 0x80) {
            Put(at::kV0, Get(at::kV1));
            Put(at::kV0 + 2, Get(at::kV1 + 2));
            Put(at::kV1, SinOf(a, at::kR2));
            Put(at::kV1 + 2, CosOf(a, at::kR2));
            Put(at::kV2, Get(at::kV3));
            Put(at::kV2 + 2, Get(at::kV3 + 2));
            Put(at::kV3, SinOf(a, at::kW4));
            const int y = CosOf(a, at::kW4);
            const unsigned char* const sc = Sc();
            Put(at::kV3 + 2, y);
            const std::int32_t x = Shl(Get(at::kV3), 9) + Long(sc + 0x34);
            const std::int32_t z = Shl(static_cast<short>(y), 9) + Long(sc + 0x38);
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0xC);
            unsigned char* const prim = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyGT4)(prim);
            MH_CALL(Gpu_SetSemiTrans)(prim, 1);
            Project4(prim, 8, 0x1C, 0x30, 0x44);
            MH_CALL(Gte_PrimDepths4_14)(prim);
            SetWord(prim + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
            SetWord(prim + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
            const unsigned char u0 = static_cast<unsigned char>(j << 3);
            const unsigned char u1 = static_cast<unsigned char>((j + 1) << 3);
            prim[0x14] = u0;
            prim[0x15] = static_cast<unsigned char>((Sc()[0xA] + v_low) << 2);
            prim[0x28] = u1;
            prim[0x29] = static_cast<unsigned char>((Sc()[0xA] + v_low) << 2);
            prim[0x3C] = u0;
            prim[0x50] = u1;
            prim[0x3D] = static_cast<unsigned char>((Sc()[0xA] + v_high) << 2);
            prim[0x51] = static_cast<unsigned char>((Sc()[0xA] + v_high) << 2);
            Shade(prim, 4, at::kW8);
            Shade(prim, 0x18, at::kW8);
            Shade(prim, 0x2C, at::kW6);
            Shade(prim, 0x40, at::kW6);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), 2, 0x54);
        }
    }
}

// original 0x4CFD20: SimoonFx_Types[1]. Its phase +2 through
// SimoonDust_Phases (read in place, unchecked); alive and past phase 0, the
// dust drawn (a tail jmp in the original).
S23_EXPORT void __cdecl SimoonDust_Task(void) {
    Entry(at::kDustPhases, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(SimoonDust_Draw)();
}

// original 0x4CFD50: SimoonDust_Phases[0]. +9 down; at 0 the source's
// position plus SimoonDust_Offsets[+0xB] (three dwords, +0xB unbounded);
// +9 and +0xA 0; on.
S23_EXPORT void __cdecl SimoonDust_Wait(void) {
    Bump(Sc()[9], -1);
    if (Sc()[9] != 0) return;
    const unsigned char* const src = Pointer(hat::kSource);
    for (unsigned k = 0; k < 3; ++k) {
        unsigned char* const sc = Sc();
        const std::int32_t base = Long(src + 0x34 + 4 * k);
        SetLong(sc + 0x34 + 4 * k, Long(Mem(at::kDustOffsets + 4 * k + 12u * sc[0xB])) + base);
    }
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// original 0x4CFDE0: SimoonDust_Phases[1]. The screen point; +0xA up 2, +9
// up, at 8 on.
S23_EXPORT void __cdecl SimoonDust_Rise(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    Bump(Sc()[0xA], 2);
    Bump(Sc()[9]);
    if (Sc()[9] == 8) Bump(Sc()[2]);
}

// The dust's arc: the screen y +0x30 less (+9 - 8) / 2.
void Arc() {
    unsigned char* const sc = Sc();
    SetWord(sc + 0x30, static_cast<unsigned>(Word(sc + 0x30) - (static_cast<int>(sc[9]) - 8) / 2));
}

// original 0x4CFE10: SimoonDust_Phases[2]. The screen point; the arc; +9 up,
// at 0x1C on.
S23_EXPORT void __cdecl SimoonDust_Arc(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    Arc();
    Bump(Sc()[9]);
    if (Sc()[9] == 0x1C) Bump(Sc()[2]);
}

// original 0x4CFE50: SimoonDust_Phases[3]. The screen point; the arc; +9 up
// to 0x2C; +0xA down, at 0 the owner's count +0xB down and the task freed.
S23_EXPORT void __cdecl SimoonDust_Fall(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    Arc();
    unsigned char* sc = Sc();
    if (sc[9] != 0x2C) {
        Bump(sc[9]);
        sc = Sc();
    }
    Bump(sc[0xA], -1);
    if (Sc()[0xA] != 0) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// One corner of the dust's quad: sin / cos of the angle by the size, plus
// the screen point, as floats.
void DustCorner(unsigned char* prim, unsigned offset, unsigned angle_of_life) {
    const int x = SinOf(static_cast<int>(angle_of_life), at::kR) + Get(at::kWC);
    const float fx = static_cast<float>(x);
    std::memcpy(prim + offset, &fx, sizeof fx);
}

// original 0x4CFEB0: the dust. A draw-mode primitive (0x35) linked at the
// position (row offset 0); a GT4 in screen space: its size +9 * 2 after
// phase 1, (+9 - 8) / 4 + 0x10 before; its colour (+0xA * 8, * 7, * 4); its
// corners at the screen point +0x2E / +0x30 turned by +9 (the four angles
// (+9 - 0x80), (+9 + 0x40), (+9 - 0x40) as bytes, and +9, times 16); the page
// (0x340, 0x100), the CLUT (0x10, 0x1FA), u / v 1..0x1F; linked with 0x54
// bytes.
S23_EXPORT void __cdecl SimoonDust_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    {
        const unsigned char* const sc = Sc();
        MH_CALL(MapView_LinkPrimAt)(Long(sc + 0x34), Long(sc + 0x38), 0, 0xC);
    }
    unsigned char* const prim = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(prim);
    MH_CALL(Gpu_SetSemiTrans)(prim, 1);
    {
        const unsigned char* const sc = Sc();
        Put(at::kR, sc[2] > 1 ? sc[9] * 2 : (static_cast<int>(sc[9]) - 8) / 4 + 0x10);
        Put(at::kW6, sc[0xA] << 3);
        Put(at::kW8, sc[0xA] * 7);
        Put(at::kWA, sc[0xA] << 2);
        Put(at::kWC, Word(sc + 0x2E));
        Put(at::kWE, Word(sc + 0x30));
    }
    static constexpr unsigned kX[4] = {8, 0x1C, 0x30, 0x44};
    static constexpr int kTurn[4] = {-0x80, 0x40, -0x40, 0};
    for (unsigned k = 0; k < 4; ++k) {
        const unsigned angle = k < 3 ? ((Sc()[9] + kTurn[k]) & 0xFF) << 4 : static_cast<unsigned>(Sc()[9]) << 4;
        DustCorner(prim, kX[k], angle);
        const unsigned again = k < 3 ? ((Sc()[9] + kTurn[k]) & 0xFF) << 4 : static_cast<unsigned>(Sc()[9]) << 4;
        const int y = CosOf(static_cast<int>(again), at::kR) + Get(at::kWE);
        const float fy = static_cast<float>(y);
        std::memcpy(prim + kX[k] + 4, &fy, sizeof fy);
    }
    SetWord(prim + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
    SetWord(prim + 0x16, MH_CALL(Gpu_GetClut)(0x10, 0x1FA));
    prim[0x14] = 1;
    prim[0x15] = 1;
    prim[0x28] = 0x1F;
    prim[0x29] = 1;
    prim[0x3C] = 1;
    prim[0x3D] = 0x1F;
    prim[0x50] = 0x1F;
    prim[0x51] = 0x1F;
    for (unsigned k = 0; k < 4; ++k) {
        prim[4 + 0x14 * k] = Byte(at::kW6);
        prim[5 + 0x14 * k] = Byte(at::kW8);
        prim[6 + 0x14 * k] = Byte(at::kWA);
    }
    const unsigned char* const sc = Sc();
    MH_CALL(MapView_LinkPrimAt)(Long(sc + 0x34), Long(sc + 0x38), 0, 0x54);
}

// original 0x4D0200: SimoonFx_Types[2]. Its phase +2 through
// SimoonFan_Phases (read in place, unchecked; entries 2 and 3 are MAGIC056's
// 0x4AE0D0 / 0x4AE0F0); alive and past phase 0, the fan under the actor's
// matrix.
S23_EXPORT void __cdecl SimoonFan_Task(void) {
    Entry(at::kFanPhases, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(SimoonFan_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4D0240: SimoonFan_Phases[0]. The source's position; +9 and
// +0xA 0; on.
S23_EXPORT void __cdecl SimoonFan_Start(void) {
    const unsigned char* const src = Pointer(hat::kSource);
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetLong(Sc() + 0x3C, Long(src + 0x3C));
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Bump(Sc()[2]);
}

// original 0x4D0290: SimoonFan_Phases[1]. +0xA up 2, +9 up, at 8 on.
S23_EXPORT void __cdecl SimoonFan_Grow(void) {
    Bump(Sc()[0xA], 2);
    Bump(Sc()[9]);
    if (Sc()[9] == 8) Bump(Sc()[2]);
}

// original 0x4D02C0: the fan. A draw-mode primitive (0x55) committed to slot
// 5; the radius cos(0x800) * (+9 + 0x58) / 1024 after phase 1, cos(0x800) *
// +9 * 3 / 256 before; colours +0xA * 6 (centre red) and +0xA * 8; sixteen G3
// triangles from the centre, 0x100 apart, the outer corners black; committed
// with 0x34 bytes.
S23_EXPORT void __cdecl SimoonFan_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    const unsigned char* sc = Sc();
    if (sc[2] > 1) {
        const int c = Cos(0x800);
        sc = Sc();
        Put(at::kR, Shl(Imul(c, sc[9] + 0x58), 2) >> 12);
    } else {
        const int c = Cos(0x800);
        sc = Sc();
        Put(at::kR, Shl(Imul(Imul(c, sc[9]), 3), 4) >> 12);
    }
    Put(at::kW6, sc[0xA] << 3);
    Put(at::kW8, sc[0xA] * 6);
    Put(at::kV2, SinOf(0, at::kR));
    Put(at::kV2 + 2, CosOf(0, at::kR));
    Put(at::kV2 + 4, 0);
    Put(at::kV1 + 4, 0);
    Put(at::kV0 + 4, 0);
    for (int angle = 0x100; angle < 0x1100; angle += 0x100) {
        unsigned char* const prim = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        const short x = Get(at::kV2), y = Get(at::kV2 + 2);
        Put(at::kV0, 0);
        Put(at::kV0 + 2, 0);
        Put(at::kV1, x);
        Put(at::kV1 + 2, y);
        Put(at::kV2, SinOf(angle, at::kR));
        Put(at::kV2 + 2, CosOf(angle, at::kR));
        prim[4] = Byte(at::kW8);
        prim[5] = Byte(at::kW6);
        prim[6] = Byte(at::kW6);
        Shade(prim, 0x14, static_cast<unsigned char>(0));
        Shade(prim, 0x24, static_cast<unsigned char>(0));
        Project3(prim);
        MH_CALL(Gte_PrimDepths3_10B)(prim);
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
}

void MagicS23_Inject() {
    if (bof3::WantsShadow("magic_s23")) magic_s23::SelfTest();
    BOF3_INJECT(Cyclone_Task);
    BOF3_INJECT(Cyclone_Start);
    BOF3_INJECT(Cyclone_Wait);
    BOF3_INJECT(FxFunnel_Dispatch);
    BOF3_INJECT(FxFunnel_Task);
    BOF3_INJECT(FxFunnel_Wait);
    BOF3_INJECT(FxFunnel_Approach);
    BOF3_INJECT(FxFunnel_Orbit);
    BOF3_INJECT(FxFunnel_Rise);
    BOF3_INJECT(FxFunnel_PushMatrix);
    BOF3_INJECT(FxFunnel_Draw);
    BOF3_INJECT(FxFunnel_PushGroundMatrix);
    BOF3_INJECT(FxFunnel_DrawGround);
    BOF3_INJECT(Typhoon_Task);
    BOF3_INJECT(Typhoon_Start);
    BOF3_INJECT(Typhoon_Grow);
    BOF3_INJECT(Typhoon_Fade);
    BOF3_INJECT(Typhoon_DrawFan);
    BOF3_INJECT(FxSpiral_Dispatch);
    BOF3_INJECT(FxSpiral_Task);
    BOF3_INJECT(FxSpiral_Wait);
    BOF3_INJECT(FxSpiral_Grow);
    BOF3_INJECT(FxSpiral_Hold);
    BOF3_INJECT(FxSpiral_Spread);
    BOF3_INJECT(FxSpiral_Fade);
    BOF3_INJECT(FxSpiral_PushMatrix);
    BOF3_INJECT(FxSpiral_Draw);
    BOF3_INJECT(Quake_Task);
    BOF3_INJECT(Quake_Start);
    BOF3_INJECT(Quake_Rumble);
    BOF3_INJECT(Quake_Heave);
    BOF3_INJECT(Quake_End);
    BOF3_INJECT(Simoon_Task);
    BOF3_INJECT(Simoon_Start);
    BOF3_INJECT(Simoon_Wait);
    BOF3_INJECT(SimoonFx_Dispatch);
    BOF3_INJECT(SimoonDome_Task);
    BOF3_INJECT(SimoonDome_Start);
    BOF3_INJECT(SimoonDome_Grow);
    BOF3_INJECT(SimoonDome_Expand);
    BOF3_INJECT(SimoonDome_Draw);
    BOF3_INJECT(SimoonDust_Task);
    BOF3_INJECT(SimoonDust_Wait);
    BOF3_INJECT(SimoonDust_Rise);
    BOF3_INJECT(SimoonDust_Arc);
    BOF3_INJECT(SimoonDust_Fall);
    BOF3_INJECT(SimoonDust_Draw);
    BOF3_INJECT(SimoonFan_Task);
    BOF3_INJECT(SimoonFan_Start);
    BOF3_INJECT(SimoonFan_Grow);
    BOF3_INJECT(SimoonFan_Draw);
}
