// Two overlays of Magic_Rows, round nine group S18 (docs/magic_s18.md):
//
//   - MAGIC079.EMI, row 52 (the ability ids 0x4F and 0xB7; read one id down,
//     Drain): 0x4BEB50..0x4BF8C3, 25 functions. A kind-2 task that starts
//     four kind-1 children (parameter 0x2B), each of one of four types by its
//     +1, each type a .data phase table and one of two draws.
//   - MAGIC082.EMI, row 32 (the ids 0x22 0x23 0x52 0x54 0x55 0xB8 0xBA 0xBB;
//     read one id down, Steroids, Magic Belt, Protect, Speed, Might):
//     0x4C01F0..0x4C1461, 17 functions. A kind-2 task whose kind (0..3, by the
//     ability id) picks the colours; a ring child and four spike children
//     (kind 1, parameter 5), a CLUT strip copy, a tint fade, and a last child
//     (kind 1, parameter 0x48) whose type depends on 0x4FB6F0.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase, and the .data
// tables read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. No divergence: each is a faithful replacement,
// except that an index past a dispatch table aborts where the original would
// call through the bytes after it (docs/magic_fx_reached.md section 3, the
// precedent), and BuffSpike_Draw aborts where the original would store a
// depth outside its four-entry stack array.
#include "game/magic_s18.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The two blocks' objects (docs/battle_actions.md): the acting block's +8 and
// the target block's +8 (the harness's kSource).
constexpr std::uint32_t kActorObject = 0x904B3C;
constexpr std::uint32_t kTargetObject = 0x904B4C;
// The acting block's command kind, and the action's id (docs/battle_actions.md).
constexpr std::uint32_t kCommandKind = 0x904B35;
constexpr std::uint32_t kActionId = 0x904B80;

// Buff_Start's CLUT strip: 16 words at +0 and 16 at +0x20, copied over.
constexpr std::uint32_t kClutFrom = 0x80E980, kClutTo = 0x812980;
// A byte per kind, Buff_Fade's first argument to 0x4FB6F0 (in the effect
// library's .data, 4 bytes below FxDim_Phases; not this group's to name).
constexpr std::uint32_t kKindByte = 0x65C39C;

// The effect library's (queue group L), by address (docs/magic_s18.md section 5).
constexpr std::uint32_t kLinkDepths = 0x4FB880;   // (x, z, depths[], prims, count, size, slot)
constexpr std::uint32_t kKindTest = 0x4FB6F0;     // (byte, target) -> 0 / not 0
using LinkDepthsFn = void (__cdecl*)(unsigned long, unsigned long, const long*, unsigned char*, unsigned, unsigned, unsigned);
using KindTestFn = unsigned char (__cdecl*)(unsigned, unsigned);

// The GTE calls as the originals push them: one pointer more than
// symbols.toml's prototypes carry (a flag word the callee may write).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                               long*, long*);
using Avg3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);
template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

unsigned char* SC() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Slot(unsigned char k) { return Mem(at::kTasks + k * at::kTaskStride); }
std::int32_t Ptr(const unsigned char* p) { return static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::int32_t AddL(std::int32_t a, std::uint32_t b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + b); }

// The scratch words DamageScratch + k (the PSX scratchpad's) and the vertex
// scratch Prim_VertexScratch + k: three SVECTORs at +0, +8, +0x10, +0x18.
constexpr std::uint32_t kDs = bof3::addr::DamageScratch;
constexpr std::uint32_t kVx = 0x9037A0;   // Prim_VertexScratch
short Ds(unsigned k) { return static_cast<short>(Word(Mem(kDs + k))); }
void SetDs(unsigned k, unsigned v) { SetWord(Mem(kDs + k), v); }
short Vx(unsigned k) { return static_cast<short>(Word(Mem(kVx + k))); }
void SetVx(unsigned k, unsigned v) { SetWord(Mem(kVx + k), v); }
const short* VxP(unsigned k) { return reinterpret_cast<const short*>(Mem(kVx + k)); }

