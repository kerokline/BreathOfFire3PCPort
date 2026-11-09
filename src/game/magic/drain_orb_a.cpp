// Moved here by tools/move_funcs.py from magic_s18.cpp (docs/source-layout-round.md).
#include "game/magic/drain_orb_a.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s18.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::SetWord;

using move_script::Word;

// The GTE calls as the originals push them: one pointer more than
// symbols.toml's prototypes carry (a flag word the callee may write).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                               long*, long*);

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

unsigned char* SC() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Slot(unsigned char k) { return Mem(::bof3::Img(BattleTask_Slots_addr) + k * at::kTaskStride); }

std::int32_t Ptr(const unsigned char* p) { return static_cast<std::int32_t>(Low32(p)); }

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::int32_t AddL(std::int32_t a, std::uint32_t b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + b); }

// The scratch words SharedScratch + k (the PSX scratchpad's) and the vertex
// scratch Prim_VertexScratch + k: three SVECTORs at +0, +8, +0x10, +0x18.
constexpr ::bof3::image::Va kDs{SharedScratch_addr};

// kVx: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch
short Ds(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(kDs.v) + k))); }

void SetDs(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kDs.v) + k), v); }

short Vx(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k))); }

void SetVx(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v); }

const short* VxP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

// `imul` (a 32-bit product that wraps) then `sar n`; `shl l` then `sar r`.
int MulSar(int a, int b, int n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

int ShlSar(int a, int l, int r) { return static_cast<int>(static_cast<std::uint32_t>(a) << l) >> r; }

float* Fl(unsigned char* p) { return reinterpret_cast<float*>(p); }

// A word of a table of dwords (the originals read the low word at stride 4),
// the index signed as the originals' movsx gives it.
unsigned TableWord(const unsigned long* table, int index) {
    return Word(reinterpret_cast<const unsigned char*>(table) + static_cast<std::int32_t>(index) * 4);
}

// A .data dispatch table read in place: the cell called, the index checked
// (the originals' is not).
void CallCell(const unsigned long* table, unsigned entries, unsigned index, const char* who) {
    if (index >= entries) bof3::Fatal("%s: index %u, past the %u-entry table", who, index, entries);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

// A stack table's handler by the phase +1.
void CallPhase(const std::uint32_t* phases, unsigned entries, const char* who) {
    const unsigned phase = SC()[1];
    if (phase >= entries) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
    magic_harness::Phase(phases[phase])();
}

void LinkAt(unsigned dy, unsigned size) {
    const unsigned char* const sc = SC();
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z),
                                static_cast<int>(dy), size);
}

long Rtp3(unsigned char* p) {
    long depth = 0, flag = 0;
    return MH_CALL(As<Rtp3Fn>(&::Gte_RotTransPers3))(VxP(0), VxP(8), VxP(0x10), Fl(p + 8), Fl(p + 0x18), Fl(p + 0x28), &depth,
                                                     &flag);
}

long Rtp4(unsigned char* p) {
    long depth = 0, flag = 0;
    return MH_CALL(As<Rtp4Fn>(&::Gte_RotTransPers4))(VxP(0), VxP(8), VxP(0x10), VxP(0x18), Fl(p + 8), Fl(p + 0x18),
                                                     Fl(p + 0x28), Fl(p + 0x38), &depth, &flag);
}

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// A colour triple from one signed word c: c, c / 3, c / 2 (each truncated
// toward zero, as the originals' multiply-by-0x55555556 and cdq/sar give it).
void Shade3(unsigned char* q, short c) {
    q[0] = static_cast<unsigned char>(c);
    q[1] = static_cast<unsigned char>(c / 3);
    q[2] = static_cast<unsigned char>(c / 2);
}

