// Moved here by tools/move_funcs.py from magic_s17.cpp (docs/source-layout-round.md).
#include "game/magic/leech_shell.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

constexpr ::bof3::image::Va kScratchA{SharedScratch_addr}; // SharedScratch (the PSX scratchpad): radius, angle words

constexpr ::bof3::image::Va kScratchB{0x903854};

using SortFn = void (__cdecl*)(std::uint32_t, std::uint32_t, long*, unsigned char*, unsigned, unsigned, unsigned);

void Bump(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

unsigned char* Task(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::int32_t Table(std::uint32_t at) { return Long(Mem(at)); }

short* V(unsigned k) { return reinterpret_cast<short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

unsigned char* VB(unsigned k) { return Mem(::bof3::Img(Prim_VertexScratch_addr) + k); }

float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }

// A .data table's entry, read in place, the index unchecked (as the
// originals' `jmp / call [eax*4 + table]`).
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// `imul` then `sar 0xC`: a 32-bit product that wraps, shifted arithmetically.
int M(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

int Wrap(std::uint32_t v) { return static_cast<int>(v); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

std::int32_t R() { return Long(Mem(::bof3::Img(kScratchA.v))); }

std::int32_t H() { return Long(Mem(::bof3::Img(kScratchB.v))); }

// Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, tpage, 0) and its commit.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

void LinkAtActor(int dy, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z), dy,
                                size);
}

void ModeCommit4() {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(4, 0xC);
}

// The target's done flag, 0x904AA8 bit 2, and the task freed: every end of
// an effect here.
void TargetDone() {
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

}  // namespace

#define MS17_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The shell's vertex for latitude `lat`, longitude `lon`: height cos(lat) *
// (+9 * 7) (dword 0x903854), and sin(lat) * (+9 * 10) (dword 0x903850) round
// by cos / sin(lon) - the calls in the original's order: cos(lat), then
// sin(lat) and cos(lon), then sin(lat) and sin(lon).
void ShellVertex(unsigned k, int lat, int lon) {
    SetWord(VB(k), static_cast<unsigned>(M(Cos(lat), H())));
    int s = Sin(lat);
    int t = M(s, R());
    int c = Cos(lon);
    SetWord(VB(k + 2), static_cast<unsigned>(M(t, c)));
    s = Sin(lat);
    t = M(s, R());
    c = Sin(lon);
    SetWord(VB(k + 4), static_cast<unsigned>(M(t, c)));
}

void CopyVertex(unsigned to, unsigned from) {
    const unsigned x = Word(VB(from)), y = Word(VB(from + 2)), z = Word(VB(from + 4));
    SetWord(VB(to), x);
    SetWord(VB(to + 2), y);
    SetWord(VB(to + 4), z);
}

}  // namespace

// original 0x4BDAC0: the kind-2 task. Between two draw modes (tpage 0x35,
// committed to slot 4) its phase +1 through a two-entry table on its stack -
// Leech_Start, Leech_WaitOrbs; 2..255 would call through the original's
// stack, ours aborts.
MS17_EXPORT void __cdecl Leech_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Leech_Start, bof3::addr::Leech_WaitOrbs};
    ModeCommit4();
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Leech_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    ModeCommit4();
}

