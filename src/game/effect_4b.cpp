// Round thirteen group E4B (docs/effect_4b.md): the 61 functions of
// analysis/round13_cut.tsv's group E4B, 0x489030..0x48B1FC, and five starts the
// cut does not list - kind 0x87's pane draw 0x489390 (the cut ran 0x489220 over
// it) and its midpoint helper 0x489630, kind 0xA4's state 2 0x48A550, and the
// two shared tails with their own ret that kinds 0x9F and 0xA4 jump to,
// 0x48A480 and 0x48A560 - each read with capstone to its last instruction
// (2026-10-03). Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names. What each
// kind is, as far as the code says:
//
//   kind 0x87   (E4A's dispatcher and states) six shaded panes of four
//               POLY_G3s over fourteen projected points, stepped through four
//               cases - the helpers here
//   kind 0x88   thirty-two particles (0x2C bytes at EffectKind30_Shards)
//               emitted every eighth frame from the record's point toward
//               +0xC.., each a ring of sixteen POLY_G3s, until extra object
//               0's x reaches 0x3C8000 / 0x408000
//   kind 0x89   a full-screen tile (EffectKind89_DrawTint) at 0x80 grey, the
//               party at pose 3, faded out by +1 = 2
//   kind 0x9D   a textured quad of a field object's (+0xB) at its projected
//               point, pulsing (+6 = 0) or growing and shrinking
//   kind 0x9F   the tile brightened to 0x7C, held, faded, the party tinted
//               against it
//   kind 0xA4   the tile brightened to white, the field objects of record
//               words 0x261 / 0x308 redrawn over it
//   kind 0x8A   three lines (kind 0x6F's segment and ribbon, E3C's, at the
//               record's +0xC) along x 0x10, the six cells under them blocked
//               while it lives
//   kind 0x8B   the leader's two poses 0x6F / 0x6E by the counter 0x903849
//   kind 0x8C   a count of pad presses in two timed windows, a steered step
//               of the leader between them, judged against a kept count
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes a table of its own past its rows, or ObjTrio or
// Sprite_Objects past their records, ours aborts with a message
// (docs/effect_4b.md section 6).
#include "game/effect_4b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4b::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Shards() { return AddressOf(EffectKind30_Shards); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// imul of two whole registers: the low 32 bits.
U Mul(U a, std::int32_t b) { return a * static_cast<U>(b); }
unsigned char& Byte(U cell) { return At(cell)[0]; }

// --- x87 as the original has it (the game's control word); every value goes
// through the FPU as Capcom's does.
// `fild dword [v]; fstp dword [o]`
void Fild(std::int32_t v, void* o) {
    __asm__ volatile("fildl %1\n\tfstps (%0)" : : "r"(o), "m"(v) : "st", "memory");
}
// `fild dword [v]; fadd dword [c]; fstp dword [o]`
void FildAdd(std::int32_t v, const void* c, void* o) {
    __asm__ volatile("fildl %2\n\tfadds (%1)\n\tfstps (%0)" : : "r"(o), "r"(c), "m"(v) : "st", "memory");
}
// `fld dword [a]; fadd dword [c]; fstp dword [o]`
void FldAdd(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%0)\n\tfadds (%1)\n\tfstps (%2)" : : "r"(a), "r"(c), "r"(o) : "st", "memory");
}
// `fld dword [a]; fsub dword [c]; fstp dword [o]`
void FldSub(const void* a, const void* c, void* o) {
    __asm__ volatile("flds (%0)\n\tfsubs (%1)\n\tfstps (%2)" : : "r"(a), "r"(c), "r"(o) : "st", "memory");
}
// The CRT's _ftol 0x5B9550 on st(0): truncation toward zero set in a copy of
// the control word for one fistp to 64 bits, the word put back; the low dword
// (eax), which is all the callers keep.
// `fld dword [a]; fsub dword [b]; call _ftol`
U FldSubFtol(const void* a, const void* b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fsubs (%[b])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a), [b] "r"(b)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}
// `fld dword [a]; call _ftol`
U FldFtol(const void* a) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the
// dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4b.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// ObjTrio record m; past the three, ours aborts (the original writes what
// follows).
unsigned char* Member(unsigned m, const char* who) {
    if (m >= at::kObjTrioCount)
        bof3::Fatal("%s: party member %u, past ObjTrio's three records - the original writes what follows "
                    "(docs/effect_4b.md section 6)",
                    who, m);
    return ObjTrio + at::kObjTrioStride * m;
}
// Field_MemberCount as a loop bound over ObjTrio; past three, ours aborts.
unsigned Members(const char* who) {
    const unsigned n = Field_MemberCount;
    if (n > at::kObjTrioCount)
        bof3::Fatal("%s: Field_MemberCount is %u, past ObjTrio's three records - the original writes what follows "
                    "(docs/effect_4b.md section 6)",
                    who, n);
    return n;
}
// Sprite_Objects record k (the effect record's +0xB); past the thirty, ours
// aborts.
unsigned char* Sprite(unsigned k, const char* who) {
    if (k >= at::kSpriteCount)
        bof3::Fatal("%s: +0xB is %u, past Sprite_Objects' thirty records - the original reads and writes what follows "
                    "(docs/effect_4b.md section 6)",
                    who, k);
    return Sprite_Objects + at::kSpriteStride * k;
}

}  // namespace

// ===========================================================================
// Kind 0x87's helpers (its dispatcher 0x488F60 and states are E4A's)
// ===========================================================================

namespace {
U PanePoint(unsigned n) { return at::kPanePoints + at::kPanePointStride * n; }
unsigned char* Pane(unsigned i) { return At(at::kPanes + at::kPaneStride * i); }
void SetPanePoints(unsigned i, unsigned p0, unsigned p1, unsigned p2, unsigned p3) {
    unsigned char* const r = Pane(i);
    SetUL(r + 0, PanePoint(p0));
    SetUL(r + 4, PanePoint(p1));
    SetUL(r + 8, PanePoint(p2));
    SetUL(r + 0xC, PanePoint(p3));
}
}  // namespace

// original 0x489030 (E4A's 0x488F80 calls it): the six panes of 0x1C at
// EffectKind30_Shards - each live (+0x18 1) at case 0 (+0x17), its timer +0x19
// = 15 * i, grey 0xF0 (+0x14..+0x16), its slot +0x1B 3 or 1 by i's parity;
// their four corners and centre pointers into the fourteen projected points
// (0x92C028, 0xC each), their animations +0x1A 1, 1, 7, 5, 5, 3; and rows 8..13
// of kKind87Points made the midpoints of two others (EffectKind87_Midpoint).
extern "C" void __cdecl EffectKind87_Setup(void) {
    for (unsigned i = 0; i < at::kPaneCount; ++i) {
        unsigned char* const r = Pane(i);
        r[0x19] = static_cast<unsigned char>(i * 0xF);
        r[0x18] = 1;
        r[0x17] = 0;
        r[0x14] = r[0x15] = r[0x16] = 0xF0;
        r[0x1B] = (i & 1) == 0 ? 3 : 1;
    }
    SetPanePoints(0, 0, 1, 4, 5);
    SetUL(Pane(0) + 0x10, PanePoint(8));
    Pane(0)[0x1A] = 1;
    SH_CALL(EffectKind87_Midpoint)(0, 5, 8);
    SetPanePoints(1, 0, 1, 2, 3);
    SetUL(Pane(1) + 0x10, PanePoint(9));
    Pane(1)[0x1A] = 1;
    SH_CALL(EffectKind87_Midpoint)(0, 3, 9);
    // pane 2: +0 point 2, +4 point 0
    SetPanePoints(2, 2, 0, 6, 4);
    SetUL(Pane(2) + 0x10, PanePoint(10));
    Pane(2)[0x1A] = 7;
    SH_CALL(EffectKind87_Midpoint)(2, 4, 0xA);
    SetPanePoints(3, 2, 3, 6, 7);
    SetUL(Pane(3) + 0x10, PanePoint(11));
    SH_CALL(EffectKind87_Midpoint)(2, 7, 0xB);
    Pane(3)[0x1A] = 5;
    SetPanePoints(4, 4, 5, 6, 7);
    SetUL(Pane(4) + 0x10, PanePoint(12));
    Pane(4)[0x1A] = 5;
    SH_CALL(EffectKind87_Midpoint)(4, 7, 0xC);
    SetPanePoints(5, 3, 1, 7, 5);
    SH_CALL(EffectKind87_Midpoint)(3, 5, 0xD);
    Pane(5)[0x1A] = 3;
    SetUL(Pane(5) + 0x10, PanePoint(13));
}

// original 0x4891F0 (E4A's 0x488FB0 calls it): the six panes live at case 3
// (+0x17), their timer +0x19 0x1E, coloured (0, 0xF0, 0).
extern "C" void __cdecl EffectKind87_FadePanes(void) {
    for (unsigned i = 0; i < at::kPaneCount; ++i) {
        unsigned char* const r = Pane(i);
        r[0x18] = 1;
        r[0x19] = 0x1E;
        r[0x17] = 3;
        r[0x14] = 0;
        r[0x15] = 0xF0;
        r[0x16] = 0;
    }
}