// `imul` (a 32-bit product that wraps) then `sar n`; `shl l` then `sar r`.
int MulSar(int a, int b, int n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}
int ShlSar(int a, int l, int r) { return static_cast<int>(static_cast<std::uint32_t>(a) << l) >> r; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}
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
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(sc + 0x34)), static_cast<unsigned long>(Long(sc + 0x38)),
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
long Avg3(unsigned char* p) {
    long depth = 0, flag = 0;
    return MH_CALL(As<Avg3Fn>(&::Gte_RotAverage3))(VxP(0), VxP(8), VxP(0x10), Fl(p + 8), Fl(p + 0x18), Fl(p + 0x28), &depth,
                                                   &flag);
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
    SetLong(SC() + 0x34, AddL(Long(obj + 0x34), dx));
    SetLong(SC() + 0x38, AddL(Long(obj + 0x38), dz));
    SetLong(SC() + 0x3C, AddL(Long(obj + 0x3C), dy));
}

}  // namespace

#define MS18_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC079 (row 52): the task and its four children

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
        unsigned char* const sc = SC();
        SetLong(t + 0x80, Ptr(sc));
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
    PlaceAt(Pointer(kTargetObject), 0xFFFFC000u, 0xFFFFC000u, 0x4000000u);
    SC()[0xB] = 0x10;
    SC()[9] = 0x10;
    SC()[0xA] = 0xB0;
    Bump(SC()[2]);
}

// original 0x4BECD0: the word +0x3E down 0x40; +9 down; on at 0.
MS18_EXPORT void __cdecl DrainOrbA_Drop(void) {
    SetWord(SC() + 0x3E, Word(SC() + 0x3E) + 0xFFC0u);
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
    PlaceAt(Pointer(kActorObject), 0xFFFFC000u, 0xFFFFC000u, 0x4000000u);
    SC()[0xB] = 0x10;
    SC()[9] = 0x10;
    SC()[0xA] = 0;
    Bump(SC()[2]);
}

// original 0x4BEE80: the word +0x3E down 0x40; +9 down; at 0, +9 0x20 and on.
MS18_EXPORT void __cdecl DrainOrbB_Drop(void) {
    SetWord(SC() + 0x3E, Word(SC() + 0x3E) + 0xFFC0u);
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
    PlaceAt(Pointer(kTargetObject), 0, 0, 0);
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
    PlaceAt(Pointer(kActorObject), 0, 0, 0);
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

// ===========================================================================
// MAGIC082 (row 32): the task, the ring and the four spikes

// original 0x4C01F0: the kind-2 task. Its phase +1 through a six-entry stack
// table: Buff_Start, BattleFx_TintActor, BattleFx_Brighten (round eight's,
// MAGIC062's), Buff_WaitChildren, Buff_Fade, 0x4E5200 (MAGIC131's: the done
// flag and free once +0xB is 0). Unchecked in the original; ours aborts.
MS18_EXPORT void __cdecl Buff_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Buff_Start,        bof3::addr::BattleFx_TintActor,
                                                 bof3::addr::BattleFx_Brighten, bof3::addr::Buff_WaitChildren,
                                                 bof3::addr::Buff_Fade,         0x4E5200};
    CallPhase(kPhases, 6, "Buff_Task");
}