// original 0x4BDB20: two children (kind 1, parameter 0xD): the shell (+1 0,
// this task its owner, at the owner 0x2000000 higher) and the orb (+1 1, the
// shell its owner and this task its +0x4C, at the source 0x904B4C - read
// before either - 0x1000000 higher), each +9 0; sound 0x100; +0xB 0; on.
MS17_EXPORT void __cdecl Leech_Start(void) {
    unsigned char* const src = Source();
    const unsigned s1 = MH_CALL(BattleTask_Create)(1, 0xD) & 0xFF;
    unsigned char* const shell = Task(s1);
    records::BattleTask* const tshell = rv::As<records::BattleTask>(shell);
    tshell->owner = static_cast<std::int32_t>(Key(Sprite_Current));
    unsigned char* const owner = Owner();
    shell[1] = 0;
    shell[9] = 0;
    tshell->x = rv::As<records::SpriteHead>(owner)->x;
    tshell->z = rv::As<records::SpriteHead>(owner)->z;
    rv::Store(tshell->height, Wrap(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(owner)->height) + 0x2000000u));
    const unsigned s2 = MH_CALL(BattleTask_Create)(1, 0xD) & 0xFF;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const orb = Task(s2);
    records::BattleTask* const torb = rv::As<records::BattleTask>(orb);
    torb->owner = static_cast<std::int32_t>(Key(shell));
    rv::Store<std::int32_t>(torb->anim_data, static_cast<std::int32_t>(Key(sc)));
    orb[1] = 1;
    orb[9] = 0;
    torb->x = rv::As<records::SpriteHead>(src)->x;
    torb->z = rv::As<records::SpriteHead>(src)->z;
    rv::Store(torb->height, Wrap(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->height) + 0x1000000u));
    MH_CALL(Sound_PlayById)(0x100);
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4BDC10: held until +0xB is 0xFF (the shell's end); then the
// target's done flag and the task freed (a tail jmp).
MS17_EXPORT void __cdecl Leech_WaitOrbs(void) {
    if (Sprite_Current[0xB] != 0xFF) return;
    TargetDone();
}

// original 0x4BDC40: a child (kind 1, parameter 0xD): a jmp through
// LeechOrb_Types 0x65B1DC by +1, read in place (LeechShell_Task,
// LeechOrb_Task).
MS17_EXPORT void __cdecl LeechOrb_Dispatch(void) { Entry(::bof3::Img(LeechOrb_Types_addr), Sprite_Current[1])(); }

// original 0x4BDC60: the shell. Between two draw modes (tpage 0x35, slot 4):
// before phase 3 its spin +0xC from the frame counter ((low byte & 0x3F) <<
// 6, or 0x1000 at 0x40); a call through LeechShell_Phases 0x65B1E4 by +2,
// read in place (seven entries); while the owner's +0xB is not 0xFF, the
// shell drawn under its own matrix.
MS17_EXPORT void __cdecl LeechShell_Task(void) {
    ModeCommit4();
    unsigned char* sc = Sprite_Current;
    if (sc[2] < 3) {
        const unsigned frame = Frame_Counter;
        rv::Store(rv::As<records::BattleTask>(sc)->step_x, (frame & 0xFF) == 0x40 ? 0x1000 : static_cast<std::int32_t>((frame & 0x3F) << 6));
        sc = Sprite_Current;
    }
    Entry(::bof3::Img(LeechShell_Phases_addr), sc[2])();
    if (Owner()[0xB] != 0xFF) {
        MH_CALL(LeechShell_PushMatrix)();
        MH_CALL(LeechShell_Draw)();
        MH_CALL(Gte_PopMatrix)();
    }
    ModeCommit4();
}

// original 0x4BDD00: +9 up to 12; there +0xA 15 and +2 on.
MS17_EXPORT void __cdecl LeechShell_Grow(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0xC) {
        sc[0xA] = 0xF;
        Bump(Sprite_Current[2]);
        return;
    }
    Bump(sc[9]);
}

// original 0x4BDD20 (reached from 17 files): +0xA down; at 0, +2 on.
MS17_EXPORT void __cdecl MagicFx_WaitA(void) {
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] == 0) Bump(Sprite_Current[2]);
}

// original 0x4BDD40: held until the spin faces the source: Math_Ratan2(dz,
// dx) of the source's position less the shell's (each as a float), >> 6 &
// 0x3F, equal to +0xC / 64 (signed); then +0xA 15, +2 on.
MS17_EXPORT void __cdecl LeechShell_Aim(void) {
    const unsigned char* const src = Source();
    const unsigned char* sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    const int dx = Wrap(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->x) - static_cast<std::uint32_t>(tsc->x));
    const int dz = Wrap(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(src)->z) - static_cast<std::uint32_t>(tsc->z));
    const int r = MH_CALL(Math_Ratan2)(static_cast<float>(dz), static_cast<float>(dx));
    unsigned char* const now = Sprite_Current;
    records::BattleTask* const tnow = rv::As<records::BattleTask>(now);
    if (tnow->step_x / 64 != ((r >> 6) & 0x3F)) return;
    now[0xA] = 0xF;
    Bump(Sprite_Current[2]);
}

// original 0x4BDDB0: +0xA down; at 0 the owner's +0xB 1 (the orb may go),
// +2 on.
MS17_EXPORT void __cdecl LeechShell_Hold(void) {
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    Owner()[0xB] = 1;
    Bump(Sprite_Current[2]);
}