// original 0x489220 (E4A's 0x488FB0 and 0x488FE0 call it): EffectGte_LoadMapCamera;
// the fourteen rows of kKind87Points projected to 0x92C028..; the camera
// again; each live pane stepped by its case through the switch at 0x489380
// (a case past 3 does nothing): 0 - its timer +0x19 down, at 0 a sound of
// kKind87Sounds by kSoundIndex (which steps 0, 1, 2), case up, timer 0xF; 1 -
// red and blue +0x14 / +0x16 down 0x10, at timer 0xA ObjTrio (made
// Sprite_Current for the call) given the pane's animation +0x1A, the timer
// down, at 0 the next case and 0xF; 2 / 3 - green +0x15 down 0x10 / 8, the
// timer down, at 0 the pane dead. Cases 1..3 draw it (EffectKind87_DrawPane).
// al 1 when any pane was live, else 0.
extern "C" unsigned char __cdecl EffectKind87_StepPanes(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kKind87PointCount; ++i)
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(at::kKind87Points + at::kKind87PointStride * i)),
                                        reinterpret_cast<float*>(At(PanePoint(i))));
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kPaneCount; ++i) {
        unsigned char* const r = Pane(i);
        if (r[0x18] == 0) continue;
        any = 1;
        switch (r[0x17]) {
        case 0:
            if (r[0x19] != 0) {
                r[0x19] = static_cast<unsigned char>(r[0x19] - 1);
                continue;
            }
            {
                const unsigned k = Byte(at::kSoundIndex);
                if (k >= at::kKind87SoundCount)
                    bof3::Fatal("EffectKind87_StepPanes: the sound index 0x%X is %u, past kind 0x87's three sounds at 0x%X "
                                "- the original reads the word after (docs/effect_4b.md section 6)",
                                (unsigned)at::kSoundIndex, k, (unsigned)at::kKind87Sounds);
                SH_CALL(Sound_PlayEffect)(Word(At(at::kKind87Sounds + 2 * k)));
            }
            {
                const unsigned char n = static_cast<unsigned char>(Byte(at::kSoundIndex) + 1);
                Byte(at::kSoundIndex) = n;
                if (n > 2) Byte(at::kSoundIndex) = 0;
            }
            r[0x19] = 0xF;
            r[0x17] = static_cast<unsigned char>(r[0x17] + 1);
            continue;
        case 1: {
            const unsigned char t = r[0x19];
            r[0x14] = static_cast<unsigned char>(r[0x14] + 0xF0);
            r[0x16] = static_cast<unsigned char>(r[0x16] + 0xF0);
            if (t == 0xA) {
                unsigned char* const saved = Sprite_Current;
                Sprite_Current = ObjTrio;
                SH_CALL(Sprite_SetAnimation)(r[0x1A]);
                Sprite_Current = saved;
            }
            r[0x19] = static_cast<unsigned char>(r[0x19] - 1);
            if (r[0x19] == 0) {
                r[0x19] = 0xF;
                r[0x17] = static_cast<unsigned char>(r[0x17] + 1);
            }
            break;
        }
        case 2:
            r[0x15] = static_cast<unsigned char>(r[0x15] + 0xF0);
            r[0x19] = static_cast<unsigned char>(r[0x19] - 1);
            if (r[0x19] == 0) r[0x18] = 0;
            break;
        case 3:
            r[0x15] = static_cast<unsigned char>(r[0x15] + 0xF8);
            r[0x19] = static_cast<unsigned char>(r[0x19] - 1);
            if (r[0x19] == 0) r[0x18] = 0;
            break;
        default: continue;
        }
        SH_CALL(EffectKind87_DrawPane)(r);
    }
    return any;
}

// original 0x489390 (a pane of 0x1C): a draw mode (page (0x380, 0x100), dtd 1)
// committed in the pane's slot +0x1B; then four POLY_G3s, semi-transparent,
// fanned from its centre (the point +0x10 names) over its corners +0 / +4,
// +4 / +0xC, +0xC / +8 and +8 / +0 (each point's three dwords copied), the
// centre its colour +0x14..+0x16 and the corners half of it, each committed
// (0x34) in the slot; the cursor read again after each.
extern "C" void __cdecl EffectKind87_DrawPane(unsigned char* pane) {
    DrawMode(1, 0x380, 0x100, 1);
    SH_CALL(Gfx_CommitPrim)(pane[0x1B], 0xC);
    static const unsigned kCorners[4][2] = {{0, 4}, {4, 0xC}, {0xC, 8}, {8, 0}};
    for (const auto& c : kCorners) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        std::memcpy(p + 8, At(UL(pane + 0x10)), 12);
        std::memcpy(p + 0x18, At(UL(pane + c[0])), 12);
        std::memcpy(p + 0x28, At(UL(pane + c[1])), 12);
        p[4] = pane[0x14];
        p[5] = pane[0x15];
        p[6] = pane[0x16];
        p[0x24] = p[0x14] = static_cast<unsigned char>(pane[0x14] >> 1);
        p[0x25] = p[0x15] = static_cast<unsigned char>(pane[0x15] >> 1);
        p[0x26] = p[0x16] = static_cast<unsigned char>(pane[0x16] >> 1);
        SH_CALL(Gfx_CommitPrim)(pane[0x1B], 0x34);
    }
}

// original 0x489630 (a, b, c: bytes): row c of kKind87Points (three s32) the
// midpoint of rows a and b - (a + b) sar 1, each word as the 32 bits wrap. A
// row past the fourteen is the image's .data after the table: ours aborts.
extern "C" void __cdecl EffectKind87_Midpoint(unsigned a, unsigned b, unsigned c) {
    const unsigned ra = a & 0xFFu, rb = b & 0xFFu, rc = c & 0xFFu;
    if (ra >= at::kKind87PointCount || rb >= at::kKind87PointCount || rc >= at::kKind87PointCount)
        bof3::Fatal("EffectKind87_Midpoint: rows %u, %u, %u, past kKind87Points' fourteen at 0x%X - the original reads "
                    "and writes the .data after (docs/effect_4b.md section 6)",
                    ra, rb, rc, (unsigned)at::kKind87Points);
    unsigned char* const pa = At(at::kKind87Points + at::kKind87PointStride * ra);
    unsigned char* const pb = At(at::kKind87Points + at::kKind87PointStride * rb);
    unsigned char* const pc = At(at::kKind87Points + at::kKind87PointStride * rc);
    for (unsigned w = 0; w < 12; w += 4) SetUL(pc + w, Sar(UL(pb + w) + UL(pa + w), 1));
}

// ===========================================================================
// Kind 0x88: Effect_KindHandlers[0x88] (0x655570), EffectKind88_States (four)
// ===========================================================================

// original 0x4896A0 (Effect_KindHandlers[0x88], hidden after 0x489630):
// jmp [EffectKind88_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind88_Run(void) {
    Dispatch("EffectKind88_Run", AddressOf(EffectKind88_States), EffectKind88_States_count);
}