// original 0x4C0240: the target's object's +8 and position to the task; +4
// the kind (Buff_Kind); +0xB 0, +9 0x10, the phase on. Then five children
// BattleTask_Create(1, 5), this task's +0xB counting them: the ring (+1 0,
// +4 the kind, +9 0, the task's position) and four spikes (+1 1, +4 the kind,
// +0xB 0 / 8 / 0x10 / 0x18, +9 that + 1, +0xA the task's +9). The CLUT strip
// 0x80E980 (16 + 16 words) to 0x812980, Gfx_ClutStripDirty 1,
// Sound_PlayById(0x100).
MS18_EXPORT void __cdecl Buff_Start(void) {
    const unsigned char* const src = Pointer(kTargetObject);
    SC()[8] = src[8];
    SetLong(SC() + 0x34, Long(src + 0x34));
    SetLong(SC() + 0x38, Long(src + 0x38));
    SetLong(SC() + 0x3C, Long(src + 0x3C));
    const unsigned char kind = MH_CALL(Buff_Kind)();
    SC()[4] = kind;
    SC()[0xB] = 0;
    SC()[9] = 0x10;
    Bump(SC()[1]);
    {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 5));
        unsigned char* const sc = SC();
        SetLong(t + 0x80, Ptr(sc));
        t[1] = 0;
        t[4] = sc[4];
        t[9] = 0;
        SetLong(t + 0x34, Long(sc + 0x34));
        SetLong(t + 0x38, Long(sc + 0x38));
        SetLong(t + 0x3C, Long(sc + 0x3C));
        Bump(sc[0xB]);
    }
    for (unsigned b = 0; b < 0x20; b += 8) {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 5));
        unsigned char* const sc = SC();
        SetLong(t + 0x80, Ptr(sc));
        t[1] = 1;
        t[4] = sc[4];
        t[0xB] = static_cast<unsigned char>(b);
        t[9] = static_cast<unsigned char>(b + 1);
        t[0xA] = sc[9];
        Bump(sc[0xB]);
    }
    for (unsigned i = 0; i < 0x20; i += 2) {
        SetWord(Mem(kClutTo + i), Word(Mem(kClutFrom + i)));
        SetWord(Mem(kClutTo + 0x20 + i), Word(Mem(kClutFrom + 0x20 + i)));
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C03B0: once +0xB (the children running) is 1 or 0, +9 8 and on.
MS18_EXPORT void __cdecl Buff_WaitChildren(void) {
    if (SC()[0xB] > 1) return;
    SC()[9] = 8;
    Bump(SC()[1]);
}

// original 0x4C03D0: the tint record MoveScript_TintRecords[+0xA]'s three
// colour bytes (+2, +3, +4) down one; +9 down. At 0: Sprite_ReleaseTint on
// the target's object, BattleActor_Flash(the target), and a last child
// BattleTask_Create(1, 0x48) with +4 the kind when 0x4FB6F0(the kind's byte
// of 0x65C39C, the target) answers non-zero, else 8; +9 1, +0xA 0; +0xB up;
// the phase on.
MS18_EXPORT void __cdecl Buff_Fade(void) {
    unsigned char* const sc = SC();
    unsigned char* const tints = MoveScript_TintRecords;
    Drop(tints[sc[0xA] * 12u + 2]);
    Drop(tints[sc[0xA] * 12u + 3]);
    Drop(tints[sc[0xA] * 12u + 4]);
    Drop(sc[9]);
    if (SC()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(kTargetObject));
    MH_CALL(BattleActor_Flash)(Mem(at::kTarget)[0]);
    const unsigned char target = Mem(at::kTarget)[0];
    const unsigned char byte = Mem(kKindByte)[SC()[4]];
    const unsigned char suits = MH_AT(KindTestFn, kKindTest)(byte, target);
    unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x48));
    unsigned char* const owner = SC();
    SetLong(t + 0x80, Ptr(owner));
    t[4] = suits != 0 ? owner[4] : 8;
    t[9] = 1;
    t[0xA] = 0;
    Bump(owner[0xB]);
    Bump(SC()[1]);
}

// original 0x4C04F0: the effect's kind by the action. The acting block's
// command kind 4 (an ability): by the id 0x904B80, 0x52 / 0xB8 answer 0,
// 0x54 / 0xBA 1, 0x22 / 0x23 / 0x55 / 0xBB 2, any other 3 (a jump table of
// four cases by a byte table over 0x22..0xBB). Any other kind: 0x10B becomes
// 0x52 and answers 0, 0x211 becomes 0x54 and 1, 0x14 and 0x117 become 0x55
// and 2, any other 3. Only the low word of 0x904B80 is read.
MS18_EXPORT unsigned char __cdecl Buff_Kind(void) {
    const unsigned id = Word(Mem(kActionId));
    if (Mem(kCommandKind)[0] == 4) {
        switch (id) {
        case 0x52: case 0xB8: return 0;
        case 0x54: case 0xBA: return 1;
        case 0x22: case 0x23: case 0x55: case 0xBB: return 2;
        default: return 3;
        }
    }
    switch (id) {
    case 0x10B: SetWord(Mem(kActionId), 0x52); return 0;
    case 0x211: SetWord(Mem(kActionId), 0x54); return 1;
    case 0x14: case 0x117: SetWord(Mem(kActionId), 0x55); return 2;
    default: return 3;
    }
}

// original 0x4C0620: the children (kind 1, parameter 5): jmp [BuffFx_Types +
// 4 * +1] - the ring, the spikes.
MS18_EXPORT void __cdecl BuffFx_Dispatch(void) { CallCell(BuffFx_Types, 2, SC()[1], "BuffFx_Dispatch"); }