// original 0x4BDDE0: held until the owner's +0xB is 2 (the orb merged); then
// +0xA 0, +2 on.
MS17_EXPORT void __cdecl LeechShell_WaitOrb(void) {
    if (Owner()[0xB] != 2) return;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4BDE00: +0xA up 8; past 0x80 (unsigned), +2 on.
MS17_EXPORT void __cdecl LeechShell_Fade(void) {
    Bump(Sprite_Current[0xA], 8);
    if (Sprite_Current[0xA] > 0x80) Bump(Sprite_Current[2]);
}

// original 0x4BDE20: +9 down; at 0 the owner's +0xB 0xFF and the shell freed
// (a tail jmp).
MS17_EXPORT void __cdecl LeechShell_End(void) {
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BDE50: the shell. A draw mode (tpage 0xB5) sorted at the
// shell; radius +9 * 10 and height +9 * 7 (dwords 0x903850 / 0x903854).
// Two bands of six semi-transparent quads (latitudes from
// LeechShell_Angles +0x1C..+0x24, longitudes from its first six dwords, the
// first edge at 0x180), each outlined by a line quad (LeechShell_Edge) written 0x380 past the
// first quad; blue (0x10, 0x10, 0x80), the one quad of the band whose
// number is frame >> 2 & 7 lit in phase 3, fading by +0xA from phase 5. Then
// two caps of two quads (latitudes +0x1C and +0x24) the same way (1, 1,
// 0x80). The 16 quads, then the 12 lines, sorted by their depths (0x4FB880),
// and Gfx_PacketNext left past the lines.
MS17_EXPORT void __cdecl LeechShell_Draw(void) {
    DrawMode(0xB5);
    LinkAtActor(2, 0xC);
    const unsigned char* sc = Sprite_Current;
    SetLong(Mem(::bof3::Img(kScratchA.v)), sc[9] * 10);
    SetLong(Mem(::bof3::Img(kScratchB.v)), sc[9] * 7);
    unsigned char* const first = Gfx_PacketNext;
    unsigned char* const line_base = first + 0x380;
    unsigned char* lines = line_base;
    long depths[16];
    long line_depths[12];
    long projected;
    unsigned d = 0;
    for (std::uint32_t lat = ::bof3::Img(LeechShell_Angles_addr) + 0x20; lat < ::bof3::Img(LeechShell_Angles_addr) + 0x28; lat += 4) {
        // the band's first edge, at longitude 0x180
        SetWord(VB(8), static_cast<unsigned>(M(Cos(Table(lat - 4)), H())));
        int s = Sin(Table(lat - 4));
        int t = M(s, R());
        int c = Cos(0x180);
        SetWord(VB(0xA), static_cast<unsigned>(M(t, c)));
        s = Sin(Table(lat - 4));
        t = M(s, R());
        c = Sin(0x180);
        SetWord(VB(0xC), static_cast<unsigned>(M(t, c)));
        SetWord(VB(0x18), static_cast<unsigned>(M(Cos(Table(lat)), H())));
        s = Sin(Table(lat));
        t = M(s, R());
        c = Cos(0x180);
        SetWord(VB(0x1A), static_cast<unsigned>(M(t, c)));
        s = Sin(Table(lat));
        t = M(s, R());
        c = Sin(0x180);
        SetWord(VB(0x1C), static_cast<unsigned>(M(t, c)));
        unsigned number = 1;
        for (std::uint32_t lon = ::bof3::Img(LeechShell_Angles_addr) + 4; lon < ::bof3::Img(LeechShell_Angles_addr) + 0x1C; lon += 4, ++number, ++d) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            CopyVertex(0, 8);
            ShellVertex(8, Table(lat - 4), Table(lon));
            CopyVertex(0x10, 0x18);
            ShellVertex(0x18, Table(lat), Table(lon));
            MH_CALL(LeechShell_Edge)(lines, &line_depths[d], Long(VB(0)), Long(VB(4)), Long(VB(8)), Long(VB(0xC)),
                                     Long(VB(0x18)), Long(VB(0x1C)), Long(VB(0x10)), Long(VB(0x14)));
            lines += 0x38;
            depths[d] = MH_CALL(Gte_RotAverage4)(V(0), V(8), V(0x10), V(0x18), F(p + 8), F(p + 0x14), F(p + 0x20),
                                                 F(p + 0x2C), &projected);
            p[4] = 0x10;
            p[5] = 0x10;
            p[6] = 0x80;
            sc = Sprite_Current;
            if (sc[2] == 3 && ((Frame_Counter >> 2) & 7) == number) {
                p[6] = 0x40;
                p[4] = 0x30;
                p[5] = 0x30;
                sc = Sprite_Current;
            }
            if (sc[2] >= 5) {
                const unsigned char fade = sc[0xA];
                p[5] = 0x10;
                p[4] = static_cast<unsigned char>(fade + 0x10);
                p[6] = static_cast<unsigned char>(0x80 - Sprite_Current[0xA]);
            }
            Gfx_PacketNext = Gfx_PacketNext + 0x38;
        }
    }
    unsigned base = 0;
    for (std::uint32_t lat = ::bof3::Img(LeechShell_Angles_addr) + 0x1C; lat < ::bof3::Img(LeechShell_Angles_addr) + 0x2C; lat += 8, base += 2) {
        unsigned n = 0;
        for (std::uint32_t lon = ::bof3::Img(LeechShell_Angles_addr) + 0xC; lon < ::bof3::Img(LeechShell_Angles_addr) + 0x24; lon += 0xC, ++n) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            ShellVertex(0, Table(lat), Table(lon - 0xC));
            ShellVertex(8, Table(lat), Table(lon));
            ShellVertex(0x10, Table(lat), Table(lon - 8));
            ShellVertex(0x18, Table(lat), Table(lon - 4));
            depths[12 + base + n] = MH_CALL(Gte_RotAverage4)(V(0), V(8), V(0x10), V(0x18), F(p + 8), F(p + 0x14),
                                                            F(p + 0x20), F(p + 0x2C), &projected);
            p[4] = 1;
            p[5] = 1;
            p[6] = 0x80;
            sc = Sprite_Current;
            if (sc[2] >= 5) {
                const unsigned char fade = sc[0xA];
                p[5] = 1;
                p[4] = static_cast<unsigned char>(fade + 1);
                p[6] = static_cast<unsigned char>(0x80 - Sprite_Current[0xA]);
            }
            Gfx_PacketNext = Gfx_PacketNext + 0x38;
        }
    }
    sc = Sprite_Current;
    MH_AT(SortFn, bof3::addr::MagicFx_LinkByDepth)(static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->x),
                                                   static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->z), depths, first, 0x10,
                                                   0x38, 2);
    sc = Sprite_Current;
    MH_AT(SortFn, bof3::addr::MagicFx_LinkByDepth)(static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->x),
                                                   static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->z), line_depths, line_base,
                                                   0xC, 0x38, 2);
    Gfx_PacketNext = lines;
}