// original 0x4896C0 (state 0): +6 set - the 32 particles cleared
// (EffectKind88_ClearParticles); +9 0, +1 up.
extern "C" void __cdecl EffectKind88_Start(void) {
    if (S()[6] != 0) SH_CALL(EffectKind88_ClearParticles)();
    S()[9] = 0;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

namespace {
// States 1 and 2: every eighth count (+9 & 7 0) a free particle set up; +9
// up; +6 set - the particles moved and drawn; extra object 0's x at `goal`:
// +1 up.
void Kind88Emit(U goal) {
    if ((S()[9] & 7) == 0) {
        unsigned char* const r = SH_CALL(EffectKind88_FindParticle)();
        if (r != nullptr) SH_CALL(EffectKind88_InitParticle)(r);
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* s = S();
    if (s[6] != 0) {
        SH_CALL(EffectKind88_MoveParticles)();
        s = S();
    }
    if (UL(At(at::kExtraX)) == goal) s[1] = static_cast<unsigned char>(s[1] + 1);
}
}  // namespace

// original 0x4896F0 (state 1): Kind88Emit until x 0x3C8000.
extern "C" void __cdecl EffectKind88_Emit(void) { Kind88Emit(0x3C8000); }

// original 0x489740 (state 2): Kind88Emit until x 0x408000.
extern "C" void __cdecl EffectKind88_EmitOn(void) { Kind88Emit(0x408000); }

// original 0x489790 (state 3): the particles moved and drawn; none live (al 0):
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind88_Fade(void) {
    if (SH_CALL(EffectKind88_MoveParticles)() == 0) SH_CALL(Effect_Release)();
}

// original 0x4897A0: the 32 particles' +3 cleared.
extern "C" void __cdecl EffectKind88_ClearParticles(void) {
    for (unsigned i = 0; i < at::kParticleCount; ++i) At(at::kParticles + at::kParticleStride * i)[3] = 0;
}

// original 0x4897C0: the first of the 32 particles whose +3 is 0, or null.
extern "C" unsigned char* __cdecl EffectKind88_FindParticle(void) {
    for (unsigned i = 0; i < at::kParticleCount; ++i) {
        unsigned char* const r = At(at::kParticles + at::kParticleStride * i);
        if (r[3] == 0) return r;
    }
    return nullptr;
}

// original 0x4897E0: EffectGte_LoadMapCamera; each live particle (+3) moved by
// its velocity (+0xC.. += +0x1C..), its angle +6 the screen angle from the
// record's point to its goal (+0x34.. to +0xC..), its life +5 down - at 0, or
// its x at or past extra object 0's x - 0x10000, dead (+3 0) - and drawn
// (EffectKind88_DrawBurst(its point, its size +8 / +0xA, +6, 0x80, 0)). al 1
// when any was live, else 0.
extern "C" unsigned char __cdecl EffectKind88_MoveParticles(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kParticleCount; ++i) {
        unsigned char* const r = At(at::kParticles + at::kParticleStride * i);
        if (r[3] == 0) continue;
        SetUL(r + 0xC, UL(r + 0xC) + UL(r + 0x1C));
        SetUL(r + 0x10, UL(r + 0x10) + UL(r + 0x20));
        any = 1;
        SetUL(r + 0x14, UL(r + 0x14) + UL(r + 0x24));
        unsigned char* const s = S();
        const int angle = SH_CALL(EffectKind88_ScreenAngle)(reinterpret_cast<const long*>(s + 0x34),
                                                            reinterpret_cast<const long*>(s + 0xC));
        SetWord(r + 6, static_cast<U>(angle));
        r[5] = static_cast<unsigned char>(r[5] - 1);
        if (r[5] == 0 || S32(UL(r + 0xC)) >= S32(UL(At(at::kExtraX)) + 0xFFFF0000u)) r[3] = 0;
        SH_CALL(EffectKind88_DrawBurst)(reinterpret_cast<const long*>(r + 0xC), Word(r + 8), Word(r + 0xA), Word(r + 6), 0x80,
                                        0);
    }
    return any;
}

// original 0x489890 (point, w, h, turn - low words -, centre, rim - bytes): a
// draw mode (page (0x3C0, 0), dtd 1) linked at the point (dy 0); the point
// projected and its size EffectGte_ProjectSize(point, {w, h}), each half up
// Frame_Counter & 1; then sixteen POLY_G3s at the cursor, semi-transparent,
// from the projected centre (its depth all three corners') to two points of
// an ellipse of that size at 0x100 * k and 0x100 * (k + 1) turned by `turn`
// ((cos, sin) * radius >> 12 and the rotation >> 12, the 32 bits wrapping),
// the centre `centre` grey and the rim `rim`; each linked at the point (dy 0,
// 0x34).
extern "C" void __cdecl EffectKind88_DrawBurst(const long* point, unsigned w, unsigned h, unsigned turn, unsigned centre,
                                               unsigned rim) {
    DrawMode(1, 0x3C0, 0, 1);
    SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]), 0, 0xC);
    float l[3];
    SH_CALL(EffectGte_ProjectPoint)(point, l);
    short size[2] = {static_cast<short>(w), static_cast<short>(h)};
    short out[2];
    SH_CALL(EffectGte_ProjectSize)(point, size, out);
    const unsigned short odd = static_cast<unsigned short>(Frame_Counter & 1u);
    out[0] = static_cast<short>(static_cast<unsigned short>(out[0]) + odd);
    out[1] = static_cast<short>(static_cast<unsigned short>(out[1]) + odd);
    const int t = static_cast<int>(turn & 0xFFFFu);
    U around = 0;
    U e = 0;
    for (int n = 0x10; n != 0; --n) {
        unsigned char* const q = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(q);
        SH_CALL(Gpu_SetSemiTrans)(q, 1);
        std::memcpy(q + 8, &l[0], 4);
        std::memcpy(q + 0xC, &l[1], 4);
        std::memcpy(q + 0x30, &l[2], 4);
        std::memcpy(q + 0x20, &l[2], 4);
        std::memcpy(q + 0x10, &l[2], 4);
        // the first rim point, at e
        const int c0 = SH_CALL(Math_Cos)(static_cast<int>(e));
        const U rx0 = Sar(Mul(static_cast<U>(c0), static_cast<short>(out[0])), 12);
        const int s0 = SH_CALL(Math_Sin)(static_cast<int>(e));
        const std::int32_t ry = static_cast<short>(Sar(Mul(static_cast<U>(s0), static_cast<short>(out[1])), 12));
        const std::int32_t rx = static_cast<short>(rx0);
        const U cx = Mul(static_cast<U>(SH_CALL(Math_Cos)(t)), rx);
        const U sy = Mul(static_cast<U>(SH_CALL(Math_Sin)(t)), ry);
        FildAdd(S32(Sar(cx - sy, 12)), &l[0], q + 0x18);
        const U sx = Mul(static_cast<U>(SH_CALL(Math_Sin)(t)), rx);
        const U cy = Mul(static_cast<U>(SH_CALL(Math_Cos)(t)), ry);
        around += 0x100;
        e = around & 0xFFFFu;
        FildAdd(S32(Sar(sx + cy, 12)), &l[1], q + 0x1C);
        // the second, at e + 0x100
        const int c1 = SH_CALL(Math_Cos)(static_cast<int>(e));
        const U rx1w = Sar(Mul(static_cast<U>(c1), static_cast<short>(out[0])), 12);
        const int s1 = SH_CALL(Math_Sin)(static_cast<int>(e));
        const std::int32_t ry1 = static_cast<short>(Sar(Mul(static_cast<U>(s1), static_cast<short>(out[1])), 12));
        const std::int32_t rx1 = static_cast<short>(rx1w);
        const U cx1 = Mul(static_cast<U>(SH_CALL(Math_Cos)(t)), rx1);
        const U sy1 = Mul(static_cast<U>(SH_CALL(Math_Sin)(t)), ry1);
        FildAdd(S32(Sar(cx1 - sy1, 12)), &l[0], q + 0x28);
        const U sx1 = Mul(static_cast<U>(SH_CALL(Math_Sin)(t)), rx1);
        const U cy1 = Mul(static_cast<U>(SH_CALL(Math_Cos)(t)), ry1);
        FildAdd(S32(Sar(sx1 + cy1, 12)), &l[1], q + 0x2C);
        q[4] = q[5] = q[6] = static_cast<unsigned char>(centre);
        q[0x14] = q[0x15] = q[0x16] = static_cast<unsigned char>(rim);
        q[0x24] = q[0x25] = q[0x26] = static_cast<unsigned char>(rim);
        SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(point[0]), static_cast<unsigned long>(point[1]), 0, 0x34);
    }
}

// original 0x489AF0 (from, to: points of three s32): both projected; the
// screen angle Math_Ratan2(to.y - from.y, to.x - from.x), the differences
// taken in the FPU as floats. eax its answer (the callers keep ax).
extern "C" int __cdecl EffectKind88_ScreenAngle(const long* from, const long* to) {
    float a[3], b[3];
    SH_CALL(EffectGte_ProjectPoint)(from, a);
    SH_CALL(EffectGte_ProjectPoint)(to, b);
    float dx, dy;
    FldSub(&b[0], &a[0], &dx);
    FldSub(&b[1], &a[1], &dy);
    return SH_CALL(Math_Ratan2)(dy, dx);
}

// original 0x489B40 (a particle of 0x2C): live (+3 1), +4 0, at the record's
// point (+0xC.. = +0x34..), its velocity a 32nd of the way to the record's
// goal (+0x1C.. = (+0xC.. - +0x34..) sar 5), its size (0x40, 0x20) (+8 /
// +0xA), its life +5 0x20, its angle +6 EffectKind88_ScreenAngle(point, goal).
extern "C" void __cdecl EffectKind88_InitParticle(unsigned char* particle) {
    particle[3] = 1;
    particle[4] = 0;
    SetUL(particle + 0xC, UL(S() + 0x34));
    SetUL(particle + 0x10, UL(S() + 0x38));
    SetUL(particle + 0x14, UL(S() + 0x3C));
    unsigned char* s = S();
    SetUL(particle + 0x1C, Sar(UL(s + 0xC) - UL(s + 0x34), 5));
    s = S();
    SetUL(particle + 0x20, Sar(UL(s + 0x10) - UL(s + 0x38), 5));
    s = S();
    SetUL(particle + 0x24, Sar(UL(s + 0x14) - UL(s + 0x3C), 5));
    SetWord(particle + 8, 0x40);
    SetWord(particle + 0xA, 0x20);
    particle[5] = 0x20;
    s = S();
    const int angle = SH_CALL(EffectKind88_ScreenAngle)(reinterpret_cast<const long*>(s + 0x34),
                                                        reinterpret_cast<const long*>(s + 0xC));
    SetWord(particle + 6, static_cast<U>(angle));
}

// ===========================================================================
// Kind 0x89: Effect_KindHandlers[0x89] (0x655574), EffectKind89_States (four;
// entry 3 BareRet) - and the screen tile kinds 0x89, 0x9F and 0xA4 draw
// ===========================================================================

// original 0x489BE0 (Effect_KindHandlers[0x89], hidden after 0x489B40):
// jmp [EffectKind89_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind89_Run(void) {
    Dispatch("EffectKind89_Run", AddressOf(EffectKind89_States), EffectKind89_States_count);
}

// original 0x489C00 (state 0): each party member below Field_MemberCount (read
// again after each) made Sprite_Current, pose +0x29 3, Sprite_UpdateScreen;
// Sprite_Current put back; the tile's colour +0x5D..+0x5F 0x80 (+0x5F on the
// record kept, the others read afresh); +1 = 1.
extern "C" void __cdecl EffectKind89_Start(void) {
    unsigned char* const saved = Sprite_Current;
    if (Field_MemberCount != 0) {
        unsigned char b = 0;
        do {
            unsigned char* const m = Member(b, "EffectKind89_Start");
            Sprite_Current = m;
            m[0x29] = 3;
            SH_CALL(Sprite_UpdateScreen)();
            b = static_cast<unsigned char>(b + 1);
        } while (b < Field_MemberCount);
    }
    Sprite_Current = saved;
    saved[0x5F] = 0x80;
    S()[0x5E] = 0x80;
    S()[0x5D] = 0x80;
    S()[1] = 1;
}