// The four types' shared task body: the phase through its .data table, then
// the draw between the actor matrix's push and pop while the task is live
// (+0) and past its first phase (+2).
template <void (__cdecl* Draw)(void)> void OrbTask(const unsigned long* phases, unsigned n, const char* who) {
    CallCell(phases, n, SC()[2], who);
    const unsigned char* const sc = SC();
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// Place the task at an object's position plus (dx, dz, dy).
void PlaceAt(const unsigned char* obj, std::uint32_t dx, std::uint32_t dz, std::uint32_t dy) {
    rv::Store(rv::Current<records::BattleTask>()->x, AddL(rv::As<records::SpriteHead>(obj)->x, dx));
    rv::Store(rv::Current<records::BattleTask>()->z, AddL(rv::As<records::SpriteHead>(obj)->z, dz));
    rv::Store(rv::Current<records::BattleTask>()->height, AddL(rv::As<records::SpriteHead>(obj)->height, dy));
}

}  // namespace

#define MS18_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4BEB50: the kind-2 task. Its phase +1 through a two-entry stack
// table: Drain_Start, BattleFx_Finish (round eight's). Unchecked in the
// original; ours aborts past it.
MS18_EXPORT void __cdecl Drain_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Drain_Start, bof3::addr::BattleFx_Finish};
    CallPhase(kPhases, 2, "Drain_Task");
}

// original 0x4BEB80: +0xB 0; four children BattleTask_Create(1, 0x2B), each
// with the owner +0x80 this task, +1 its type 0..3, +2 0, +9 0x10, and this
// task's +0xB counting them; Sound_PlayById(0x100); the phase on.
MS18_EXPORT void __cdecl Drain_Start(void) {
    SC()[0xB] = 0;
    for (unsigned char i = 0; i < 4; ++i) {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x2B));
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        unsigned char* const sc = SC();
        tt->owner = Ptr(sc);
        t[1] = i;
        t[2] = 0;
        t[9] = 0x10;
        Bump(sc[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
    Bump(SC()[1]);
}

// original 0x4BEC00: the child (kind 1, parameter 0x2B): jmp [DrainOrb_Types
// + 4 * +1].
MS18_EXPORT void __cdecl DrainOrb_Dispatch(void) { CallCell(DrainOrb_Types, 4, SC()[1], "DrainOrb_Dispatch"); }

// original 0x4BEC20: type 0 - its five phases, DrainOrb_Draw.
MS18_EXPORT void __cdecl DrainOrbA_Task(void) { OrbTask<&::DrainOrb_Draw>(DrainOrbA_Phases, 5, "DrainOrbA_Task"); }

// original 0x4BEC60: at the target's object less 0x4000 in x and z, 0x400 up
// in the high word of +0x3C; +0xB (the draw's radius) 0x10, +9 0x10, +0xA
// 0xB0; on.
MS18_EXPORT void __cdecl DrainOrbA_Place(void) {
    PlaceAt(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)), 0xFFFFC000u, 0xFFFFC000u, 0x4000000u);
    SC()[0xB] = 0x10;
    SC()[9] = 0x10;
    SC()[0xA] = 0xB0;
    Bump(SC()[2]);
}

// original 0x4BECD0: the word +0x3E down 0x40; +9 down; on at 0.
MS18_EXPORT void __cdecl DrainOrbA_Drop(void) {
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::Current<records::BattleTask>()->height_whole + 0xFFC0u);
    Drop(SC()[9]);
    if (SC()[9] == 0) Bump(SC()[2]);
}

// original 0x4BED00: +9 up to 0x20; +0xA down; at 0x30, +9 0 and on.
MS18_EXPORT void __cdecl DrainOrbA_Run(void) {
    unsigned char* const sc = SC();
    if (sc[9] != 0x20) sc[9] = static_cast<unsigned char>(sc[9] + 1);
    Drop(SC()[0xA]);
    if (SC()[0xA] != 0x30) return;
    SC()[9] = 0;
    Bump(SC()[2]);
}