// original 0x4BE550: one edge of the shell: a semi-transparent flat line
// quad at `prim` (0x40, 0x40, 0xC0) through four SVECTORs passed by value,
// its depth (Gte_RotAverage4's result) to *out. The original also passes the
// address of its own first argument as a tenth, which Gte_RotAverage4 does
// not read.
MS17_EXPORT void __cdecl LeechShell_Edge(unsigned char* prim, long* out, unsigned long a0, unsigned long a1,
                                         unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5,
                                         unsigned long a6, unsigned long a7) {
    MH_CALL(Gpu_SetLineF4)(prim);
    MH_CALL(Gpu_SetSemiTrans)(prim, 1);
    prim[4] = 0x40;
    prim[5] = 0x40;
    prim[6] = 0xC0;
    const unsigned long v[8] = {a0, a1, a2, a3, a4, a5, a6, a7};
    const short* const s = reinterpret_cast<const short*>(v);
    long projected;
    *out = MH_CALL(Gte_RotAverage4)(s, s + 4, s + 8, s + 12, F(prim + 8), F(prim + 0x14), F(prim + 0x20), F(prim + 0x2C),
                                    &projected);
}

// original 0x4BE5B0: pushes the matrix and loads Camera_Matrix x the
// shell's: translation RotTrans of (x >> 9 - 0x4000, z >> 9 - 0x4000,
// -(height / 2)), rotation 0 but about z the spin +0xC when +5 (the task's
// parameter) is 0xD. MagicFx_PushActorMatrix with a spin; pops nothing.
MS17_EXPORT void __cdecl LeechShell_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    short rot[4] = {0, 0, 0, 0};
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    if (sc[5] == 0xD) rot[2] = S16(sc + 0xC);
    short v[4];
    v[0] = static_cast<short>((tsc->x >> 9) - 0x4000);
    v[1] = static_cast<short>((tsc->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(sc + 0x3E) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
    MH_CALL(Gte_RotTrans)(v, m.t);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4BE660: the orb. Between two draw modes (tpage 0x35, slot 4):
// once started (+2 not 0) its rings under the actor matrix; then a call
// through LeechOrb_Phases 0x65B228 by +2, read in place (six entries).
MS17_EXPORT void __cdecl LeechOrb_Task(void) {
    ModeCommit4();
    if (Sprite_Current[2] != 0) {
        MH_CALL(MagicFx_PushActorMatrix)();
        MH_CALL(LeechOrb_DrawRings)();
        MH_CALL(Gte_PopMatrix)();
    }
    Entry(::bof3::Img(LeechOrb_Phases_addr), Sprite_Current[2])();
    ModeCommit4();
}

// original 0x4BE6D0: held until the effect's task (+0x4C) has +0xB 1 (the
// shell has aimed); then +0xA 0, +2 on.
MS17_EXPORT void __cdecl LeechOrb_WaitShell(void) {
    unsigned char* const sc = Sprite_Current;
    if (Pointer(Key(sc + 0x4C))[0xB] != 1) return;
    sc[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4BE6F0: +9 up to 8; there sound 0x101 and +2 on.
MS17_EXPORT void __cdecl LeechOrb_Chime(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 8) {
        MH_CALL(Sound_PlayById)(0x101);
        Bump(Sprite_Current[2]);
        return;
    }
    Bump(sc[9]);
}

// original 0x4BE720: toward the shell (0x4FB9F0, speed 0x30), 0x10 higher a
// frame; once within 0x1C000 of it (0x4FBBD0), +0xA 30 and +2 on.
MS17_EXPORT void __cdecl LeechOrb_Rise(void) {
    MH_CALL(MagicFx_StepToward)(Owner(), 0x30);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::Current<records::BattleTask>()->height_whole + 0x10u);
    if (MH_CALL(MagicFx_NearSprite3D)(Owner(), 0x1C000) == 0) return;
    Sprite_Current[0xA] = 0x1E;
    Bump(Sprite_Current[2]);
}

// original 0x4BE770: +0xA down; at 0 sound 0x102 and +2 on.
MS17_EXPORT void __cdecl LeechOrb_Pause(void) {
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    MH_CALL(Sound_PlayById)(0x102);
    Bump(Sprite_Current[2]);
}

// original 0x4BE7A0: toward the shell (speed 8); once within 0x4000 of it,
// the effect's task (+0x4C, read before) +0xB 2, +0xA 0, +2 on.
MS17_EXPORT void __cdecl LeechOrb_Merge(void) {
    const unsigned char* const sc = Sprite_Current;
    unsigned char* const owner = Owner();
    unsigned char* const task = Pointer(Key(sc + 0x4C));
    MH_CALL(MagicFx_StepToward)(owner, 8);
    if (MH_CALL(MagicFx_NearSprite3D)(Owner(), 0x4000) == 0) return;
    task[0xB] = 2;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4BE7F0: +0xA up; at 8 the orb freed (a tail jmp).
MS17_EXPORT void __cdecl LeechOrb_End(void) {
    Bump(Sprite_Current[0xA]);
    if (Sprite_Current[0xA] == 8) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BE810: eight rings of sixteen semi-transparent flat quads
// round the orb, latitudes 0x400..0xC00 by 0x100, longitudes by 0x100,
// radius +9 * 4 (dword 0x903850): the vertex (cos(lat) * r * sin(lon),
// cos(lat) * r * cos(lon), sin(lat) * r). Each quad a draw mode (tpage 0xB5)
// and the quad, both sorted at the orb moved by its far edge's x / y << 9;
// red (0x80, 0x10, 0x10), from phase 5 fading by +0xA.
MS17_EXPORT void __cdecl LeechOrb_DrawRings(void) {
    SetLong(Mem(::bof3::Img(kScratchA.v)), Sprite_Current[9] << 2);
    int lat = 0x400;
    for (;;) {
        int c = Cos(lat);
        int t = M(c, R());
        int s = Sin(0);
        SetWord(VB(8), static_cast<unsigned>(M(t, s)));
        c = Cos(lat);
        t = M(c, R());
        s = Cos(0);
        SetWord(VB(0xA), static_cast<unsigned>(M(t, s)));
        s = Sin(lat);
        SetWord(VB(0xC), static_cast<unsigned>(M(s, R())));
        const int next = lat + 0x100;
        c = Cos(next);
        t = M(c, R());
        s = Sin(0);
        SetWord(VB(0x18), static_cast<unsigned>(M(t, s)));
        c = Cos(next);
        t = M(c, R());
        s = Cos(0);
        SetWord(VB(0x1A), static_cast<unsigned>(M(t, s)));
        s = Sin(next);
        SetWord(VB(0x1C), static_cast<unsigned>(M(s, R())));
        for (int lon = 0x100; lon < 0x1100; lon += 0x100) {
            CopyVertex(0, 8);
            c = Cos(lat);
            t = M(c, R());
            s = Sin(lon);
            SetWord(VB(8), static_cast<unsigned>(M(t, s)));
            c = Cos(lat);
            t = M(c, R());
            s = Cos(lon);
            SetWord(VB(0xA), static_cast<unsigned>(M(t, s)));
            s = Sin(lat);
            SetWord(VB(0xC), static_cast<unsigned>(M(s, R())));
            CopyVertex(0x10, 0x18);
            c = Cos(next);
            t = M(c, R());
            s = Sin(lon);
            SetWord(VB(0x18), static_cast<unsigned>(M(t, s)));
            c = Cos(next);
            t = M(c, R());
            s = Cos(lon);
            SetWord(VB(0x1A), static_cast<unsigned>(M(t, s)));
            s = Sin(next);
            SetWord(VB(0x1C), static_cast<unsigned>(M(s, R())));
            const unsigned char* sc = Sprite_Current;
            const std::uint32_t x = (static_cast<std::uint32_t>(static_cast<int>(S16(VB(0x18)))) << 9) +
                                    static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->x);
            const std::uint32_t z = (static_cast<std::uint32_t>(static_cast<int>(S16(VB(0x1A)))) << 9) +
                                    static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->z);
            DrawMode(0xB5);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            long depth;
            MH_CALL(Gte_RotTransPers4)(V(0), V(8), V(0x10), V(0x18), F(p + 8), F(p + 0x14), F(p + 0x20), F(p + 0x2C),
                                       &depth);
            MH_CALL(Gte_PrimDepths4_0C)(p);
            sc = Sprite_Current;
            if (sc[2] < 5) {
                p[4] = 0x80;
                p[5] = 0x10;
                p[6] = 0x10;
            } else {
                p[4] = static_cast<unsigned char>(0x81 - (sc[0xA] << 4));
                p[5] = static_cast<unsigned char>(0x11 - (Sprite_Current[0xA] << 1));
                p[6] = static_cast<unsigned char>(0x11 - (Sprite_Current[0xA] << 1));
            }
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x38);
        }
        lat = next;
        if (lat >= 0xC00) break;
    }
}

void LeechShell_Inject() {
    BOF3_INJECT(Leech_Task);
    BOF3_INJECT(Leech_Start);
    BOF3_INJECT(Leech_WaitOrbs);
    BOF3_INJECT(LeechOrb_Dispatch);
    BOF3_INJECT(LeechShell_Task);
    BOF3_INJECT(LeechShell_Grow);
    BOF3_INJECT(MagicFx_WaitA);
    BOF3_INJECT(LeechShell_Aim);
    BOF3_INJECT(LeechShell_Hold);
    BOF3_INJECT(LeechShell_WaitOrb);
    BOF3_INJECT(LeechShell_Fade);
    BOF3_INJECT(LeechShell_End);
    BOF3_INJECT(LeechShell_Draw);
    BOF3_INJECT(LeechShell_Edge);
    BOF3_INJECT(LeechShell_PushMatrix);
    BOF3_INJECT(LeechOrb_Task);
    BOF3_INJECT(LeechOrb_WaitShell);
    BOF3_INJECT(LeechOrb_Chime);
    BOF3_INJECT(LeechOrb_Rise);
    BOF3_INJECT(LeechOrb_Pause);
    BOF3_INJECT(LeechOrb_Merge);
    BOF3_INJECT(LeechOrb_End);
    BOF3_INJECT(LeechOrb_DrawRings);
}