// original 0x489C80 (state 1): the tile (a tail jump).
extern "C" void __cdecl EffectKind89_Hold(void) { SH_CALL(EffectKind89_DrawTint)(); }

// original 0x489C90 (state 2): +0x5D above 0 - the colour down 2 each; else
// the chapter's step byte up and +1 = 3. Then the tile (a tail jump).
extern "C" void __cdecl EffectKind89_Fade(void) {
    unsigned char* const s = S();
    if (s[0x5D] != 0) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] - 2);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 0xFE);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 0xFE);
    } else {
        Byte(at::kStep) = static_cast<unsigned char>(Byte(at::kStep) + 1);
        s[1] = 3;
    }
    SH_CALL(EffectKind89_DrawTint)();
}

// original 0x489CD0 (kinds 0x89, 0x9F and 0xA4 draw it): a draw mode (page
// (0x3C0, 0), abr 2, dtd 1) committed in slot 3; a TILE at the cursor, (0, 0)
// 320 x 240 (the floats Capcom wrote: DIVERGENCE.md DIV-0041 names 0x489D47 a
// fill not yet widened), of the record's +0x5D..+0x5F, semi-transparent,
// untextured-shaded off, committed (3, 0x1C).
extern "C" void __cdecl EffectKind89_DrawTint(void) {
    DrawMode(2, 0x3C0, 0, 1);
    SH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const t = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(t);
    t[4] = S()[0x5D];
    t[5] = S()[0x5E];
    t[6] = S()[0x5F];
    SetUL(t + 8, 0);
    SetUL(t + 0xC, 0);
    SetUL(t + 0x14, 0x43A00000u);   // 320.0f
    SetUL(t + 0x18, 0x43700000u);   // 240.0f
    SH_CALL(Gpu_SetSemiTrans)(t, 1);
    SH_CALL(Gpu_SetShadeTex)(t, 0);
    SH_CALL(Gfx_CommitPrim)(3, 0x1C);
}

// ===========================================================================
// Kind 0x9D: Effect_KindHandlers[0x9D] (0x6555C4), EffectKind9D_States (four)
// ===========================================================================

// original 0x489D70 (Effect_KindHandlers[0x9D], hidden after 0x489CD0):
// jmp [EffectKind9D_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind9D_Run(void) {
    Dispatch("EffectKind9D_Run", AddressOf(EffectKind9D_States), EffectKind9D_States_count);
}

// original 0x489D90 (state 0): nothing while Field_Request is 3 or 1. Else
// the field object of +0xB: its +0x24 bit 3 set, its words +0x2E / +0x30 the
// record's, its point +0x34.. the record's point; the colour +0x5D..+0x5F 0;
// +6 0 - the pulse at once (EffectKind9D_Pulse) and +1 = 1; else +1 = 2.
extern "C" void __cdecl EffectKind9D_Start(void) {
    const unsigned char request = Field_Request;
    if (request == 3 || request == 1) return;
    unsigned char* const s = S();
    unsigned char* const o = Sprite(s[0xB], "EffectKind9D_Start");
    o[0x24] = static_cast<unsigned char>(o[0x24] | 8);
    SetWord(o + 0x2E, Word(s + 0x2E));
    SetWord(o + 0x30, Word(s + 0x30));
    SetUL(s + 0x34, UL(o + 0x34));
    SetUL(S() + 0x38, UL(o + 0x38));
    SetUL(S() + 0x3C, UL(o + 0x3C));
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    unsigned char* const s2 = S();
    if (s2[6] == 0) {
        SH_CALL(EffectKind9D_Pulse)();
        S()[1] = 1;
        return;
    }
    s2[1] = 2;
}

namespace {
// Field_Request 3 or 1: the object's +0x24 bit 3 cleared, +1 = 0.
void Kind9DLetGo(unsigned char* s, const char* who) {
    unsigned char* const o = Sprite(s[0xB], who);
    o[0x24] = static_cast<unsigned char>(o[0x24] & 0xF7);
    s[1] = 0;
}
}  // namespace

// original 0x489E50 (state 1): Field_Request 3 or 1 - let go (+1 = 0). Else
// projected (EffectKind9D_Project); +9 up 8; +0x5D / +0x5E Math_Sin / Math_Cos(+9
// << 4) sar 7; the pulse drawn at the projected point.
extern "C" void __cdecl EffectKind9D_Pulse(void) {
    const unsigned char request = Field_Request;
    if (request == 3 || request == 1) {
        Kind9DLetGo(S(), "EffectKind9D_Pulse");
        return;
    }
    float out[2];
    SH_CALL(EffectKind9D_Project)(out);
    S()[9] = static_cast<unsigned char>(S()[9] + 8);
    const int sn = SH_CALL(Math_Sin)(static_cast<int>(static_cast<U>(S()[9]) << 4));
    S()[0x5D] = static_cast<unsigned char>(Sar(static_cast<U>(sn), 7));
    const int cs = SH_CALL(Math_Cos)(static_cast<int>(static_cast<U>(S()[9]) << 4));
    S()[0x5E] = static_cast<unsigned char>(Sar(static_cast<U>(cs), 7));
    U x, y;
    std::memcpy(&x, &out[0], 4);
    std::memcpy(&y, &out[1], 4);
    SH_CALL(EffectKind9D_DrawPulse)(x, y);
}

// original 0x489F00 (state 2): projected; +0x5D up 2 below 0x70 and +0x5E up 1
// below 0x30 (signed); the glow drawn; Field_Request 3 or 1 - let go; then at
// (0x70, 0x30) exactly, +6 0 and +1 = 1.
extern "C" void __cdecl EffectKind9D_Grow(void) {
    float out[2];
    SH_CALL(EffectKind9D_Project)(out);
    unsigned char* s = S();
    if (static_cast<signed char>(s[0x5D]) < 0x70) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 2);
        s = S();
    }
    if (static_cast<signed char>(s[0x5E]) < 0x30) s[0x5E] = static_cast<unsigned char>(s[0x5E] + 1);
    U x, y;
    std::memcpy(&x, &out[0], 4);
    std::memcpy(&y, &out[1], 4);
    SH_CALL(EffectKind9D_DrawGlow)(x, y);
    const unsigned char request = Field_Request;
    if (request == 3 || request == 1) Kind9DLetGo(S(), "EffectKind9D_Grow");
    s = S();
    if (s[0x5D] == 0x70 && s[0x5E] == 0x30) {
        s[6] = 0;
        S()[1] = 1;
    }
}

// original 0x489FA0 (state 3): projected; +0x5D down 2 and +0x5E down 1 while
// not 0; the glow drawn; both 0: Effect_Release.
extern "C" void __cdecl EffectKind9D_Shrink(void) {
    float out[2];
    SH_CALL(EffectKind9D_Project)(out);
    unsigned char* s = S();
    if (s[0x5D] != 0) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] - 2);
        s = S();
    }
    if (s[0x5E] != 0) s[0x5E] = static_cast<unsigned char>(s[0x5E] - 1);
    U x, y;
    std::memcpy(&x, &out[0], 4);
    std::memcpy(&y, &out[1], 4);
    SH_CALL(EffectKind9D_DrawGlow)(x, y);
    s = S();
    if (s[0x5D] == 0 && s[0x5E] == 0) SH_CALL(Effect_Release)();
}

namespace {
// Kind 0x9D's quad (0x48A010 and 0x48A160): a POLY_GT4 at the cursor (read
// once), Gte_PrimDepthFlat4_14; its corners (x, y), (x + kQuadW, y), (x, y +
// kQuadH), (x + kQuadW, y + kQuadH) - the sums in the FPU; its texture the
// record's u +0x2E and v +0x30 -/+ 0xC and -0x25 / +0xB; `top` the two upper
// corners' grey, `left` / `right` the lower two's green; a page (2, abr 1,
// (0x340, 0x100)); semi-transparent; linked at the record's point (dy 1,
// 0x54).
void Kind9DQuad(U x, U y, int top, int left, int right) {
    unsigned char* const q = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyGT4)(q);
    SH_CALL(Gte_PrimDepthFlat4_14)(q);
    float x1, y1;
    FldAdd(&x, At(at::kQuadW), &x1);
    FldAdd(&y, At(at::kQuadH), &y1);
    SetUL(q + 0xC, y);
    SetUL(q + 8, x);
    SetUL(q + 0x20, y);
    std::memcpy(q + 0x1C, &x1, 4);
    SetUL(q + 0x30, x);
    std::memcpy(q + 0x34, &y1, 4);
    std::memcpy(q + 0x44, &x1, 4);
    std::memcpy(q + 0x48, &y1, 4);
    q[0x14] = static_cast<unsigned char>(S()[0x2E] - 0xC);
    q[0x15] = static_cast<unsigned char>(S()[0x30] - 0x25);
    q[0x28] = static_cast<unsigned char>(S()[0x2E] + 0xC);
    q[0x29] = static_cast<unsigned char>(S()[0x30] - 0x25);
    q[0x3C] = static_cast<unsigned char>(S()[0x2E] - 0xC);
    q[0x3D] = static_cast<unsigned char>(S()[0x30] + 0xB);
    q[0x50] = static_cast<unsigned char>(S()[0x2E] + 0xC);
    q[0x51] = static_cast<unsigned char>(S()[0x30] + 0xB);
    q[4] = q[5] = q[6] = static_cast<unsigned char>(top);
    q[0x18] = q[0x19] = q[0x1A] = static_cast<unsigned char>(top);
    if (left < 0) {
        // the glow: both lower corners `right` grey
        q[0x2C] = q[0x2D] = q[0x2E] = static_cast<unsigned char>(right);
        q[0x40] = q[0x41] = q[0x42] = static_cast<unsigned char>(right);
    } else {
        // the pulse: the lower corners green only
        q[0x2C] = 0;
        q[0x2D] = static_cast<unsigned char>(left);
        q[0x2E] = 0;
        q[0x40] = 0;
        q[0x41] = static_cast<unsigned char>(right);
        q[0x42] = 0;
    }
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(2, 1, 0x340, 0x100);
    SetWord(q + 0x2A, tpage);
    SH_CALL(Gpu_SetSemiTrans)(q, 1);
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0x54);
}
}  // namespace