// original 0x4BED40: +9 up to 0x20; +0xA down; at 0x10, the owner's +0xB
// down and on.
MS18_EXPORT void __cdecl DrainOrbA_Signal(void) {
    unsigned char* const sc = SC();
    if (sc[9] != 0x20) sc[9] = static_cast<unsigned char>(sc[9] + 1);
    Drop(SC()[0xA]);
    if (SC()[0xA] != 0x10) return;
    Drop(Owner()[0xB]);
    Bump(SC()[2]);
}

// original 0x4BED90: +0xA and +0xB down; at +0xB 0, freed.
MS18_EXPORT void __cdecl DrainOrbA_Shrink(void) {
    Drop(SC()[0xA]);
    Drop(SC()[0xB]);
    if (SC()[0xB] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BEDC0: type 1 - its five phases, DrainOrb_Draw.
MS18_EXPORT void __cdecl DrainOrbB_Task(void) { OrbTask<&::DrainOrb_Draw>(DrainOrbB_Phases, 5, "DrainOrbB_Task"); }

// original 0x4BEE00: +9 down; at 0, at the actor's object less 0x4000 in x
// and z, 0x400 up; +0xB 0x10, +9 0x10, +0xA 0; on.
MS18_EXPORT void __cdecl DrainOrbB_Place(void) {
    Drop(SC()[9]);
    if (SC()[9] != 0) return;
    PlaceAt(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)), 0xFFFFC000u, 0xFFFFC000u, 0x4000000u);
    SC()[0xB] = 0x10;
    SC()[9] = 0x10;
    SC()[0xA] = 0;
    Bump(SC()[2]);
}

// original 0x4BEE80: the word +0x3E down 0x40; +9 down; at 0, +9 0x20 and on.
MS18_EXPORT void __cdecl DrainOrbB_Drop(void) {
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::Current<records::BattleTask>()->height_whole + 0xFFC0u);
    Drop(SC()[9]);
    if (SC()[9] != 0) return;
    SC()[9] = 0x20;
    Bump(SC()[2]);
}

// original 0x4BEEB0: +9 down to 0; +0xA up; at 0x80, +9 0x20 and on.
MS18_EXPORT void __cdecl DrainOrbB_Run(void) {
    unsigned char* const sc = SC();
    if (sc[9] != 0) sc[9] = static_cast<unsigned char>(sc[9] - 1);
    Bump(SC()[0xA]);
    if (SC()[0xA] != 0x80) return;
    SC()[9] = 0x20;
    Bump(SC()[2]);
}

// original 0x4BEEF0: +9 down to 0; +0xA up; at 0xA0, the owner's +0xB down
// and on.
MS18_EXPORT void __cdecl DrainOrbB_Signal(void) {
    unsigned char* const sc = SC();
    if (sc[9] != 0) sc[9] = static_cast<unsigned char>(sc[9] - 1);
    Bump(SC()[0xA]);
    if (SC()[0xA] != 0xA0) return;
    Drop(Owner()[0xB]);
    Bump(SC()[2]);
}