// original 0x4C0640: the ring - its three phases (0x4C2D10 MAGIC086's,
// BuffRing_Wait, 0x4B1740 MAGIC060's); while live (+0), the band and the disc
// under the actor matrix.
MS18_EXPORT void __cdecl BuffRing_Task(void) {
    CallCell(BuffRing_Phases, 3, SC()[2], "BuffRing_Task");
    if (SC()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(BuffRing_DrawBand)();
    MH_CALL(BuffRing_DrawDisc)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C0680: on once the owner's +0xB is 2 or less.
MS18_EXPORT void __cdecl BuffRing_Wait(void) {
    if (Owner()[0xB] <= 2) Bump(SC()[2]);
}

// original 0x4C06A0: a flat disc of sixteen semi-transparent Gouraud
// triangles (tpage 0x35), radius 0x80 (sin / cos << 7 >> 12); the centre
// +9 * 5 grey, the rim BuffRing_DiscColors[+4] times +9.
MS18_EXPORT void __cdecl BuffRing_DrawDisc(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const sc = SC();
        const unsigned char* const rgb = BuffRing_DiscColors + sc[4] * 3u;
        SetDs(6, sc[9] * 5u);
        SetDs(8, rgb[0] * static_cast<unsigned>(sc[9]));
        SetDs(0xA, rgb[1] * static_cast<unsigned>(sc[9]));
        SetDs(0xC, rgb[2] * static_cast<unsigned>(sc[9]));
    }
    int r = Sin(0);
    SetVx(0x10, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Cos(0);
    SetVx(0x12, static_cast<unsigned>(ShlSar(r, 7, 12)));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        const unsigned x = static_cast<std::uint16_t>(Vx(0x10)), y = static_cast<std::uint16_t>(Vx(0x12));
        SetVx(0, 0);
        SetVx(2, 0);
        SetVx(8, x);
        SetVx(0xA, y);
        r = Sin(a);
        SetVx(0x10, static_cast<unsigned>(ShlSar(r, 7, 12)));
        r = Cos(a);
        unsigned char* const p = Gfx_PacketNext;
        SetVx(0x14, 0);
        SetVx(0x12, static_cast<unsigned>(ShlSar(r, 7, 12)));
        SetVx(0xC, 0);
        SetVx(4, 0);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = p[5] = p[6] = static_cast<unsigned char>(Ds(6));
        for (unsigned k : {0x14u, 0x24u}) {
            p[k] = static_cast<unsigned char>(Ds(8));
            p[k + 1] = static_cast<unsigned char>(Ds(0xA));
            p[k + 2] = static_cast<unsigned char>(Ds(0xC));
        }
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4C08A0: a flat band of sixteen semi-transparent Gouraud quads
// (tpage 0x35) from radius 0x80 to 0x100; the inner edge
// BuffRing_BandColors[+4] times +9, the outer 1 1 1.
MS18_EXPORT void __cdecl BuffRing_DrawBand(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const sc = SC();
        const unsigned char* const rgb = BuffRing_BandColors + sc[4] * 3u;
        SetDs(8, rgb[0] * static_cast<unsigned>(sc[9]));
        SetDs(0xA, rgb[1] * static_cast<unsigned>(sc[9]));
        SetDs(0xC, rgb[2] * static_cast<unsigned>(sc[9]));
    }
    int r = Sin(0);
    SetVx(8, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Cos(0);
    SetVx(0xA, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Sin(0);
    SetVx(0x18, static_cast<unsigned>(ShlSar(r, 8, 12)));
    r = Cos(0);
    SetVx(0x1A, static_cast<unsigned>(ShlSar(r, 8, 12)));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        SetVx(0, static_cast<unsigned>(Vx(8)));
        SetVx(2, static_cast<unsigned>(Vx(0xA)));
        SetVx(0x10, static_cast<unsigned>(Vx(0x18)));
        SetVx(0x12, static_cast<unsigned>(Vx(0x1A)));
        r = Sin(a);
        SetVx(8, static_cast<unsigned>(ShlSar(r, 7, 12)));
        r = Cos(a);
        SetVx(0xA, static_cast<unsigned>(ShlSar(r, 7, 12)));
        r = Sin(a);
        SetVx(0x18, static_cast<unsigned>(ShlSar(r, 8, 12)));
        r = Cos(a);
        unsigned char* const p = Gfx_PacketNext;
        SetVx(0x1C, 0);
        SetVx(0x1A, static_cast<unsigned>(ShlSar(r, 8, 12)));
        SetVx(0x14, 0);
        SetVx(0xC, 0);
        SetVx(4, 0);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 0x14u}) {
            p[k] = static_cast<unsigned char>(Ds(8));
            p[k + 1] = static_cast<unsigned char>(Ds(0xA));
            p[k + 2] = static_cast<unsigned char>(Ds(0xC));
        }
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4C0AF0: a spike - jmp [BuffSpike_Phases + 4 * +2] (six:
// 0x4BDD20 MAGIC078's, 0x4C1EB0 MAGIC083's, then the four below).
MS18_EXPORT void __cdecl BuffSpike_Dispatch(void) { CallCell(BuffSpike_Phases, 6, SC()[2], "BuffSpike_Dispatch"); }

namespace {

// The spikes' shared frame: the actor's screen point, a disc of radius
// `base` + Rand() & 3 (MagicFx_DrawDiscRadius), and the spike itself under the
// actor matrix.
void SpikeFrame(unsigned base) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    const int r = MH_CALL(Rand)();
    MH_CALL(MagicFx_DrawDiscRadius)(static_cast<unsigned>((r & 3) + static_cast<int>(base)));
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(BuffSpike_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// +9 up on odd frames; the speed +0x14 plus the acceleration +0x20, and the
// height word +0x3E plus the speed's low word; +0xA down, answering whether
// it reached 0.
bool SpikeFly() {
    if (Frame_Counter & 1) Bump(SC()[9]);
    unsigned char* sc = SC();
    SetLong(sc + 0x14, AddL(Long(sc + 0x14), static_cast<std::uint32_t>(Long(sc + 0x20))));
    sc = SC();
    SetWord(sc + 0x3E, Word(sc + 0x3E) + Word(sc + 0x14));
    Drop(SC()[0xA]);
    return SC()[0xA] == 0;
}

}  // namespace

// original 0x4C0B10: the spike's frame (disc 0x18); flies; at +0xA 0, the
// speed -16, the acceleration 4, +0xA 8, on.
MS18_EXPORT void __cdecl BuffSpike_Arc(void) {
    SpikeFrame(0x18);
    if (!SpikeFly()) return;
    SetLong(SC() + 0x14, -16);
    SetLong(SC() + 0x20, 4);
    SC()[0xA] = 8;
    Bump(SC()[2]);
}

// original 0x4C0BB0: the same frame and flight; at +0xA 0, +0xA 0x1C - +0xB
// and on.
MS18_EXPORT void __cdecl BuffSpike_Arc2(void) {
    SpikeFrame(0x18);
    if (!SpikeFly()) return;
    unsigned char* const sc = SC();
    sc[0xA] = static_cast<unsigned char>(0x1C - sc[0xB]);
    Bump(SC()[2]);
}

// original 0x4C0C40: the frame (disc 0x40 once the spin +0xC is 4 and +0xA
// below 5, else 0x18); the spin up one every eighth frame of +0x10 until 4;
// +9 (the angle) plus the spin. At spin 4, +0xA down; at 0,
// Sound_PlayById(0x101) for the first spike (+0xB 0), +0x10 +0x14 +0x20 8,
// on.
MS18_EXPORT void __cdecl BuffSpike_Spin(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* sc = SC();
    const bool wide = Long(sc + 0xC) == 4 && sc[0xA] < 5;
    const int r = MH_CALL(Rand)();
    MH_CALL(MagicFx_DrawDiscRadius)(static_cast<unsigned>((r & 3) + (wide ? 0x40 : 0x18)));
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(BuffSpike_Draw)();
    MH_CALL(Gte_PopMatrix)();
    sc = SC();
    if (Long(sc + 0xC) < 4) {
        SetLong(sc + 0x10, AddL(Long(sc + 0x10), 1));
        sc = SC();
        if ((sc[0x10] & 7) == 0) {
            SetLong(sc + 0xC, AddL(Long(sc + 0xC), 1));
            sc = SC();
        }
    }
    sc[9] = static_cast<unsigned char>(sc[9] + sc[0xC]);
    sc = SC();
    if (Long(sc + 0xC) != 4) return;
    Drop(sc[0xA]);
    sc = SC();
    if (sc[0xA] != 0) return;
    if (sc[0xB] == 0) {
        MH_CALL(Sound_PlayById)(0x101);
        sc = SC();
    }
    SetLong(sc + 0x10, 8);
    SetLong(SC() + 0x14, 8);
    SetLong(SC() + 0x20, 8);
    Bump(SC()[2]);
}

// original 0x4C0D20: the frame (disc 0x20); +0x10 up 4; +9 plus the spin; +0xB
// up 2; the spike at the owner's x / z plus sin / cos((+0xB & 31) << 7) times
// (+0xA * +0x10 + 0xB0) >> 3; +0xA up; at 0x10, the owner's +0xB down and
// freed.
MS18_EXPORT void __cdecl BuffSpike_Orbit(void) {
    SpikeFrame(0x20);
    unsigned char* sc = SC();
    SetLong(sc + 0x10, AddL(Long(sc + 0x10), 4));
    sc = SC();
    sc[9] = static_cast<unsigned char>(sc[9] + sc[0xC]);
    sc = SC();
    sc[0xB] = static_cast<unsigned char>(sc[0xB] + 2);
    sc = SC();
    SetDs(4, (sc[0xB] & 0x1Fu) << 7);
    SetDs(0, sc[0xA] * static_cast<unsigned>(Word(sc + 0x10)) + 0xB0u);
    int r = Sin(Ds(4));
    SetLong(SC() + 0x34, AddL(static_cast<std::int32_t>(MulSar(r, Ds(0), 3)), static_cast<std::uint32_t>(Long(Owner() + 0x34))));
    r = Cos(Ds(4));
    SetLong(SC() + 0x38, AddL(static_cast<std::int32_t>(MulSar(r, Ds(0), 3)), static_cast<std::uint32_t>(Long(Owner() + 0x38))));
    Bump(SC()[0xA]);
    if (SC()[0xA] != 0x10) return;
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// One of BuffSpike_Draw's two rows: four flat triangles from +9 to +9 + 0x20
// by 8, each from the centre to two points of radius 0x30 at angles
// (a & 31) << 7 and ((a + 8) & 31) << 7, the centre `height` above (the
// first row +0x50, the second the scratch word +2 negated), coloured by
// `colors`[3 * (i + 4 * +4)]; each triangle's depth into `depths` and the
// packet cursor moved past it (0x34).
void SpikeRow(const unsigned char* colors, bool first, long (&depths)[4]) {
    unsigned char* sc = SC();
    int a = sc[9];
    if (a >= sc[9] + 0x20) return;
    if (first) {
        SetDs(2, 0x50);
        SetDs(0, 0x30);
    }
    do {
        const int i = (a - sc[9]) / 8;
        SetDs(0xE, static_cast<unsigned>(i));
        const unsigned char* const rgb = colors + 3 * (static_cast<short>(i) + 4 * sc[4]);
        SetDs(8, rgb[0]);
        SetDs(0xA, rgb[1]);
        const unsigned height = first ? 0x50u : static_cast<unsigned>(-static_cast<int>(Ds(2)));
        SetVx(0, 0);
        SetVx(2, 0);
        SetDs(0xC, rgb[2]);
        SetDs(4, (static_cast<unsigned>(a) & 0x1Fu) << 7);
        SetVx(4, height);
        int r = Sin(Ds(4));
        SetVx(8, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        r = Cos(Ds(4));
        SetVx(0xC, 0);
        SetVx(0xA, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        SetDs(4, ((static_cast<unsigned>(a) + 8) & 0x1Fu) << 7);
        r = Sin(Ds(4));
        SetVx(0x10, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        r = Cos(Ds(4));
        unsigned char* const p = Gfx_PacketNext;
        SetVx(0x14, 0);
        SetVx(0x12, static_cast<unsigned>(MulSar(r, Ds(0), 12)));
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 0);
        const long depth = Avg3(p);
        const int k = (a - SC()[9]) / 8;
        a += 8;
        // The original stores into its four-entry stack array by this index,
        // unchecked; nothing between the reads moves +9 in the game.
        if (k < 0 || k > 3) bof3::Fatal("BuffSpike_Draw: depth index %d, past the four-entry array", k);
        depths[k] = depth;
        for (unsigned q : {4u, 0x14u, 0x24u}) {
            p[q] = static_cast<unsigned char>(Ds(8));
            p[q + 1] = static_cast<unsigned char>(Ds(0xA));
            p[q + 2] = static_cast<unsigned char>(Ds(0xC));
        }
        Gfx_PacketNext = Gfx_PacketNext + 0x34;
        sc = SC();
    } while (a < sc[9] + 0x20);
}

}  // namespace

// original 0x4C0E30: a spike (tpage 0x15, linked at the task's x / z, +2,
// 0xC), two rows of four triangles - up from BuffSpike_ColorsUp, down from
// BuffSpike_ColorsDown - each row's four depths and first packet to
// 0x4FB880(x, z, depths, packets, 4, 0x34, 2), the effect library's.
MS18_EXPORT void __cdecl BuffSpike_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    LinkAt(2, 0xC);
    long depths[4] = {};
    unsigned char* const up = Gfx_PacketNext;
    SpikeRow(BuffSpike_ColorsUp, true, depths);
    {
        const unsigned char* const sc = SC();
        MH_AT(LinkDepthsFn, kLinkDepths)(static_cast<unsigned long>(Long(sc + 0x34)), static_cast<unsigned long>(Long(sc + 0x38)),
                                         depths, up, 4, 0x34, 2);
    }
    unsigned char* const down = Gfx_PacketNext;
    SpikeRow(BuffSpike_ColorsDown, false, depths);
    const unsigned char* const sc = SC();
    MH_AT(LinkDepthsFn, kLinkDepths)(static_cast<unsigned long>(Long(sc + 0x34)), static_cast<unsigned long>(Long(sc + 0x38)),
                                     depths, down, 4, 0x34, 2);
}

// original 0x4C12F0 (also reached from MAGIC083): a flat disc of eight
// semi-transparent Gouraud triangles on screen (tpage 0x35), at the task's
// screen point (+0x2E / +0x30), radius the argument's low word; the centre
// 0x80 grey, the rim 1 1 1; each linked at the task's x / z, +2 (0xC for the
// mode, 0x34 for the triangle). MagicFx_DrawDisc's shape with the radius
// passed in.
MS18_EXPORT void __cdecl MagicFx_DrawDiscRadius(unsigned radius) {
    SetDs(0, radius);
    int a = 0;
    do {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        LinkAt(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, static_cast<short>(Word(SC() + 0x2E)));
        PutFloat(p + 0xC, static_cast<short>(Word(SC() + 0x30)));
        int r = Sin(a);
        PutFloat(p + 0x18, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(static_cast<short>(Word(SC() + 0x2E)))));
        r = Cos(a);
        PutFloat(p + 0x1C, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(static_cast<short>(Word(SC() + 0x30)))));
        a += 0x200;
        r = Sin(a);
        PutFloat(p + 0x28, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(static_cast<short>(Word(SC() + 0x2E)))));
        r = Cos(a);
        PutFloat(p + 0x2C, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(static_cast<short>(Word(SC() + 0x30)))));
        p[4] = p[5] = p[6] = 0x80;
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        LinkAt(2, 0x34);
    } while (a < 0x1000);
}

void MagicS18_Inject() {
    if (bof3::WantsShadow("magic_s18")) magic_s18::SelfTest();
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
    BOF3_INJECT(Buff_Task);
    BOF3_INJECT(Buff_Start);
    BOF3_INJECT(Buff_WaitChildren);
    BOF3_INJECT(Buff_Fade);
    BOF3_INJECT(Buff_Kind);
    BOF3_INJECT(BuffFx_Dispatch);
    BOF3_INJECT(BuffRing_Task);
    BOF3_INJECT(BuffRing_Wait);
    BOF3_INJECT(BuffRing_DrawDisc);
    BOF3_INJECT(BuffRing_DrawBand);
    BOF3_INJECT(BuffSpike_Dispatch);
    BOF3_INJECT(BuffSpike_Arc);
    BOF3_INJECT(BuffSpike_Arc2);
    BOF3_INJECT(BuffSpike_Spin);
    BOF3_INJECT(BuffSpike_Orbit);
    BOF3_INJECT(BuffSpike_Draw);
    BOF3_INJECT(MagicFx_DrawDiscRadius);
}