// original 0x48A010 (x, y: a screen point's floats): Kind9DQuad with the upper
// corners 0x90 grey and the lower two green +0x5D + 0x20 / +0x5E + 0x20 (both
// read at entry).
extern "C" void __cdecl EffectKind9D_DrawPulse(unsigned long x, unsigned long y) {
    unsigned char* const s = S();
    const unsigned char a = static_cast<unsigned char>(s[0x5D] + 0x20);
    const unsigned char b = static_cast<unsigned char>(s[0x5E] + 0x20);
    Kind9DQuad(static_cast<U>(x), static_cast<U>(y), 0x90, a, b);
}

// original 0x48A160 (x, y): Kind9DQuad with the upper corners +0x5D grey and
// the lower two +0x5E grey (both read at entry).
extern "C" void __cdecl EffectKind9D_DrawGlow(unsigned long x, unsigned long y) {
    unsigned char* const s = S();
    const unsigned char a = s[0x5D];
    const unsigned char b = s[0x5E];
    Kind9DQuad(static_cast<U>(x), static_cast<U>(y), a, -1, b);
}

// original 0x48A2A0 (out: two floats): the field object of +0xB given the
// record's words +0x2E / +0x30; the vector ((+0x34 sar 9) - 0x4000, (+0x38 sar
// 9) - 0x4000, -(+0x3E / 2)) through Gte_RotTransPers (its fourth word Capcom
// never wrote; the original pushes its own argument's slot as a fourth
// argument the callee does not read), the depth to +0x60; out less kCentreX /
// kCentreY.
extern "C" void __cdecl EffectKind9D_Project(float* out) {
    unsigned char* const s = S();
    unsigned char* const o = Sprite(s[0xB], "EffectKind9D_Project");
    SetWord(o + 0x2E, Word(s + 0x2E));
    SetWord(o + 0x30, Word(s + 0x30));
    short v[3];
    v[0] = static_cast<short>(Sar(UL(s + 0x34), 9) - 0x4000u);
    v[1] = static_cast<short>(Sar(UL(s + 0x38), 9) - 0x4000u);
    v[2] = static_cast<short>(-(SW(s + 0x3E) / 2));
    long depth;
    const long r = SH_CALL(Gte_RotTransPers)(v, reinterpret_cast<unsigned long*>(out), &depth);
    SetUL(S() + 0x60, static_cast<U>(r));
    FldSub(&out[0], At(at::kCentreX), &out[0]);
    FldSub(&out[1], At(at::kCentreY), &out[1]);
}

// ===========================================================================
// Kind 0x9F: Effect_KindHandlers[0x9F] (0x6555CC), EffectKind9F_States (four);
// kind 0xA4: Effect_KindHandlers[0xA4] (0x6555E0), EffectKindA4_States (three)
// ===========================================================================

// original 0x48A350 (Effect_KindHandlers[0x9F], hidden after 0x48A2A0):
// jmp [EffectKind9F_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind9F_Run(void) {
    Dispatch("EffectKind9F_Run", AddressOf(EffectKind9F_States), EffectKind9F_States_count);
}

// original 0x48A370 (state 0 of kinds 0x9F and 0xA4): the party members below
// Field_MemberCount (read once) at pose +0x29 3; the colour +0x5D..+0x5F 0; +1
// = 1.
extern "C" void __cdecl EffectKind9F_Start(void) {
    const unsigned n = Members("EffectKind9F_Start");
    for (unsigned i = 0; i < n; ++i) Member(i, "EffectKind9F_Start")[0x29] = 3;
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[1] = 1;
}

// original 0x48A3C0 (state 1): +0x5D below 0x7C (signed) - the colour up 4
// each; else the counter 0x903848 up and +1 = 2. The tile; the party tinted (a
// tail jump).
extern "C" void __cdecl EffectKind9F_Brighten(void) {
    unsigned char* const s = S();
    const unsigned char c = s[0x5D];
    if (static_cast<signed char>(c) < 0x7C) {
        s[0x5D] = static_cast<unsigned char>(c + 4);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 4);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 4);
    } else {
        Byte(at::kCounter) = static_cast<unsigned char>(Byte(at::kCounter) + 1);
        s[1] = 2;
    }
    SH_CALL(EffectKind89_DrawTint)();
    SH_CALL(EffectKind9F_TintParty)();
}

// original 0x48A410 (state 2): the tile; the party tinted (a tail jump).
extern "C" void __cdecl EffectKind9F_Hold(void) {
    SH_CALL(EffectKind89_DrawTint)();
    SH_CALL(EffectKind9F_TintParty)();
}

// original 0x48A420 (state 3): +0x5D above 0 - the colour down 2 each, the
// tile, the party tinted (a tail jump). Else the party members below
// Field_MemberCount (read once) given Draw_OtSlot (read once) as pose +0x29,
// and Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind9F_Fade(void) {
    unsigned char* const s = S();
    const unsigned char c = s[0x5D];
    if (c != 0) {
        s[0x5D] = static_cast<unsigned char>(c - 2);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 0xFE);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 0xFE);
        SH_CALL(EffectKind89_DrawTint)();
        SH_CALL(EffectKind9F_TintParty)();
        return;
    }
    const unsigned n = Members("EffectKind9F_Fade");
    if (n != 0) {
        const unsigned char slot = Draw_OtSlot;
        for (unsigned i = 0; i < n; ++i) Member(i, "EffectKind9F_Fade")[0x29] = slot;
    }
    SH_CALL(Effect_Release)();
}

// original 0x48A480 (the tail of states 1..3, its own ret): v = -(s8 +0x5D /
// 2), truncating; each party member below Field_MemberCount (read once) not
// at +0x89 7 given v as its colour +0x5D..+0x5F.
extern "C" void __cdecl EffectKind9F_TintParty(void) {
    const std::int32_t c = static_cast<signed char>(S()[0x5D]);
    const auto v = static_cast<unsigned char>(-(c / 2));
    const unsigned n = Members("EffectKind9F_TintParty");
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const m = Member(i, "EffectKind9F_TintParty");
        if (m[0x89] == 7) continue;
        m[0x5F] = v;
        m[0x5E] = v;
        m[0x5D] = v;
    }
}

// original 0x48A4C0 (Effect_KindHandlers[0xA4], hidden after 0x48A2A0):
// jmp [EffectKindA4_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKindA4_Run(void) {
    Dispatch("EffectKindA4_Run", AddressOf(EffectKindA4_States), EffectKindA4_States_count);
}

// original 0x48A4E0 (state 1): +0x5D below 0xF8 (unsigned) - the colour up 8
// each; else all 0xFF, the chapter's step byte up and +1 = 2. The tile; the
// marked objects drawn (a tail jump).
extern "C" void __cdecl EffectKindA4_Brighten(void) {
    unsigned char* const s = S();
    const unsigned char c = s[0x5D];
    if (c < 0xF8) {
        s[0x5D] = static_cast<unsigned char>(c + 8);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 8);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 8);
    } else {
        s[0x5F] = 0xFF;
        S()[0x5E] = 0xFF;
        S()[0x5D] = 0xFF;
        Byte(at::kStep) = static_cast<unsigned char>(Byte(at::kStep) + 1);
        S()[1] = 2;
    }
    SH_CALL(EffectKind89_DrawTint)();
    SH_CALL(EffectKindA4_MarkSprites)();
}

// original 0x48A550 (state 2; not in the cut): the tile; the marked objects
// drawn (a tail jump).
extern "C" void __cdecl EffectKindA4_Hold(void) {
    SH_CALL(EffectKind89_DrawTint)();
    SH_CALL(EffectKindA4_MarkSprites)();
}