// original 0x4BEF40: +0xA up, +0xB down; at +0xB 0, freed.
MS18_EXPORT void __cdecl DrainOrbB_Shrink(void) {
    Bump(SC()[0xA]);
    Drop(SC()[0xB]);
    if (SC()[0xB] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BEF70: types 0 and 1's draw. 24 rings (n = 1..24) of eight
// Gouraud quads each, stacked: ring n's radius the scratch word +0 (s16,
// 0x903850), its height -40 n, its x offset (sin(frame + n) * 3 n) >> 12;
// each quad joins ring n - 1 (the previous radius +2, height +6, offset) to
// ring n, coloured by DrainOrb_ShadeA / _ShadeB by (+0xA + n) & 15. The
// radius comes from sin of that index times 4, or (phases 2 and 3, the index
// below 8) times the ring's 7 + 3 (n - 1); in phases 2 and 3 a ring past +9
// (or short of it, by +1 and the phase) collapses to the base radius +0xB * 2
// with ShadeB. Every scratch word is written and read where the original
// does; Frame_Counter and Sprite_Current are read again after each call.
MS18_EXPORT void __cdecl DrainOrb_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    LinkAt(0, 0xC);
    const unsigned angle = (Frame_Counter & 0x3F) << 6;
    SetDs(0xE, SC()[0xB] * 2u);
    SetDs(4, 0);
    int r = Sin(static_cast<int>(angle));
    const int first = MulSar(r, 3, 12);
    unsigned char* sc = SC();
    const unsigned index = sc[0xA] & 0xFu;
    SetDs(0xC, index);
    if (sc[2] > 1) {
        SetDs(8, TableWord(DrainOrb_ShadeA, static_cast<int>(index)));
        r = Sin(static_cast<int>(index << 8));
        SetDs(0, static_cast<unsigned>(ShlSar(r, 2, 12) + Ds(0xE)));
        sc = SC();
        if (sc[1] != 0 && sc[9] != 0) {
            const int i = Ds(0xC);
            SetDs(8, TableWord(DrainOrb_ShadeB, i));
            r = Sin(static_cast<int>(static_cast<std::uint32_t>(i) << 8));
            SetDs(0, static_cast<unsigned>(ShlSar(r, 2, 12) + Ds(0xE)));
        }
    } else {
        SetDs(8, TableWord(DrainOrb_ShadeB, static_cast<int>(index)));
        r = Sin(static_cast<int>(index << 8));
        SetDs(0, static_cast<unsigned>(ShlSar(r, 2, 12) + Ds(0xE)));
    }

    // The two stack words: the previous ring's offset (the first: `first`)
    // and this ring's. Only their low words reach a vertex.
    unsigned prev = static_cast<std::uint16_t>(first);
    for (int n = 1, e = 7; e < 0x4F; ++n, e += 3) {
        const unsigned before = prev;
        const unsigned a = ((Frame_Counter + static_cast<unsigned>(n)) & 0x3F) << 6;
        SetDs(6, static_cast<unsigned>(Ds(4)));
        SetDs(4, static_cast<std::uint32_t>(n) * 0xFFFFFFD8u);   // -40 n
        r = Sin(static_cast<int>(a));
        const unsigned now = static_cast<std::uint16_t>(
            static_cast<int>(static_cast<std::uint32_t>(r) * static_cast<std::uint32_t>(n) * 3u) >> 12);
        {
            const unsigned radius = static_cast<std::uint16_t>(Ds(0)), shade = static_cast<std::uint16_t>(Ds(8));
            SetDs(2, radius);
            SetDs(0xA, shade);
        }
        sc = SC();
        const unsigned i = static_cast<unsigned char>(sc[0xA] + n) & 0xFu;
        SetDs(0xC, i);
        const unsigned phase = sc[2];
        if (phase == 2 || phase == 3) {
            SetDs(8, TableWord(DrainOrb_ShadeA, static_cast<int>(i)));
            r = Sin(static_cast<int>(i << 8));
            const int v = static_cast<short>(i) < 8
                              ? static_cast<int>(static_cast<std::uint32_t>(r) * static_cast<std::uint32_t>(e))
                              : static_cast<int>(static_cast<std::uint32_t>(r) << 2);
            const short base = Ds(0xE);
            SetDs(0, static_cast<unsigned>((v >> 12) + base));
            sc = SC();
            const int c9 = sc[9];
            const bool collapse = (phase == 3) == (sc[1] == 0) ? c9 > n : c9 < n;
            if (collapse) {
                const int j = Ds(0xC);
                SetDs(0, static_cast<unsigned>(base));
                SetDs(8, TableWord(DrainOrb_ShadeB, j));
            }
        } else {
            SetDs(8, TableWord(DrainOrb_ShadeB, static_cast<int>(i)));
            r = Sin(static_cast<int>(i << 8));
            SetDs(0, static_cast<unsigned>(ShlSar(r, 2, 12) + Ds(0xE)));
        }

        r = Cos(0);
        SetVx(8, static_cast<unsigned>(MulSar(r, Ds(0), 12)) + now);
        r = Sin(0);
        SetVx(0xA, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        SetVx(0xC, static_cast<unsigned>(Ds(4)));
        r = Cos(0);
        SetVx(0x18, static_cast<unsigned>(MulSar(r, Ds(2), 12)) + before);
        r = Sin(0);
        SetVx(0x1A, static_cast<unsigned>(MulSar(r, Ds(2), 12)));
        SetVx(0x1C, static_cast<unsigned>(Ds(6)));
        for (int a2 = 0x200; a2 < 0x1200; a2 += 0x200) {
            SetVx(0, static_cast<unsigned>(Vx(8)));
            SetVx(2, static_cast<unsigned>(Vx(0xA)));
            SetVx(4, static_cast<unsigned>(Vx(0xC)));
            SetVx(0x10, static_cast<unsigned>(Vx(0x18)));
            SetVx(0x12, static_cast<unsigned>(Vx(0x1A)));
            SetVx(0x14, static_cast<unsigned>(Vx(0x1C)));
            r = Cos(a2);
            SetVx(8, static_cast<unsigned>(MulSar(r, Ds(0), 12)) + now);
            r = Sin(a2);
            SetVx(0xA, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
            SetVx(0xC, static_cast<unsigned>(Ds(4)));
            r = Cos(a2);
            SetVx(0x18, static_cast<unsigned>(MulSar(r, Ds(2), 12)) + before);
            r = Sin(a2);
            unsigned char* const p = Gfx_PacketNext;
            SetVx(0x1A, static_cast<unsigned>(MulSar(r, Ds(2), 12)));
            SetVx(0x1C, static_cast<unsigned>(Ds(6)));
            MH_CALL(Gpu_SetPolyG4)(p);
            Shade3(p + 4, Ds(8));
            Shade3(p + 0x14, Ds(8));
            Shade3(p + 0x24, Ds(0xA));
            Shade3(p + 0x34, Ds(0xA));
            Rtp4(p);
            MH_CALL(Gte_PrimDepths4_10B)(p);
            LinkAt(0, 0x44);
        }
        prev = now;
    }
}

// original 0x4BF520: type 2 - its four phases, DrainOrb_DrawDisc.
MS18_EXPORT void __cdecl DrainOrbC_Task(void) { OrbTask<&::DrainOrb_DrawDisc>(DrainOrbC_Phases, 4, "DrainOrbC_Task"); }

// original 0x4BF560: at the target's object; +9 4, +0xA 0xB0; on.
MS18_EXPORT void __cdecl DrainOrbC_Place(void) {
    PlaceAt(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)), 0, 0, 0);
    SC()[9] = 4;
    SC()[0xA] = 0xB0;
    Bump(SC()[2]);
}

// original 0x4BF5B0: +0xA down; on when the owner's +0xB (the children still
// running) equals this child's type +1.
MS18_EXPORT void __cdecl DrainOrbC_Wait(void) {
    Drop(SC()[0xA]);
    if (Owner()[0xB] == SC()[1]) Bump(SC()[2]);
}

// original 0x4BF5E0: +0xA and +9 down; at +9 4, the owner's +0xB down and
// freed.
MS18_EXPORT void __cdecl DrainOrbC_Shrink(void) {
    Drop(SC()[0xA]);
    Drop(SC()[9]);
    if (SC()[9] != 4) return;
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BF620: type 3 - its four phases, DrainOrb_DrawDisc.
MS18_EXPORT void __cdecl DrainOrbD_Task(void) { OrbTask<&::DrainOrb_DrawDisc>(DrainOrbD_Phases, 4, "DrainOrbD_Task"); }

// original 0x4BF660: +9 down; at 0, at the actor's object, +9 4, on.
MS18_EXPORT void __cdecl DrainOrbD_Place(void) {
    Drop(SC()[9]);
    if (SC()[9] != 0) return;
    PlaceAt(Pointer(::bof3::Img(MagicS23_ActorSprite_addr)), 0, 0, 0);
    SC()[9] = 4;
    Bump(SC()[2]);
}

// original 0x4BF6B0: +0xA up; on when the owner's +0xB equals +1.
MS18_EXPORT void __cdecl DrainOrbD_Wait(void) {
    Bump(SC()[0xA]);
    if (Owner()[0xB] == SC()[1]) Bump(SC()[2]);
}

// original 0x4BF6E0: +0xA up, +9 down; at +9 4, the owner's +0xB down and
// freed.
MS18_EXPORT void __cdecl DrainOrbD_Shrink(void) {
    Bump(SC()[0xA]);
    Drop(SC()[9]);
    if (SC()[9] != 4) return;
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BF720: types 2 and 3's draw. A flat fan of eight semi-
// transparent Gouraud triangles (tpage 0x55), radius +9 * 16 (the scratch
// word +0), each from the centre to two rim points 0x200 apart; the centre's
// colour DrainOrb_DiscShade[+0xA & 15] with 0xF0 0xF0, the rim's 1 1 1.
MS18_EXPORT void __cdecl DrainOrb_DrawDisc(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetDs(0, SC()[9] << 4);
    int r = Sin(0);
    SetVx(0x10, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
    r = Cos(0);
    SetVx(0x12, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const unsigned x = static_cast<std::uint16_t>(Vx(0x10)), y = static_cast<std::uint16_t>(Vx(0x12));
        SetVx(0, 0);
        SetVx(2, 0);
        SetVx(8, x);
        SetVx(0xA, y);
        r = Sin(a);
        SetVx(0x10, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        r = Cos(a);
        SetVx(0x12, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        SetVx(0x14, 0);
        SetVx(0xC, 0);
        SetVx(4, 0);
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        const unsigned c = TableWord(DrainOrb_DiscShade, SC()[0xA] & 0xF);
        SetDs(8, c);
        p[4] = static_cast<unsigned char>(c);
        p[5] = 0xF0;
        p[6] = 0xF0;
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

void DrainOrbA_Inject() {
    BOF3_INJECT(Drain_Task);
    BOF3_INJECT(Drain_Start);
    BOF3_INJECT(DrainOrb_Dispatch);
    BOF3_INJECT(DrainOrbA_Task);
    BOF3_INJECT(DrainOrbA_Place);
    BOF3_INJECT(DrainOrbA_Drop);
    BOF3_INJECT(DrainOrbA_Run);
    BOF3_INJECT(DrainOrbA_Signal);
    BOF3_INJECT(DrainOrbA_Shrink);
    BOF3_INJECT(DrainOrbB_Task);
    BOF3_INJECT(DrainOrbB_Place);
    BOF3_INJECT(DrainOrbB_Drop);
    BOF3_INJECT(DrainOrbB_Run);
    BOF3_INJECT(DrainOrbB_Signal);
    BOF3_INJECT(DrainOrbB_Shrink);
    BOF3_INJECT(DrainOrb_Draw);
    BOF3_INJECT(DrainOrbC_Task);
    BOF3_INJECT(DrainOrbC_Place);
    BOF3_INJECT(DrainOrbC_Wait);
    BOF3_INJECT(DrainOrbC_Shrink);
    BOF3_INJECT(DrainOrbD_Task);
    BOF3_INJECT(DrainOrbD_Place);
    BOF3_INJECT(DrainOrbD_Wait);
    BOF3_INJECT(DrainOrbD_Shrink);
    BOF3_INJECT(DrainOrb_DrawDisc);
}
