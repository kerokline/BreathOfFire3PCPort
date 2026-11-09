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
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
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
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
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
    SetLong(cell, static_cast<std::int32_t>(Low32(p)));
}
unsigned char* Slot(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + index * hat::kTaskStride); }

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
    MH_CALL(Gte_RotTransPers4)(Vertex(::bof3::Img(Prim_VertexScratch_addr)), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18), At(prim, a), At(prim, b),
                               At(prim, c), At(prim, d), &depth);
}
void Project3(unsigned char* prim) {
    long depth = 0;
    MH_CALL(Gte_RotTransPers3)(Vertex(::bof3::Img(Prim_VertexScratch_addr)), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10), At(prim, 8), At(prim, 0x18), At(prim, 0x28),
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
    v[0] = static_cast<short>((rv::As<records::SpriteHead>(sc)->x >> 9) - 0x4000);
    v[1] = static_cast<short>((rv::As<records::SpriteHead>(sc)->z >> 9) - 0x4000);
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

// The acting side's facing: the actor sprite's +8, turned by 2 when the
// target's bit 0x40 and the actor's side disagree (bit 0x40 and a party
// actor, or neither and an enemy).
unsigned char Facing() {
    const bool bit = (Mem(::bof3::Img(hat::kTarget))[0] & 0x40) != 0;
    const bool party = Mem(::bof3::Img(hat::kActor))[0] < 3;
    const unsigned char f = Pointer(::bof3::Img(MagicS23_ActorSprite_addr))[8];
    return bit == party ? static_cast<unsigned char>(f ^ 2) : f;
}

long Ground(const unsigned char* at_position) {
    return MH_CALL(AreaMap_Elevation)(rv::As<records::SpriteHead>(at_position)->x, rv::As<records::SpriteHead>(at_position)->z);
}

}  // namespace

#define S23_EXPORT extern "C" __attribute__((disable_tail_calls))

// ============================================================================
// MAGIC100, row 13 (Cyclone)
// ============================================================================

// ============================================================================
// MAGIC101, row 26 (Typhoon)
// ============================================================================

// ============================================================================
// MAGIC102, row 69 (Quake)
// ============================================================================

// ============================================================================
// MAGIC103, row 57 (Simoon)
// ============================================================================

void MagicS23_Inject() {
    if (bof3::WantsShadow("magic_s23")) magic_s23::SelfTest();
}