// original 0x48A560 (the tail of states 1 and 2, its own frame and ret): each
// of the thirty field objects whose record word (Scena15_RecordWord(its word
// +0x2C)) is 0x261, and each whose word (asked again) is 0x308 while flag 0x11
// of the chapter's row is set, made Sprite_Current, pose +0x29 3,
// Sprite_UpdateScreen; Sprite_Current put back.
extern "C" void __cdecl EffectKindA4_MarkSprites(void) {
    unsigned char* const saved = Sprite_Current;
    for (unsigned i = 0; i < at::kSpriteCount; ++i) {
        unsigned char* const o = Sprite_Objects + at::kSpriteStride * i;
        if ((SH_CALL(Scena15_RecordWord)(Word(o + 0x2C)) & 0xFFFFu) == 0x261) {
            Sprite_Current = o;
            o[0x29] = 3;
            SH_CALL(Sprite_UpdateScreen)();
        }
        if ((SH_CALL(Scena15_RecordWord)(Word(o + 0x2C)) & 0xFFFFu) == 0x308 &&
            SH_CALL(Flags_Test)(reinterpret_cast<const unsigned char*>(UL(At(at::kFlagRow))), 0x11) != 0) {
            Sprite_Current = o;
            o[0x29] = 3;
            SH_CALL(Sprite_UpdateScreen)();
        }
    }
    Sprite_Current = saved;
}

// ===========================================================================
// Kind 0x8A: Effect_KindHandlers[0x8A] (0x655578), EffectKind8A_States (three)
// ===========================================================================

// original 0x48A5F0 (Effect_KindHandlers[0x8A], hidden after 0x48A2A0):
// jmp [EffectKind8A_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8A_Run(void) {
    Dispatch("EffectKind8A_Run", AddressOf(EffectKind8A_States), EffectKind8A_States_count);
}

// original 0x48A610 (state 0): the six cells of kKind8ACells at x 0x10 given
// map byte 0x89 (AreaMap_SetByte); +1 = 1.
extern "C" void __cdecl EffectKind8A_Block(void) {
    for (unsigned i = 0; i < at::kKind8ACellCount; ++i) SH_CALL(AreaMap_SetByte)(0x10, At(at::kKind8ACells + i)[0], 0x89);
    S()[1] = 1;
}

// original 0x48A650 (state 1): +6 0; the lines' x 0x100000 (+0xC and +0x18),
// their foot +0x3C 0 - so their top +0x20 (and +0x14) (+0x3E + 0x80) << 16;
// each of kKind8ALines' three z pairs to +0x10 / +0x1C and drawn
// (EffectKind8A_DrawSegment(+0xC, +0x18)). Story flag 0x91 set: +1 = 2.
extern "C" void __cdecl EffectKind8A_Lines(void) {
    S()[6] = 0;
    SetUL(S() + 0x18, 0x100000);
    SetUL(S() + 0xC, 0x100000);
    SetUL(S() + 0x3C, 0);
    for (unsigned i = 0; i < at::kKind8ALineCount; ++i) {
        const U pair = at::kKind8ALines + 8 * i;
        SetUL(S() + 0x10, UL(At(pair)));
        SetUL(S() + 0x1C, UL(At(pair + 4)));
        unsigned char* s = S();
        SetUL(s + 0x20, static_cast<U>(SW(s + 0x3E) + 0x80) << 16);
        s = S();
        SetUL(s + 0x14, UL(s + 0x20));
        s = S();
        SH_CALL(EffectKind8A_DrawSegment)(reinterpret_cast<const long*>(s + 0xC), reinterpret_cast<const long*>(s + 0x18));
    }
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x91) != 0) S()[1] = 2;
}

// original 0x48A700 (state 2): the six cells given map byte 0; Effect_Release.
extern "C" void __cdecl EffectKind8A_Unblock(void) {
    for (unsigned i = 0; i < at::kKind8ACellCount; ++i) SH_CALL(AreaMap_SetByte)(0x10, At(at::kKind8ACells + i)[0], 0);
    SH_CALL(Effect_Release)();
}

namespace {
// Kind 0x8A's colour row, by Sprite_Current +6 (read afresh each time): one
// row of three bytes; a +6 past it reads into kKind8ACells.
unsigned char Kind8AColour(unsigned byte, const char* who) {
    const unsigned k = S()[6];
    if (k >= at::kKind8AColourCount)
        bof3::Fatal("%s: +6 is %u, past kind 0x8A's one colour row at 0x%X - the original reads the map cells after "
                    "as a colour (docs/effect_4b.md section 6)",
                    who, k, (unsigned)at::kKind8AColour);
    return At(at::kKind8AColour + 3 * k + byte)[0];
}
}  // namespace

// original 0x48A730 (a, b: two points of three dwords; E3C's
// EffectKind6F_DrawSegment's twin, linked at +0xC / +0x10 and without its
// closing draw mode): a draw mode (page (0x3C0, 0), dtd 1) linked at the
// record's +0xC / +0x10 with dy 1; EffectGte_LoadMapCamera; a LINE_F2 at the
// cursor, opaque, from a to b projected, kind 0x8A's colour, linked (.., 1,
// 0x20); the screen angle Math_Ratan2(dy, dx) (each through _ftol, then a
// float); the two ends' sizes EffectGte_ProjectSize(end, {0x40, 0}, out) -
// out's dword plus Frame_Counter & 1 (read after each);
// EffectKind8A_DrawRibbon(the four screen words through _ftol, the sizes, the
// angle + 0x400 and + 0xC00).
extern "C" void __cdecl EffectKind8A_DrawSegment(const long* a, const long* b) {
    DrawMode(1, 0x3C0, 0, 1);
    unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0xC), UL(s + 0x10), 1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    SH_CALL(EffectGte_ProjectPoint)(a, reinterpret_cast<float*>(prim + 8));
    SH_CALL(EffectGte_ProjectPoint)(b, reinterpret_cast<float*>(prim + 0x14));
    prim[4] = Kind8AColour(0, "EffectKind8A_DrawSegment");
    prim[5] = Kind8AColour(1, "EffectKind8A_DrawSegment");
    prim[6] = Kind8AColour(2, "EffectKind8A_DrawSegment");
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0xC), UL(s + 0x10), 1, 0x20);
    const U dx = FldSubFtol(prim + 0x14, prim + 8);
    float fx, fy;
    Fild(S32(dx), &fx);
    const U dy = FldSubFtol(prim + 0x18, prim + 0xC);
    Fild(S32(dy), &fy);
    const U angle = static_cast<U>(SH_CALL(Math_Ratan2)(fy, fx));
    short size[2] = {0x40, 0};
    short out[2];
    SH_CALL(EffectGte_ProjectSize)(a, size, out);
    U wa;
    std::memcpy(&wa, out, 4);
    wa += Frame_Counter & 1u;
    SH_CALL(EffectGte_ProjectSize)(b, size, out);
    U wb;
    std::memcpy(&wb, out, 4);
    wb += Frame_Counter & 1u;
    const U by = FldFtol(prim + 0x18);
    const U bx = FldFtol(prim + 0x14);
    const U ay = FldFtol(prim + 0xC);
    const U ax = FldFtol(prim + 8);
    SH_CALL(EffectKind8A_DrawRibbon)(S32(ax), S32(ay), S32(wa), S32(angle + 0x400u), S32(bx), S32(by), S32(wb),
                                     S32(angle + 0xC00u));
}

// original 0x48A8E0 (ax, ay, wa, ta, bx, by, wb, tb - every one read as its low
// s16, the angles masked to 16 bits; E3C's EffectKind6F_DrawRibbon's twin,
// linked at +0xC / +0x10): two POLY_G4s at the cursor (read once),
// semi-transparent, sharing the edge (ax, ay)-(bx, by) in kind 0x8A's colour;
// the far corners black at (Math_Cos, Math_Sin) * radius >> 12 off the ends -
// the first quad at ta round a (radius wa) and tb + 0x800 round b (wb), the
// second (a copy of the first's 0x44 bytes) at ta + 0x800 and tb. Each linked
// at the record's +0xC / +0x10 (.., 1, 0x44); the second is the cursor read at
// entry + 0x44.
extern "C" void __cdecl EffectKind8A_DrawRibbon(int ax, int ay, int wa, int ta, int bx, int by, int wb, int tb) {
    unsigned char* const q = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(q);
    SH_CALL(Gpu_SetSemiTrans)(q, 1);
    const std::int32_t x0 = static_cast<short>(ax), y0 = static_cast<short>(ay);
    const std::int32_t x1 = static_cast<short>(bx), y1 = static_cast<short>(by);
    Fild(x0, q + 8);
    Fild(y0, q + 0xC);
    Fild(x1, q + 0x18);
    Fild(y1, q + 0x1C);
    const U t1 = static_cast<U>(ta) & 0xFFFFu;
    const std::int32_t r1 = static_cast<short>(wa);
    const std::int32_t r2 = static_cast<short>(wb);
    auto corner = [](int trig, std::int32_t radius, std::int32_t base) {
        return S32(Sar(Mul(static_cast<U>(trig), radius), 12) + static_cast<U>(base));
    };
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t1)), r1, x0), q + 0x28);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t1)), r1, y0), q + 0x2C);
    const U t2 = static_cast<U>(tb) & 0xFFFFu;
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t2 + 0x800u)), r2, x1), q + 0x38);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t2 + 0x800u)), r2, y1), q + 0x3C);
    q[4] = Kind8AColour(0, "EffectKind8A_DrawRibbon");
    q[5] = Kind8AColour(1, "EffectKind8A_DrawRibbon");
    q[6] = Kind8AColour(2, "EffectKind8A_DrawRibbon");
    q[0x14] = Kind8AColour(0, "EffectKind8A_DrawRibbon");
    q[0x15] = Kind8AColour(1, "EffectKind8A_DrawRibbon");
    q[0x16] = Kind8AColour(2, "EffectKind8A_DrawRibbon");
    q[0x24] = q[0x25] = q[0x26] = 0;
    q[0x34] = q[0x35] = q[0x36] = 0;
    unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0xC), UL(s + 0x10), 1, 0x44);
    unsigned char* const q2 = q + 0x44;
    std::memcpy(q2, q, 0x44);
    const U t3 = t1 + 0x800u;
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t3)), r1, x0), q2 + 0x28);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t3)), r1, y0), q2 + 0x2C);
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t2)), r2, x1), q2 + 0x38);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t2)), r2, y1), q2 + 0x3C);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0xC), UL(s + 0x10), 1, 0x44);
}

// ===========================================================================
// Kind 0x8B: Effect_KindHandlers[0x8B] (0x65557C), EffectKind8B_States (four)
// ===========================================================================

// original 0x48AB30 (Effect_KindHandlers[0x8B], hidden after 0x48A8E0):
// jmp [EffectKind8B_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8B_Run(void) {
    Dispatch("EffectKind8B_Run", AddressOf(EffectKind8B_States), EffectKind8B_States_count);
}

// original 0x48AB50 (state 0): +9 = 0xB4, +1 = 1.
extern "C" void __cdecl EffectKind8B_Start(void) {
    S()[9] = 0xB4;
    S()[1] = 1;
}

// original 0x48AB70 (state 1): the counter 0x903849 at 1 or more - the leader
// (ObjTrio, made Field_State and Sprite_Current) its +0x124 bit 6 set and
// animation 0x6F, its +7 kept to bits 0x11; Sprite_Current put back; +1 = 2.
extern "C" void __cdecl EffectKind8B_Wait(void) {
    const unsigned char n = Byte(at::kCounter2);
    unsigned char* const saved = Sprite_Current;
    if (n < 1) return;
    Field_State = ObjTrio;
    Sprite_Current = ObjTrio;
    ObjTrio[0x124] = static_cast<unsigned char>(ObjTrio[0x124] | 0x40);
    SH_CALL(Sprite_SetAnimation)(0x6F);
    const unsigned char b = static_cast<unsigned char>(ObjTrio[7] & 0x11);
    Sprite_Current = saved;
    ObjTrio[7] = b;
    saved[1] = 2;
}

// original 0x48ABC0 (state 2): the leader made Field_State and Sprite_Current;
// at its word +0x58 0xC and byte +0x4A 1 (the word read first) - animation
// 0x6E, its +7 kept to bits 0x11, Sprite_Current put back and +1 = 3. Else
// Sprite_Current is left on the leader (section 7).
extern "C" void __cdecl EffectKind8B_Pose(void) {
    const bool word = Word(ObjTrio + 0x58) == 0xC;
    unsigned char* const saved = Sprite_Current;
    Field_State = ObjTrio;
    Sprite_Current = ObjTrio;
    if (!word || ObjTrio[0x4A] != 1) return;
    SH_CALL(Sprite_SetAnimation)(0x6E);
    const unsigned char b = static_cast<unsigned char>(ObjTrio[7] & 0x11);
    Sprite_Current = saved;
    ObjTrio[7] = b;
    saved[1] = 3;
}

// original 0x48AC10 (state 3): the counter 0x903849 0; +1 = 1.
extern "C" void __cdecl EffectKind8B_Again(void) {
    unsigned char* const s = S();
    Byte(at::kCounter2) = 0;
    s[1] = 1;
}

// ===========================================================================
// Kind 0x8C: Effect_KindHandlers[0x8C] (0x655580), EffectKind8C_States (23;
// entries 2, 5..9, 14 and 19 BareRet)
// ===========================================================================

namespace {
unsigned short Pad() { return Input_Pressed; }
void CountUp() { SetWord(At(at::kCount), Word(At(at::kCount)) + 1u); }
// The leader's x (+0x34) or z (+0x38) moved by `d`, Field_State the leader
// (made so before the store, as the original).
void Nudge(unsigned off, U d) {
    const U v = UL(ObjTrio + off);
    Field_State = ObjTrio;
    SetUL(ObjTrio + off, v + d);
}
}  // namespace

// original 0x48AC30 (Effect_KindHandlers[0x8C], hidden after 0x48A8E0):
// jmp [EffectKind8C_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8C_Run(void) {
    Dispatch("EffectKind8C_Run", AddressOf(EffectKind8C_States), EffectKind8C_States_count);
}

// original 0x48AC50 (state 0): both count-downs a byte of kKind8CCounts by
// Rand & 0xF; the frame count, step count, direction, presses and target 0;
// +1 = 1.
extern "C" void __cdecl EffectKind8C_Start(void) {
    const unsigned r = static_cast<unsigned>(SH_CALL(Rand)()) & 0xFu;
    unsigned char* const s = S();
    const unsigned char v = At(at::kKind8CCounts + r)[0];
    Byte(at::kTimerA) = v;
    Byte(at::kTimerB) = v;
    Byte(at::kPresses) = 0;
    Byte(at::kSteps) = 0;
    SetWord(At(at::kDirection), 0);
    SetWord(At(at::kCount), 0);
    SetWord(At(at::kTarget), 0);
    s[1] = 1;
}

namespace {
// States 1 and 4: the first count-down down - at 0, `sound`, the count kept
// as the target, +1 = 0xA; else the pad's 0x40 pressed - counted, the frame
// count 0, +1 = 3; else +1 = `idle`.
void Kind8CWait(unsigned short sound, unsigned char pressed, unsigned char idle) {
    const unsigned char t = static_cast<unsigned char>(Byte(at::kTimerA) - 1);
    Byte(at::kTimerA) = t;
    if (t == 0) {
        SH_CALL(Sound_PlayEffect)(sound);
        const unsigned short c = Word(At(at::kCount));
        unsigned char* const s = S();
        SetWord(At(at::kTarget), c);
        Byte(at::kTimerA) = 0;
        s[1] = 0xA;
        return;
    }
    if (Pad() == 0x40) {
        unsigned char* const s = S();
        CountUp();
        Byte(at::kPresses) = 0;
        s[1] = pressed;
        return;
    }
    S()[1] = idle;
}
// States 3 and 0x11: the pad's `button` counted; the frame count up - at 0x1C
// or more +1 = 0x16, else +1 = `next`.
void Kind8CHold(unsigned short button, unsigned char next) {
    if (Pad() == button) CountUp();
    const unsigned char n = static_cast<unsigned char>(Byte(at::kPresses) + 1);
    Byte(at::kPresses) = n;
    S()[1] = n >= 0x1C ? 0x16 : next;
}
}  // namespace

// original 0x48ACA0 (state 1): Kind8CWait(0x200, 3, 1).
extern "C" void __cdecl EffectKind8C_WaitA(void) { Kind8CWait(0x200, 3, 1); }

// original 0x48AD10 (state 3): Kind8CHold(0x40, 4).
extern "C" void __cdecl EffectKind8C_PressA(void) { Kind8CHold(0x40, 4); }

// original 0x48AD50 (state 4): Kind8CWait(0x201, 1, 3).
extern "C" void __cdecl EffectKind8C_WaitB(void) { Kind8CWait(0x201, 1, 3); }

// original 0x48ADC0 (state 0xA): the target 0 - +1 = 0x16; else the frame
// count 0 and +1 = 0xB.
extern "C" void __cdecl EffectKind8C_Check(void) {
    if (Word(At(at::kTarget)) == 0) {
        S()[1] = 0x16;
        return;
    }
    unsigned char* const s = S();
    Byte(at::kPresses) = 0;
    s[1] = 0xB;
}

// original 0x48ADF0 (state 0xB): the pad's word one of 0x1000, 0x4000, 0x2000,
// 0x8000, 0x3000, 0x6000, 0x9000, 0xC000 - kept as the direction, sound 0x202,
// then by the word read again: 0x1000 / 0x3000 the leader's z down 0x800,
// 0x2000 / 0x6000 its x up, 0x4000 / 0xC000 z up, 0x8000 / 0x9000 x down (any
// other nothing); +1 = 0xC and +9 = 0xC. Any other word: nothing.
extern "C" void __cdecl EffectKind8C_Steer(void) {
    const unsigned short w = Pad();
    switch (w) {
    case 0x1000: case 0x4000: case 0x2000: case 0x8000:
    case 0x3000: case 0x6000: case 0x9000: case 0xC000: break;
    default: return;
    }
    SetWord(At(at::kDirection), w);
    SH_CALL(Sound_PlayEffect)(0x202);
    switch (Pad()) {
    case 0x1000: case 0x3000: Nudge(0x38, 0xFFFFF800u); break;
    case 0x2000: case 0x6000: Nudge(0x34, 0x800u); break;
    case 0x4000: case 0xC000: Nudge(0x38, 0x800u); break;
    case 0x8000: case 0x9000: Nudge(0x34, 0xFFFFF800u); break;
    default: break;
    }
    unsigned char* const s = S();
    s[1] = 0xC;
    S()[9] = 0xC;
}

// original 0x48AF10 (state 0xC): +9 down; it was 0 - +1 = 0xD.
extern "C" void __cdecl EffectKind8C_SteerWait(void) {
    unsigned char* const s = S();
    const unsigned char c = s[9];
    s[9] = static_cast<unsigned char>(c - 1);
    if (c == 0) S()[1] = 0xD;
}

// original 0x48AF30 (state 0xD): the step count up - at 8, the leader's x and
// z kept to multiples of 0x1000, +1 = 0xF, the count and the step count 0;
// else the leader moved back by the direction (0x1000 / 0x3000 z up, 0x2000 /
// 0x6000 x down, 0x4000 / 0xC000 z down, 0x8000 / 0x9000 x up) and +1 = 0xB.
extern "C" void __cdecl EffectKind8C_SteerBack(void) {
    const unsigned char n = static_cast<unsigned char>(Byte(at::kSteps) + 1);
    Byte(at::kSteps) = n;
    if (n >= 8) {
        const U z = UL(ObjTrio + 0x38) & 0xFFFFF000u;
        Field_State = ObjTrio;
        SetUL(ObjTrio + 0x38, z);
        const U x = UL(ObjTrio + 0x34) & 0xFFFFF000u;
        unsigned char* const s = S();
        SetUL(ObjTrio + 0x34, x);
        s[1] = 0xF;
        SetWord(At(at::kCount), 0);
        Byte(at::kSteps) = 0;
        return;
    }
    switch (Word(At(at::kDirection))) {
    case 0x1000: case 0x3000: Nudge(0x38, 0x800u); break;
    case 0x2000: case 0x6000: Nudge(0x34, 0xFFFFF800u); break;
    case 0x4000: case 0xC000: Nudge(0x38, 0xFFFFF800u); break;
    case 0x8000: case 0x9000: Nudge(0x34, 0x800u); break;
    default: break;
    }
    S()[1] = 0xB;
}

// original 0x48B070 (state 0xF): the pad's 0x20 - counted, +1 = 0x10.
extern "C" void __cdecl EffectKind8C_Ready(void) {
    if (Pad() != 0x20) return;
    unsigned char* const s = S();
    CountUp();
    s[1] = 0x10;
}

// original 0x48B090 (state 0x10): the second count-down down - at 0 +1 =
// 0x14; else the pad's 0x20 - counted, the frame count 0, +1 = 0x11 and the
// counter 0x903849 up; else Kind8CHold's count (0x1C: +1 = 0x16, else 0x10).
extern "C" void __cdecl EffectKind8C_CountA(void) {
    const unsigned char t = static_cast<unsigned char>(Byte(at::kTimerB) - 1);
    Byte(at::kTimerB) = t;
    if (t == 0) {
        S()[1] = 0x14;
        return;
    }
    if (Pad() == 0x20) {
        unsigned char* const s = S();
        CountUp();
        Byte(at::kPresses) = 0;
        s[1] = 0x11;
        Byte(at::kCounter2) = static_cast<unsigned char>(Byte(at::kCounter2) + 1);
        return;
    }
    const unsigned char n = static_cast<unsigned char>(Byte(at::kPresses) + 1);
    Byte(at::kPresses) = n;
    S()[1] = n >= 0x1C ? 0x16 : 0x10;
}

// original 0x48B100 (state 0x11): Kind8CHold(0x20, 0x12).
extern "C" void __cdecl EffectKind8C_CountPress(void) { Kind8CHold(0x20, 0x12); }

// original 0x48B140 (state 0x12): the second count-down down - at 0 it is set
// 0 again and +1 = 0x14; else the pad's 0x20 - counted, the frame count 0, +1
// = 0x10; else +1 = 0x11.
extern "C" void __cdecl EffectKind8C_CountB(void) {
    const unsigned char t = static_cast<unsigned char>(Byte(at::kTimerB) - 1);
    Byte(at::kTimerB) = t;
    if (t == 0) {
        unsigned char* const s = S();
        Byte(at::kTimerB) = 0;
        s[1] = 0x14;
        return;
    }
    if (Pad() == 0x20) {
        unsigned char* const s = S();
        CountUp();
        Byte(at::kPresses) = 0;
        s[1] = 0x10;
        return;
    }
    S()[1] = 0x11;
}

// original 0x48B190 (state 0x14): the count within one of the target - the
// frame count 0, +1 = 0x15; else +1 = 0x16.
extern "C" void __cdecl EffectKind8C_Judge(void) {
    const std::int32_t c = static_cast<std::int32_t>(UL(At(at::kCount)) & 0xFFFFu);
    const std::int32_t t = Word(At(at::kTarget));
    if (c >= t - 1 && c <= t + 1) {
        unsigned char* const s = S();
        Byte(at::kPresses) = 0;
        s[1] = 0x15;
        return;
    }
    S()[1] = 0x16;
}

// original 0x48B1D0 (state 0x15): the counter 0x903848 0, the chapter's step
// byte 0xF; Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind8C_Pass(void) {
    Byte(at::kCounter) = 0;
    Byte(at::kStep) = 0xF;
    SH_CALL(Effect_Release)();
}

// original 0x48B1F0 (state 0x16): the chapter's step byte 8; Effect_Release (a
// tail jump).
extern "C" void __cdecl EffectKind8C_Fail(void) {
    Byte(at::kStep) = 8;
    SH_CALL(Effect_Release)();
}

void Effect4B_Inject() {
    if (bof3::WantsShadow("effect_4b")) effect_4b::SelfTest();
    BOF3_INJECT(EffectKind87_Setup);
    BOF3_INJECT(EffectKind87_FadePanes);
    BOF3_INJECT(EffectKind87_StepPanes);
    BOF3_INJECT(EffectKind87_DrawPane);
    BOF3_INJECT(EffectKind87_Midpoint);
    BOF3_INJECT(EffectKind88_Run);
    BOF3_INJECT(EffectKind88_Start);
    BOF3_INJECT(EffectKind88_Emit);
    BOF3_INJECT(EffectKind88_EmitOn);
    BOF3_INJECT(EffectKind88_Fade);
    BOF3_INJECT(EffectKind88_ClearParticles);
    BOF3_INJECT(EffectKind88_FindParticle);
    BOF3_INJECT(EffectKind88_MoveParticles);
    BOF3_INJECT(EffectKind88_DrawBurst);
    BOF3_INJECT(EffectKind88_ScreenAngle);
    BOF3_INJECT(EffectKind88_InitParticle);
    BOF3_INJECT(EffectKind89_Run);
    BOF3_INJECT(EffectKind89_Start);
    BOF3_INJECT(EffectKind89_Hold);
    BOF3_INJECT(EffectKind89_Fade);
    BOF3_INJECT(EffectKind89_DrawTint);
    BOF3_INJECT(EffectKind9D_Run);
    BOF3_INJECT(EffectKind9D_Start);
    BOF3_INJECT(EffectKind9D_Pulse);
    BOF3_INJECT(EffectKind9D_Grow);
    BOF3_INJECT(EffectKind9D_Shrink);
    BOF3_INJECT(EffectKind9D_DrawPulse);
    BOF3_INJECT(EffectKind9D_DrawGlow);
    BOF3_INJECT(EffectKind9D_Project);
    BOF3_INJECT(EffectKind9F_Run);
    BOF3_INJECT(EffectKind9F_Start);
    BOF3_INJECT(EffectKind9F_Brighten);
    BOF3_INJECT(EffectKind9F_Hold);
    BOF3_INJECT(EffectKind9F_Fade);
    BOF3_INJECT(EffectKind9F_TintParty);
    BOF3_INJECT(EffectKindA4_Run);
    BOF3_INJECT(EffectKindA4_Brighten);
    BOF3_INJECT(EffectKindA4_Hold);
    BOF3_INJECT(EffectKindA4_MarkSprites);
    BOF3_INJECT(EffectKind8A_Run);
    BOF3_INJECT(EffectKind8A_Block);
    BOF3_INJECT(EffectKind8A_Lines);
    BOF3_INJECT(EffectKind8A_Unblock);
    BOF3_INJECT(EffectKind8A_DrawSegment);
    BOF3_INJECT(EffectKind8A_DrawRibbon);
    BOF3_INJECT(EffectKind8B_Run);
    BOF3_INJECT(EffectKind8B_Start);
    BOF3_INJECT(EffectKind8B_Wait);
    BOF3_INJECT(EffectKind8B_Pose);
    BOF3_INJECT(EffectKind8B_Again);
    BOF3_INJECT(EffectKind8C_Run);
    BOF3_INJECT(EffectKind8C_Start);
    BOF3_INJECT(EffectKind8C_WaitA);
    BOF3_INJECT(EffectKind8C_PressA);
    BOF3_INJECT(EffectKind8C_WaitB);
    BOF3_INJECT(EffectKind8C_Check);
    BOF3_INJECT(EffectKind8C_Steer);
    BOF3_INJECT(EffectKind8C_SteerWait);
    BOF3_INJECT(EffectKind8C_SteerBack);
    BOF3_INJECT(EffectKind8C_Ready);
    BOF3_INJECT(EffectKind8C_CountA);
    BOF3_INJECT(EffectKind8C_CountPress);
    BOF3_INJECT(EffectKind8C_CountB);
    BOF3_INJECT(EffectKind8C_Judge);
    BOF3_INJECT(EffectKind8C_Pass);
    BOF3_INJECT(EffectKind8C_Fail);
}
