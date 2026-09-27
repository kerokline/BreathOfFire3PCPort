// Three spell overlays compiled into the exe, round nine group S35
// (docs/magic_s35.md): the PSX's MAGIC167, MAGIC168 and MAGIC169.EMI,
// Magic_Rows rows 91, 117 and 97. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Last Resort, Cure and Benediction; the
// names below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC167 0x4EF620..0x4F0666: one ring child and eight beam children
//     (kind 1, 0x47) at the source sprite; the ring draws a gouraud disc and a
//     band round it, each beam a column of four gouraud quads a row, rising
//     round the centre, and two trails of flat tiles;
//   - MAGIC168 0x4F0670..0x4F119E: a pool of 128 motes of its own
//     (0x6AE910, 0x2C bytes each, 0x6AFF10 the current one), each flying up
//     from the owner and drawn as an eight-point star of gouraud triangles and
//     two sets of gouraud lines;
//   - MAGIC169 0x4F11A0..0x4F1E36: a textured halo over the side's centre and,
//     when the target is the party side, one child (kind 1, 8) per party
//     member that tints the member and releases 24 motes of a second pool
//     (0x6AFF18, 0x84 bytes each, run as tasks), which spiral up round it.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s35.h"

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
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

constexpr std::uint32_t kPartyCount = 0x904AB0;    // u8: the party's size

// MAGIC168's pool: 128 motes of 0x2C bytes, the one being run at 0x6AFF10.
constexpr std::uint32_t kCurePool = 0x6AE910;
constexpr std::uint32_t kCureStride = 0x2C;
constexpr unsigned kCureCount = 0x80;
constexpr std::uint32_t kCureCurrent = 0x6AFF10;
// MAGIC169's pool: 80 motes of 0x84 bytes, run as tasks (+0x80 the owner).
constexpr std::uint32_t kBlessPool = 0x6AFF18;
constexpr std::uint32_t kBlessStride = 0x84;
constexpr unsigned kBlessCount = 0x50;

// MAGIC168's .data, indexed by a mote's kind (Cure_Start sets 3) or by its
// number: the motes per kind, their delay step and lifetime base, a colour
// triple per kind and variant, a start offset per number.
constexpr std::uint32_t kCureOffsets = 0x65C080;    // 32 used (& 0x1F) of s16 dx, dy
constexpr std::uint32_t kCureColours = 0x65C120;    // 3 bytes by kind x 4 + variant
constexpr std::uint32_t kCureCounts = 0x65C15C;
constexpr std::uint32_t kCureDelays = 0x65C164;
constexpr std::uint32_t kCureLifetimes = 0x65C16C;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char* Cur() { return Pointer(kCureCurrent); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }
void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(kS + k), static_cast<std::int32_t>(v)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `imul` alone: the 32-bit product, wrapped.
std::int32_t Mul32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
std::int32_t Add32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
// `cdq / and edx, 2^n - 1 / add / sar n` (or `cdq / sub eax, edx / sar 1`):
// a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `fild dword` then `fst(p) dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The originals index the party records by a byte, unchecked.
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* CureRecord(unsigned n) { return Mem(kCurePool + n * kCureStride); }
unsigned char* BlessRecord(unsigned n) { return Mem(kBlessPool + n * kBlessStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using Fn1 = void (__cdecl*)(int);
using Fn2 = void (__cdecl*)(unsigned, unsigned);
using FnAlloc = std::uint32_t (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call1(std::uint32_t address, int a) { MH_AT(Fn1, address)(a); }
void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_AT(Fn2, address)(a, b); }
unsigned Alloc(std::uint32_t address) { return MH_AT(FnAlloc, address)() & 0xFFu; }

// The phase handlers of other units a table holds, and the one tail jump to
// another unit (docs/magic_s35.md section 3): called by their addresses.
constexpr std::uint32_t kFreePoolSlot = 0x4F6290;   // MAGIC219: Sprite_Current bytes 0..4 cleared

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The projections with the arguments the originals push: the depth and a
// flag pointer after the outputs (the flag one more than the callee takes).
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, unsigned char*, unsigned char*, unsigned char*,
                               long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S35_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp1(unsigned char* sxy) {
    long p, flag;
    S35_AS(Rtp1Fn, Gte_RotTransPers)(VP(0), sxy, &p, &flag);
}
// Three points at +8, +0x18, +0x28 (a gouraud triangle's).
void Rtp3(unsigned char* prim) {
    long p, flag;
    S35_AS(Rtp3Fn, Gte_RotTransPers3)(VP(0), VP(8), VP(0x10), prim + 8, prim + 0x18, prim + 0x28, &p, &flag);
}
// Four points at +8, +0x18, +0x28, +0x38 (a gouraud quad's).
void Rtp4(unsigned char* prim) {
    long p, flag;
    S35_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &p, &flag);
}
// Math_Sin / Math_Cos of `angle` times a scratch value the original reads
// after the call (the product wrapped, >> 12): the dword 0x903850, or a s16.
int SinSD0(int angle) {
    const int v = MH_CALL(Math_Sin)(angle);
    return Mul12(v, SD(0));
}
int SinSS(int angle, unsigned k) {
    const int v = MH_CALL(Math_Sin)(angle);
    return Mul12(v, SS(k));
}
int CosSS(int angle, unsigned k) {
    const int v = MH_CALL(Math_Cos)(angle);
    return Mul12(v, SS(k));
}
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }
void Link(std::int32_t x, std::int32_t z, int dy, unsigned size) {
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), dy, size);
}
// Linked at the sprite's +0x34 / +0x38 (the task's or the mote's).
void LinkAt(const unsigned char* at, int dy, unsigned size) { Link(Long(at + 0x34), Long(at + 0x38), dy, size); }
// Linked at a MAGIC168 mote's +0x14 / +0x18.
void LinkCure(int dy, unsigned size) {
    const unsigned char* const r = Cur();
    Link(Long(r + 0x14), Long(r + 0x18), dy, size);
}

}  // namespace

#define S35_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC167 (row 91, Last Resort read one id down)

// original 0x4EF620: the kind-2 task. A six-entry stack table by +1:
// LastResort_Start, MAGIC077's Revive_TintSource, BattleFx_Brighten,
// LastResort_WaitChildren, MAGIC086's Barrier_Fade, BattleFx_Finish.
S35_EXPORT void __cdecl LastResort_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::LastResort_Start,       bof3::addr::Revive_TintSource,
                                                 bof3::addr::BattleFx_Brighten,      bof3::addr::LastResort_WaitChildren,
                                                 bof3::addr::Barrier_Fade,           bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 6) PastTable("LastResort_Task", phase, 6);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4EF670: the source sprite's (0x904B4C) +8, +0x34, +0x38, +0x3C
// to the task; +0xB 0, +9 0x10, +1 on. One ring child (kind 1, 0x47: +1 0,
// +9 0) at the task's +0x34 / +0x38 / word +0x3E, then eight beam children (+1
// 1, +0xB (i & 3) << 3, +9 (i >> 2) x 0x28 + 0x11) at its +0x34..+0x3C, each
// counted in +0xB; sound 0x100.
S35_EXPORT void __cdecl LastResort_Start(void) {
    const unsigned char* const src = Pointer(at::kSource);
    Sc()[8] = src[8];
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetLong(Sc() + 0x3C, Long(src + 0x3C));
    Sc()[0xB] = 0;
    Sc()[9] = 0x10;
    Inc(Sc()[1]);
    {
        unsigned char* const c = TaskSlot(NewTask(0x47));
        unsigned char* const s = Sc();
        SetLong(c + 0x80, static_cast<std::int32_t>(Key(s)));
        c[1] = 0;
        c[9] = 0;
        SetLong(c + 0x34, Long(s + 0x34));
        SetLong(c + 0x38, Long(s + 0x38));
        SetWord(c + 0x3E, Word(s + 0x3E));
        Inc(s[0xB]);
    }
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const c = TaskSlot(NewTask(0x47));
        unsigned char* const s = Sc();
        SetLong(c + 0x80, static_cast<std::int32_t>(Key(s)));
        c[1] = 1;
        c[0xB] = static_cast<unsigned char>((i & 3) << 3);
        c[9] = static_cast<unsigned char>((i >> 2) * 0x28 + 0x11);
        SetLong(c + 0x34, Long(s + 0x34));
        SetLong(c + 0x38, Long(s + 0x38));
        SetLong(c + 0x3C, Long(s + 0x3C));
        Inc(s[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4EF7C0: +1 on once +0xB (the children still running) is 1 or less.
// Also entry 1 of Snap_Task (MAGIC013) and a table entry of MAGIC088's.
S35_EXPORT void __cdecl LastResort_WaitChildren(void) {
    if (Sc()[0xB] <= 1) Inc(Sc()[1]);
}

// original 0x4EF7D0: the children's kind-1 task (parameter 0x47), a jmp
// through LastResortChild_Kinds (two entries: the ring, the beam) by +1,
// unchecked.
S35_EXPORT void __cdecl LastResortChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::LastResortRing_Run, bof3::addr::LastResortBeam_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("LastResortChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4EF7F0: a call through LastResortRing_Steps (three entries:
// BarrierRing_Grow, MagicFx_WaitOwnerChildren, MagicFx_CountDownRelease) by +2;
// then while +0 is set: the actor matrix, dword 0x903854 +9 x 5, the disc and
// the band, the matrix popped.
S35_EXPORT void __cdecl LastResortRing_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::BarrierRing_Grow, bof3::addr::MagicFx_WaitOwnerChildren,
                                                bof3::addr::MagicFx_CountDownRelease};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("LastResortRing_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    SetSD(4, Sc()[9] * 5u);
    Call0(bof3::addr::LastResortRing_DrawDisc);
    Call0(bof3::addr::LastResortRing_DrawBand);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EF840: +2 on once the owner's +0xB (its children still running)
// is 1 or less. A step of thirteen overlays' tables.
S35_EXPORT void __cdecl MagicFx_WaitOwnerChildren(void) {
    if (Owner()[0xB] <= 1) Inc(Sc()[2]);
}

// original 0x4EF860: a disc of sixteen semi-transparent gouraud triangles
// round the matrix's origin, radius 0xC0: the centre shaded byte 0x903854 in
// all three channels, the rim (s, s / 2, 1) with s the dword 0x903854; each
// committed to layer 5 between two draw-mode packets.
S35_EXPORT void __cdecl LastResortRing_DrawDisc(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetVW(0x10, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(0), 0xC0)));
    SetVW(0x12, static_cast<unsigned>(Mul12(MH_CALL(Math_Cos)(0), 0xC0)));
    for (int angle = 0x100; angle < 0x1100; angle += 0x100) {
        const std::uint16_t y = VW(0x12);
        const std::uint16_t x = VW(0x10);
        SetVW(0, 0);
        SetVW(2, 0);
        SetVW(8, x);
        SetVW(0xA, y);
        SetVW(0x10, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(angle), 0xC0)));
        const int c = MH_CALL(Math_Cos)(angle);
        unsigned char* const p = Gfx_PacketNext;
        SetVW(0x12, static_cast<unsigned>(Mul12(c, 0xC0)));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp3(p);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = SB(4);
        p[5] = SB(4);
        p[6] = SB(4);
        p[0x14] = SB(4);
        p[0x16] = 1;
        p[0x15] = static_cast<unsigned char>(DivPow2(SD(4), 1));
        p[0x24] = SB(4);
        p[0x25] = static_cast<unsigned char>(DivPow2(SD(4), 1));
        p[0x26] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4EFA00: a band of sixteen semi-transparent
// gouraud quads between radius 0xC0 and 0x10 round the origin: the outer
// edge (s, s / 2, 1) with s the dword 0x903854, the inner edge (1, 1, 1).
S35_EXPORT void __cdecl LastResortRing_DrawBand(void) {
    DrawMode(0x35);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    SetVW(8, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(0), 0xC0)));
    SetVW(0xA, static_cast<unsigned>(Mul12(MH_CALL(Math_Cos)(0), 0xC0)));
    SetVW(0x18, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(0), 0x100)));
    SetVW(0x1A, static_cast<unsigned>(Mul12(MH_CALL(Math_Cos)(0), 0x100)));
    for (int angle = 0x100; angle < 0x1100; angle += 0x100) {
        const std::uint16_t inner_y = VW(0x1A);
        const std::uint16_t outer_x = VW(8);
        const std::uint16_t outer_y = VW(0xA);
        SetVW(0, outer_x);
        const std::uint16_t inner_x = VW(0x18);
        SetVW(2, outer_y);
        SetVW(0x10, inner_x);
        SetVW(0x12, inner_y);
        SetVW(8, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(angle), 0xC0)));
        SetVW(0xA, static_cast<unsigned>(Mul12(MH_CALL(Math_Cos)(angle), 0xC0)));
        SetVW(0x18, static_cast<unsigned>(Mul12(MH_CALL(Math_Sin)(angle), 0x100)));
        const int c = MH_CALL(Math_Cos)(angle);
        unsigned char* const p = Gfx_PacketNext;
        SetVW(0x1C, 0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(c, 0x100)));
        SetVW(0x14, 0);
        SetVW(0xC, 0);
        SetVW(4, 0);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[4] = SB(4);
        p[6] = 1;
        p[5] = static_cast<unsigned char>(DivPow2(SD(4), 1));
        p[0x14] = SB(4);
        p[0x16] = 1;
        p[0x15] = static_cast<unsigned char>(DivPow2(SD(4), 1));
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    DrawMode(0x15);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4EFC00: a call through LastResortBeam_Steps (four entries) by +2;
// then while +0 and +2 are set: the actor matrix, the column while +2 is below
// 3, the two trails (2 and 3), the matrix popped.
S35_EXPORT void __cdecl LastResortBeam_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::LastResortBeam_Wait, bof3::addr::LastResortBeam_Rise,
                                                bof3::addr::LastResortBeam_Shrink, bof3::addr::LastResortBeam_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("LastResortBeam_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] == 0 || s[2] == 0) return;
    }
    MH_CALL(MagicFx_PushActorMatrix)();
    if (Sc()[2] < 3) Call0(bof3::addr::LastResortBeam_DrawColumn);
    Call1(bof3::addr::LastResortBeam_DrawSparks, 2);
    Call1(bof3::addr::LastResortBeam_DrawSparks, 3);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EFC50: +9 down; at 0 +0xA 1 and +2 on.
S35_EXPORT void __cdecl LastResortBeam_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4EFC80: +0xB up by 2, +9 up, +0xA up while below 0x10; at +9
// 0x18 +2 on.
S35_EXPORT void __cdecl LastResortBeam_Rise(void) {
    AddB(Sc()[0xB], 2);
    Inc(Sc()[9]);
    if (Sc()[0xA] < 0x10) Inc(Sc()[0xA]);
    if (Sc()[9] == 0x18) Inc(Sc()[2]);
}

// original 0x4EFCC0: +0xB up by 2, +9 up, +0xA down; at +9 0x28 +2 on.
S35_EXPORT void __cdecl LastResortBeam_Shrink(void) {
    AddB(Sc()[0xB], 2);
    Inc(Sc()[9]);
    Dec(Sc()[0xA]);
    if (Sc()[9] == 0x28) Inc(Sc()[2]);
}

// original 0x4EFD00: +0xA up; at 0x18 the owner's +0xB down and a tail jmp to
// BattleTask_FreeCurrent.
S35_EXPORT void __cdecl LastResortBeam_End(void) {
    Inc(Sc()[0xA]);
    if (Sc()[0xA] != 0x18) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EFD30: the column. Rows i = 1 .. +0xA - 1, each a ring point at
// angle ((+0xB - i) & 0x1F) << 7, radius (i - +9) x 2 + 0xC0 (dword
// 0x903850), height (i - +9) x 12 (dword 0x903858), joined to the previous
// row's point by four semi-transparent gouraud quads stacked in height, each
// linked at the point (x << 9 + +0x34, y << 9 + +0x38) under a draw-mode
// packet: the shade 3 x (0x10 - i) (from step 2 on, 3 x (0x10 - min(+9 + i -
// 0x18, 0xF))) in dword 0x903854, the edges 12 x that, the first row's lower
// edge black. The heights of the quads: -(3 +0xA - i / 2) .. 0, then the
// running sums the original keeps in its frame (half of 2 +0xA, then of the
// previous row's 3 +0xA - i / 2, plus the previous row's height word).
S35_EXPORT void __cdecl LastResortBeam_DrawColumn(void) {
    std::uint16_t px, py;   // di / bp: the previous row's point
    std::int32_t l10;       // [esp+0x1C]: 2 +0xA, then the previous row's 3 +0xA - i / 2
    std::uint16_t l2c;      // [esp+0x38]'s low word: the previous row's height word
    unsigned char* s = Sc();
    {
        const std::uint32_t angle = static_cast<std::uint32_t>(s[0xB] & 0x1F) << 7;
        SetSD(0xC, angle);
        SetSD(0, static_cast<std::uint32_t>(Shl(0x60 - s[9], 1)));
        SetSD(8, static_cast<std::uint32_t>(Shl(-(3 * static_cast<int>(s[9])), 2)));
        l10 = 2 * static_cast<std::int32_t>(s[0xA]);
        const int sn = MH_CALL(Math_Sin)(static_cast<int>(angle));
        px = static_cast<std::uint16_t>(Mul12(sn, SD(0)));
        const int cs = MH_CALL(Math_Cos)(SD(0xC));
        py = static_cast<std::uint16_t>(Mul12(cs, SD(0)));
        l2c = SW(8);
        s = Sc();
        if (s[0xA] <= 1) return;
    }
    int i = 1;
    for (;;) {
        const std::uint32_t angle = static_cast<std::uint32_t>((s[0xB] - i) & 0x1F) << 7;
        SetSD(0xC, angle);
        SetSD(0, static_cast<std::uint32_t>(2 * (i - s[9]) + 0xC0));
        SetSD(8, static_cast<std::uint32_t>(12 * (i - s[9])));
        const std::int32_t top = 3 * static_cast<int>(s[0xA]) - DivPow2(i, 1);   // esi, then [esp+0x34]
        SetVW(8, px);
        SetVW(0xA, py);
        SetVW(0xC, static_cast<unsigned>(l2c - l10));
        SetVW(0x18, px);
        std::int32_t l20 = DivPow2(l10, 1);
        SetVW(0x1A, py);
        const std::int32_t l1c = l2c - l20;
        SetVW(0x1C, static_cast<unsigned>(l1c));
        // the first quad: from the height -top up to the previous row's words
        SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(angle))));
        {
            const int cs = MH_CALL(Math_Cos)(SD(0xC));
            const int cy = Mul12(cs, SD(0));
            const std::int32_t h = SD(8);
            SetVW(4, static_cast<unsigned>(h - top));
            const std::uint16_t x0 = VW(0);
            SetVW(2, static_cast<unsigned>(cy));
            SetVW(0x12, static_cast<unsigned>(cy));
            SetVW(0x10, x0);
            SetVW(0x14, static_cast<unsigned>(h - DivPow2(top, 1)));
        }
        const std::int32_t half = DivPow2(top, 1);   // [esp+0x18]
        s = Sc();
        const std::int32_t x = Add32(Shl(VS(0x18), 9), Long(s + 0x34));
        const std::int32_t z = Add32(Shl(VS(0x1A), 9), Long(s + 0x38));
        DrawMode(0x35);
        Link(x, z, 0, 0xC);
        unsigned char* p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        int level;
        s = Sc();
        if (s[2] == 2) {
            int e = static_cast<int>(s[9]) + i - 0x18;
            if (e > 0x10) e = 0xF;
            SetSD(4, static_cast<std::uint32_t>(3 * (0x10 - e)));
            level = 0x10 - e;
        } else {
            level = 0x10 - i;
            SetSD(4, static_cast<std::uint32_t>(3 * level));
        }
        const auto edge = static_cast<unsigned char>(level * 12);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = edge;
        p[0x25] = SB(4);
        p[0x26] = SB(4);
        if (i == 1) {
            p[0x34] = 1;
            p[0x35] = 1;
            p[0x36] = 1;
        } else {
            p[0x34] = edge;
            p[0x35] = SB(4);
            p[0x36] = SB(4);
        }
        Link(x, z, 0, 0x44);
        // the second quad
        {
            const std::uint32_t a2 = static_cast<std::uint32_t>(SD(0xC));
            SetVW(8, px);
            SetVW(0xA, py);
            SetVW(0xC, static_cast<unsigned>(l1c));
            SetVW(0x18, px);
            SetVW(0x1A, py);
            SetVW(0x1C, l2c);
            SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(a2))));
            const int cs = MH_CALL(Math_Cos)(SD(0xC));
            const int cy = Mul12(cs, SD(0));
            const std::int32_t h = SD(8);
            SetVW(0x14, static_cast<unsigned>(h));
            SetVW(4, static_cast<unsigned>(h - half));
            p = Gfx_PacketNext;
            const std::uint16_t x0 = VW(0);
            SetVW(2, static_cast<unsigned>(cy));
            SetVW(0x10, x0);
            SetVW(0x12, static_cast<unsigned>(cy));
        }
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        p[4] = edge;
        p[5] = SB(4);
        p[6] = SB(4);
        p[0x14] = edge;
        p[0x15] = SB(4);
        p[0x16] = SB(4);
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = edge;
        Link(x, z, 0, 0x44);
        // the third quad
        {
            SetVW(0xC, l2c);
            SetVW(8, px);
            SetVW(0xA, py);
            SetVW(0x18, px);
            l20 = Add32(l20, l2c);
            SetVW(0x1C, static_cast<unsigned>(l20));
            const std::uint32_t a3 = static_cast<std::uint32_t>(SD(0xC));
            SetVW(0x1A, py);
            SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(a3))));
            const int cs = MH_CALL(Math_Cos)(SD(0xC));
            const std::int32_t h = SD(8);
            p = Gfx_PacketNext;
            const std::uint16_t x0 = VW(0);
            const int cy = Mul12(cs, SD(0));
            SetVW(2, static_cast<unsigned>(cy));
            SetVW(0x12, static_cast<unsigned>(cy));
            SetVW(4, static_cast<unsigned>(h));
            SetVW(0x10, x0);
            SetVW(0x14, static_cast<unsigned>(half + h));
        }
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u}) p[k] = edge;
        p[0x25] = SB(4);
        p[0x26] = SB(4);
        p[0x34] = edge;
        p[0x35] = SB(4);
        p[0x36] = SB(4);
        Link(x, z, 0, 0x44);
        // the fourth quad, under its own draw-mode packet
        {
            SetVW(0xC, static_cast<unsigned>(l20));
            const std::uint32_t a4 = static_cast<std::uint32_t>(SD(0xC));
            SetVW(8, px);
            SetVW(0xA, py);
            SetVW(0x18, px);
            SetVW(0x1A, py);
            SetVW(0x1C, static_cast<unsigned>(Add32(l10, l2c)));
            SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(a4))));
            const int cs = MH_CALL(Math_Cos)(SD(0xC));
            const std::int32_t h = SD(8);
            SetVW(4, static_cast<unsigned>(half + h));
            px = VW(0);
            const int cy = Mul12(cs, SD(0));
            SetVW(0x14, static_cast<unsigned>(Add32(h, top)));
            SetVW(2, static_cast<unsigned>(cy));
            SetVW(0x12, static_cast<unsigned>(cy));
            py = static_cast<std::uint16_t>(cy);
            unsigned char* const mode = Gfx_PacketNext;
            SetVW(0x10, px);
            l2c = static_cast<std::uint16_t>(h);
            l10 = top;
            MH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0x35, 0);
        }
        Link(x, z, 0, 0xC);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        if (i == 1) {
            p[0x14] = 1;
            p[0x15] = 1;
            p[0x16] = 1;
        } else {
            p[0x14] = edge;
            p[0x15] = SB(4);
            p[0x16] = SB(4);
        }
        p[4] = edge;
        p[5] = SB(4);
        p[6] = SB(4);
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        Link(x, z, 0, 0x44);
        s = Sc();
        ++i;
        if (i >= static_cast<int>(s[0xA])) return;
    }
}

// original 0x4F0470 (cdecl, one argument k: 2 or 3 from LastResortBeam_Run):
// a trail of semi-transparent flat tiles up the column, one every k rows
// from row 1 while the row is below +9 (from step 3 on, while +0xA + the row
// is at most 0x18), the shade 0x7B down by 5 k a tile (from step 3 on, 0x80 -
// 5 (+0xA + row)) until it is 8 or less; each tile's point the row's (as the
// column's) lifted by (k + 2) x row + Rand & 3 + 8, kept at or below 0, under
// a draw-mode packet.
S35_EXPORT void __cdecl LastResortBeam_DrawSparks(int k) {
    std::int32_t shade = 0x7B;         // [esp+0x10]
    const std::int32_t step = -5 * k;  // [esp+0x14]
    int row = 1;                       // ebp
    for (;;) {
        unsigned char* s = Sc();
        if (row >= static_cast<int>(s[9])) return;
        std::int32_t lift;
        if (s[2] == 3) {
            const int r = static_cast<int>(s[0xA]) + row;
            if (r > 0x18) return;
            const std::uint32_t rnd = RandCall();
            lift = Add32(static_cast<std::int32_t>(rnd & 3), Mul32(k + 2, r));
            SetSD(4, static_cast<std::uint32_t>(0x80 - 5 * r));
        } else {
            const std::uint32_t rnd = RandCall();
            lift = Add32(static_cast<std::int32_t>(rnd & 3), Mul32(k + 2, row));
            SetSD(4, static_cast<std::uint32_t>(shade));
        }
        s = Sc();
        const std::uint32_t angle = static_cast<std::uint32_t>((s[0xB] - row) & 0x1F) << 7;
        SetSD(0xC, angle);
        SetSD(0, static_cast<std::uint32_t>(2 * (row - s[9]) + 0xC0));
        SetSD(8, static_cast<std::uint32_t>(12 * (row - s[9])));
        SetVW(0, static_cast<unsigned>(SinSD0(static_cast<int>(angle))));
        const int cs = MH_CALL(Math_Cos)(SD(0xC));
        const int cy = Mul12(cs, SD(0));
        const std::int32_t height = Add32(Add32(SD(8), lift), 8);
        SetVW(2, static_cast<unsigned>(cy));
        SetVW(4, static_cast<unsigned>(height));
        if (static_cast<short>(height) > 0) SetVW(4, 0);
        s = Sc();
        const std::int32_t x = Add32(Shl(VS(0), 9), Long(s + 0x34));
        const std::int32_t z = Add32(Shl(static_cast<short>(cy), 9), Long(s + 0x38));
        DrawMode(0x35);
        Link(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetTile1)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp1(p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        p[4] = SB(4);
        p[5] = SB(4);
        p[6] = SB(4);
        Link(x, z, 0, 0x44);
        shade = Add32(shade, step);
        row += k;
        if (shade <= 8) return;
    }
}

// ===========================================================================
// MAGIC168 (row 117, Cure read one id down)

// original 0x4F0670: the kind-2 task. A six-entry stack table by +1:
// Cure_Start, BattleFx_TintActor, BattleFx_Brighten, BattleFx_WaitStep4,
// Sparkle_End, BattleFx_Finish. Then every mote of the pool with bit 0 is run
// through CureMote_Task with 0x6AFF10 at it and its +0x28 as the owner cell
// 0x93B940, the owner (read once after the step) put back after each.
S35_EXPORT void __cdecl Cure_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Cure_Start,         bof3::addr::BattleFx_TintActor,
                                                 bof3::addr::BattleFx_Brighten,  bof3::addr::BattleFx_WaitStep4,
                                                 bof3::addr::Sparkle_End,        bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 6) PastTable("Cure_Task", phase, 6);
    magic_harness::Phase(kPhases[phase])();
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned n = 0; n < kCureCount; ++n) {
        unsigned char* const r = CureRecord(n);
        if ((r[0] & 1) == 0) continue;
        const std::int32_t mote_owner = Long(r + 0x28);
        magic_harness::SetPointer(kCureCurrent, r);
        SetLong(Mem(at::kOwner), mote_owner);
        Call0(bof3::addr::CureMote_Task);
        SetLong(Mem(at::kOwner), owner);
    }
}

// original 0x4F0700: every mote's +0 / +1 / +2 cleared; +4 3 (the kind); the
// source sprite's +8, +0x34, +0x38, +0x3C to the task;
// BattleActor_UpdateScreenXY; +0xB 0, +9 8, +0xA 0, +1 on; sound 0x100. Then
// Cure_MoteCounts[+4] motes (CureMote_Alloc; none free: skipped): +0x28 the
// task, +1 / +2 0, +3 Rand & 3, +4 the kind, +7 the number n, +5
// Cure_MoteDelays[kind] x (n >> 2) + 1 (bytes); each counted in +0xB.
S35_EXPORT void __cdecl Cure_Start(void) {
    for (unsigned n = 0; n < kCureCount; ++n) {
        unsigned char* const r = CureRecord(n);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    Sc()[4] = 3;
    {
        const unsigned char* const src = Pointer(at::kSource);
        Sc()[8] = src[8];
        SetLong(Sc() + 0x34, Long(src + 0x34));
        SetLong(Sc() + 0x38, Long(src + 0x38));
        SetLong(Sc() + 0x3C, Long(src + 0x3C));
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[0xB] = 0;
    Sc()[9] = 8;
    Sc()[0xA] = 0;
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayById)(0x100);
    if (Mem(kCureCounts)[Sc()[4]] == 0) return;
    unsigned char n = 0;
    do {
        const unsigned slot = Alloc(bof3::addr::CureMote_Alloc);
        if (slot != 0xFF) {
            unsigned char* const r = CureRecord(slot);
            SetLong(r + 0x28, static_cast<std::int32_t>(Key(Sc())));
            r[1] = 0;
            r[2] = 0;
            const std::uint32_t rnd = RandCall();
            unsigned char* const s = Sc();
            r[3] = static_cast<unsigned char>(rnd & 3);
            r[4] = s[4];
            r[7] = n;
            r[5] = static_cast<unsigned char>(Mem(kCureDelays)[s[4]] * (n >> 2) + 1);
            Inc(s[0xB]);
        }
        ++n;
    } while (n < Mem(kCureCounts)[Sc()[4]]);
}

// original 0x4F0850: a mote's task, a jmp through CureMote_TaskTable (one
// entry) by the current mote's +1, unchecked.
S35_EXPORT void __cdecl CureMote_Task(void) {
    const unsigned phase = Cur()[1];
    if (phase >= 1) PastTable("CureMote_Task", phase, 1);
    magic_harness::Phase(bof3::addr::CureMote_Run)();
}

// original 0x4F0870: a three-entry stack table by the mote's +2 (CureMote_Wait,
// _Rise, _Fade); then while the mote's bit 0 and +2 are set: its star, and
// while +0xC & 3: its rays (Frame_Counter >> 1, radius +6) and arcs
// ((Frame_Counter >> 1) + 4, radius +6 + +6 / 2).
S35_EXPORT void __cdecl CureMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::CureMote_Wait, bof3::addr::CureMote_Rise,
                                                bof3::addr::CureMote_Fade};
    const unsigned phase = Cur()[2];
    if (phase >= 3) PastTable("CureMote_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const r = Cur();
        if ((r[0] & 1) == 0 || r[2] == 0) return;
    }
    Call0(bof3::addr::CureMote_DrawStar);
    if ((Cur()[0xC] & 3) == 0) return;
    Call2(bof3::addr::CureMote_DrawRays, Frame_Counter >> 1, Cur()[6]);
    const unsigned w = Cur()[6];
    Call2(bof3::addr::CureMote_DrawArcs, (Frame_Counter >> 1) + 4, (w >> 1) + w);
}

// original 0x4F0900: the mote's +5 down; at 0 it starts at its owner: +0x14 /
// +0x18 / +0x1C the owner's +0x34 / +0x38 / +0x3C, words +0x20 / +0x22 the
// owner's screen point +0x2E / +0x30, moved by CureMote_Offsets[+7 & 0x1F]
// (x away from 0 by Rand & 7, y up by it plus Rand & 7); word +8 the x, word
// +0xA Rand & 1, word +0xC Rand; +7 (Rand & 6) + CureMote_Lifetimes[+4], +5
// 0x10, +6 0, +2 on. Each Rand's pointer taken before the call, each table
// read after it.
S35_EXPORT void __cdecl CureMote_Wait(void) {
    Dec(Cur()[5]);
    if (Cur()[5] != 0) return;
    SetLong(Cur() + 0x14, Long(Owner() + 0x34));
    SetLong(Cur() + 0x18, Long(Owner() + 0x38));
    SetLong(Cur() + 0x1C, Long(Owner() + 0x3C));
    SetWord(Cur() + 0x20, Word(Owner() + 0x2E));
    SetWord(Cur() + 0x22, Word(Owner() + 0x30));
    {
        unsigned char* const x = Cur() + 0x20;
        const short dx = static_cast<short>(Word(Mem(kCureOffsets + (Cur()[7] & 0x1Fu) * 4)));
        if (dx < 0) {
            const std::uint32_t rnd = RandCall() & 7;
            const std::uint16_t d = Word(Mem(kCureOffsets + (Cur()[7] & 0x1Fu) * 4));
            SetWord(x, Word(x) + static_cast<std::uint16_t>(d - rnd));
        } else {
            const std::uint32_t rnd = RandCall() & 7;
            const std::uint16_t d = Word(Mem(kCureOffsets + (Cur()[7] & 0x1Fu) * 4));
            SetWord(x, Word(x) + static_cast<std::uint16_t>(rnd + d));
        }
    }
    {
        unsigned char* const y = Cur() + 0x22;
        const std::uint32_t rnd = RandCall() & 7;
        const std::uint16_t d = Word(Mem(kCureOffsets + (Cur()[7] & 0x1Fu) * 4 + 2));
        SetWord(y, Word(y) - static_cast<std::uint16_t>(rnd + d));
    }
    SetWord(Cur() + 8, Word(Cur() + 0x20));
    {
        const std::uint32_t rnd = RandCall();
        SetWord(Cur() + 0xA, rnd & 1);
    }
    {
        const std::uint32_t rnd = RandCall();
        SetWord(Cur() + 0xC, rnd);
    }
    {
        const std::uint32_t rnd = RandCall();
        unsigned char* const r = Cur();
        r[7] = static_cast<unsigned char>((rnd & 6) + Mem(kCureLifetimes)[r[4]]);
    }
    Cur()[5] = 0x10;
    Cur()[6] = 0;
    Inc(Cur()[2]);
}

namespace {
// CureMote_Rise / _Fade's common half: word +0xC up; word 0x903856 (+0xC &
// 0x3F) << 6; word +0x20 word +8 + Math_Sin of it x 24 >> 12; word +0x22 +=
// word +0xA.
void CureMoteSway() {
    SetWord(Cur() + 0xC, Word(Cur() + 0xC) + 1u);
    const unsigned angle = (Cur()[0xC] & 0x3Fu) << 6;
    SetSW(6, angle);
    const int sn = MH_CALL(Math_Sin)(static_cast<short>(angle));
    {
        unsigned char* const r = Cur();
        SetWord(r + 0x20, static_cast<unsigned>(Mul12(sn, 24)) + Word(r + 8));
    }
    {
        unsigned char* const r = Cur();
        SetWord(r + 0x22, Word(r + 0x22) + Word(r + 0xA));
    }
}
}  // namespace

// original 0x4F0A70: the sway; +6 up; at +7 +2 on.
S35_EXPORT void __cdecl CureMote_Rise(void) {
    CureMoteSway();
    Inc(Cur()[6]);
    if (Cur()[6] == Cur()[7]) Inc(Cur()[2]);
}

// original 0x4F0AE0: the sway; +6 down every fourth frame; +5 down; at 0 the
// owner's +0xB down and a tail jmp to CureMote_Free.
S35_EXPORT void __cdecl CureMote_Fade(void) {
    CureMoteSway();
    if ((Frame_Counter & 3) == 0) Dec(Cur()[6]);
    Dec(Cur()[5]);
    if (Cur()[5] != 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::CureMote_Free);
}

namespace {
// The mote's screen point into words 0x903850 / 0x903852, a draw-mode packet
// linked at it, and words 0x903858 / 5A / 5C its +5 x 6.
void CureMoteBegin() {
    SetSW(0, Word(Cur() + 0x20));
    SetSW(2, Word(Cur() + 0x22));
    DrawMode(0x35);
    LinkCure(2, 0xC);
}
void CureMoteShade() {
    const unsigned char* const r = Cur();
    SetSW(8, r[5] * 6u);
    SetSW(0xA, r[5] * 6u);
    SetSW(0xC, r[5] * 6u);
}
}  // namespace

// original 0x4F0B70 (cdecl: a, radius, each read as a u16): four
// semi-transparent gouraud lines from the mote's screen point outwards at the
// angles (a + 8 j & 0x1F) << 7, the length radius; the inner end shaded +5 x
// 6, the outer end (1, 1, 1).
S35_EXPORT void __cdecl CureMote_DrawRays(unsigned a, unsigned radius) {
    CureMoteBegin();
    const unsigned first = a & 0xFFFF;
    CureMoteShade();
    const unsigned end = first + 0x20;
    const int length = static_cast<int>(radius & 0xFFFF);
    for (unsigned j = first; j < end; j += 8) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(6, (j & 0x1F) << 7);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        {
            const int cs = MH_CALL(Math_Cos)(SS(6));
            PutFloat(p + 0x18, Mul12(cs, length) + SS(0));
        }
        {
            const int sn = MH_CALL(Math_Sin)(SS(6));
            PutFloat(p + 0x1C, Mul12(sn, length) + SS(2));
        }
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        LinkCure(2, 0x34);
    }
}

// original 0x4F0D00 (cdecl: a, radius, each read as a u16): four
// semi-transparent three-point gouraud lines from the mote's screen point
// outwards at the angles (a + 8 j & 0x1F) << 7, through radius / 2 to radius;
// the middle point shaded +5 x 6, the ends (1, 1, 1).
S35_EXPORT void __cdecl CureMote_DrawArcs(unsigned a, unsigned radius) {
    CureMoteBegin();
    const unsigned first = a & 0xFFFF;
    CureMoteShade();
    const unsigned end = first + 0x20;
    const int outer = static_cast<int>(radius & 0xFFFF);
    const int middle = outer >> 1;
    for (unsigned j = first; j < end; j += 8) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetSW(6, (j & 0x1F) << 7);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        {
            const int cs = MH_CALL(Math_Cos)(SS(6));
            PutFloat(p + 0x18, Mul12(cs, middle) + SS(0));
        }
        {
            const int sn = MH_CALL(Math_Sin)(SS(6));
            PutFloat(p + 0x1C, Mul12(sn, middle) + SS(2));
        }
        {
            const int cs = MH_CALL(Math_Cos)(SS(6));
            PutFloat(p + 0x28, Mul12(cs, outer) + SS(0));
        }
        const int sn = MH_CALL(Math_Sin)(SS(6));
        const int y2 = Mul12(sn, outer) + SS(2);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        PutFloat(p + 0x2C, y2);
        p[0x14] = SB(8);
        p[0x15] = SB(0xA);
        p[0x16] = SB(0xC);
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        LinkCure(2, 0x34);
    }
}

// original 0x4F0EF0: an eight-point star of semi-transparent gouraud triangles
// round the mote's screen point, the radius +6 + Rand & 3 (word 0x903854):
// the centre CureMote_Colours[kind x 4 + variant] x +5, the rim +5 in all
// three channels; under a draw-mode packet.
S35_EXPORT void __cdecl CureMote_DrawStar(void) {
    DrawMode(0x35);
    LinkCure(2, 0xC);
    {
        const std::uint32_t rnd = RandCall();
        const unsigned char* const r = Cur();
        SetSW(4, (rnd & 3) + r[6]);
        SetSW(0, Word(r + 0x20));
        SetSW(2, Word(r + 0x22));
        const std::uint32_t colour = kCureColours + (r[3] + r[4] * 4u) * 3u;
        SetSW(8, Mem(colour)[0] * static_cast<unsigned>(r[5]));
        SetSW(0xA, Mem(colour + 1)[0] * static_cast<unsigned>(r[5]));
        SetSW(0xC, Mem(colour + 2)[0] * static_cast<unsigned>(r[5]));
        SetSW(0xE, r[5]);
    }
    for (int angle = 0; angle < 0x1000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SS(0));
        PutFloat(p + 0xC, SS(2));
        {
            const int sn = MH_CALL(Math_Sin)(angle);
            PutFloat(p + 0x18, Mul12(sn, SS(4)) + SS(0));
        }
        {
            const int cs = MH_CALL(Math_Cos)(angle);
            PutFloat(p + 0x1C, Mul12(cs, SS(4)) + SS(2));
        }
        angle += 0x200;
        {
            const int sn = MH_CALL(Math_Sin)(angle);
            PutFloat(p + 0x28, Mul12(sn, SS(4)) + SS(0));
        }
        {
            const int cs = MH_CALL(Math_Cos)(angle);
            PutFloat(p + 0x2C, Mul12(cs, SS(4)) + SS(2));
        }
        p[4] = SB(8);
        p[5] = SB(0xA);
        p[6] = SB(0xC);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = SB(0xE);
        LinkCure(2, 0x34);
    }
}

// original 0x4F1120: the first of the 128 motes without bit 0, which it sets;
// its number in al, or 0xFF when none is free.
S35_EXPORT unsigned __cdecl CureMote_Alloc(void) {
    for (unsigned n = 0; n < kCureCount; ++n) {
        unsigned char* const r = CureRecord(n);
        if ((r[0] & 1) == 0) {
            r[0] |= 1;
            return n;
        }
    }
    return 0xFF;
}

// original 0x4F1170: the current mote's +0 .. +4 cleared.
S35_EXPORT void __cdecl CureMote_Free(void) {
    Cur()[0] = 0;
    Cur()[1] = 0;
    Cur()[2] = 0;
    Cur()[3] = 0;
    Cur()[4] = 0;
}

// ===========================================================================
// MAGIC169 (row 97, Benediction read one id down)

// original 0x4F11A0: the kind-2 task. A four-entry stack table by +1:
// Benediction_Start, _Spawn, _Wait, MagicFx_DoneAndFree. Then while +0 and +1
// are set: BattleActor_UpdateScreenXY and the halo. Then every mote of the
// second pool with bit 0 is run through BenedictionMote_Task as
// Sprite_Current, its +0x80 as the owner cell; both put back after each (the
// owner as read before the loop, Sprite_Current as read after the halo).
S35_EXPORT void __cdecl Benediction_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Benediction_Start, bof3::addr::Benediction_Spawn,
                                                 bof3::addr::Benediction_Wait, bof3::addr::MagicFx_DoneAndFree};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Benediction_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* task = Sc();
    if (task[0] != 0 && task[1] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        Call0(bof3::addr::Benediction_DrawHalo);
        task = Sc();
    }
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned n = 0; n < kBlessCount; ++n) {
        unsigned char* const r = BlessRecord(n);
        if ((r[0] & 1) == 0) continue;
        const std::int32_t mote_owner = Long(r + 0x80);
        Sprite_Current = r;
        SetLong(Mem(at::kOwner), mote_owner);
        Call0(bof3::addr::BenedictionMote_Task);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = task;
    }
}

// original 0x4F1250: every mote's +0 / +1 / +2 cleared; MagicFx_CenterOnSide;
// +0x3C up by 0xC00000; Gfx_ClutStrip row 26 from its source with bit 15 set,
// then its first word again without it, Gfx_ClutStripDirty 1; sound 0x100; +9
// and +0xB 0, +1 on.
S35_EXPORT void __cdecl Benediction_Start(void) {
    for (unsigned n = 0; n < kBlessCount; ++n) {
        unsigned char* const r = BlessRecord(n);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    SetLong(Sc() + 0x3C, Add32(Long(Sc() + 0x3C), 0xC00000));
    for (unsigned k = 0x1A00; k < 0x1B00; ++k)
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 0;
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
}

// original 0x4F12E0: +9 up; at 0x10: when the target byte has 0x80 (the party
// side), one child (kind 1, 8) per party member (0x904AB0, read again each
// time): +0x80 the task, +1 0, +4 the member, +9 1 + 0x28 n, each counted in
// +0xB; then +1 on.
S35_EXPORT void __cdecl Benediction_Spawn(void) {
    Inc(Sc()[9]);
    if (Sc()[9] != 0x10) return;
    if ((TargetByte() & 0x80) != 0 && Mem(kPartyCount)[0] != 0) {
        unsigned char delay = 1;
        unsigned char n = 0;
        do {
            unsigned char* const c = TaskSlot(NewTask(8));
            unsigned char* const s = Sc();
            SetLong(c + 0x80, static_cast<std::int32_t>(Key(s)));
            c[1] = 0;
            c[4] = n;
            c[9] = delay;
            Inc(s[0xB]);
            AddB(delay, 0x28);
            ++n;
        } while (n < Mem(kPartyCount)[0]);
    }
    Inc(Sc()[1]);
}

// original 0x4F1390: once +0xB is 0 (every child ended): +9 down, at 0 +1 on.
S35_EXPORT void __cdecl Benediction_Wait(void) {
    if (Sc()[0xB] != 0) return;
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[1]);
}

// original 0x4F13C0: one semi-transparent flat-textured quad (tpage
// Gpu_GetTPage(1, 1, 0x340, 0x100), CLUT Gpu_GetClut(0, 0x1FA), texels (8, 8)
// .. (0x78, 0x68)), 0x70 wide and 0x60 high above the task's screen point, its
// shade +9 << 3 (word 0x903856); under a draw-mode packet (tpage 0xB5), both
// linked at the task's +0x34 / +0x38 layer 1.
S35_EXPORT void __cdecl Benediction_DrawHalo(void) {
    DrawMode(0xB5);
    LinkAt(Sc(), 1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    int x, y;
    {
        const unsigned char* const s = Sc();
        SetSW(6, static_cast<unsigned>(s[9]) << 3);
        x = S16(s + 0x2E);
        y = S16(s + 0x30);
    }
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, x - 0x38);
    PutFloat(p + 0xC, y - 0x60);
    PutFloat(p + 0x18, x + 0x38);
    PutFloat(p + 0x1C, y - 0x60);
    PutFloat(p + 0x38, x + 0x38);
    PutFloat(p + 0x28, x - 0x38);
    PutFloat(p + 0x2C, y);
    PutFloat(p + 0x3C, y);
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    p[0x14] = 8;
    p[0x15] = 8;
    p[0x25] = 8;
    p[0x34] = 8;
    p[0x24] = 0x78;
    p[0x35] = 0x68;
    p[0x44] = 0x78;
    p[0x45] = 0x68;
    p[4] = SB(6);
    p[5] = SB(6);
    p[6] = SB(6);
    LinkAt(Sc(), 1, 0x48);
}

// original 0x4F1500: the child's kind-1 task (parameter 8), a jmp through
// BenedictionChild_TaskTable (one entry) by +1, unchecked.
S35_EXPORT void __cdecl BenedictionChild_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("BenedictionChild_Task", phase, 1);
    magic_harness::Phase(bof3::addr::BenedictionChild_Run)();
}

// original 0x4F1520: a six-entry stack table by +2: BenedictionChild_Start,
// _Tint, _Brighten, MAGIC073's ActorFx_WaitStep4, _Fade, _End.
S35_EXPORT void __cdecl BenedictionChild_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::BenedictionChild_Start,    bof3::addr::BenedictionChild_Tint,
                                                bof3::addr::BenedictionChild_Brighten, bof3::addr::ActorFx_WaitStep4,
                                                bof3::addr::BenedictionChild_Fade,     bof3::addr::BenedictionChild_End};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("BenedictionChild_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4F1570: +9 down; at 0: the party member +4's (0x802D40 + 0x14C x
// +4, unchecked) +8, +0x34, +0x38, +0x3C to the task; +0xB 0, +9 8, +0xA 0, +2
// on; then 24 motes of the second pool (BenedictionMote_Alloc; none free:
// skipped): +0x80 the task, +1 0, +0xB the number n, +9 (n >> 2) + 0x10, each
// counted in +0xB.
S35_EXPORT void __cdecl BenedictionChild_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    {
        const unsigned char* const m = PartyRecord(Sc()[4]);
        Sc()[8] = m[8];
        SetLong(Sc() + 0x34, Long(m + 0x34));
        SetLong(Sc() + 0x38, Long(m + 0x38));
        SetLong(Sc() + 0x3C, Long(m + 0x3C));
    }
    Sc()[0xB] = 0;
    Sc()[9] = 8;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
    for (unsigned n = 0; n < 0x18; ++n) {
        const unsigned slot = Alloc(bof3::addr::BenedictionMote_Alloc);
        if (slot == 0xFF) continue;
        unsigned char* const r = BlessRecord(slot);
        unsigned char* const s = Sc();
        SetLong(r + 0x80, static_cast<std::int32_t>(Key(s)));
        r[1] = 0;
        r[0xB] = static_cast<unsigned char>(n);
        r[9] = static_cast<unsigned char>((n >> 2) + 0x10);
        Inc(s[0xB]);
    }
}

// original 0x4F1660: +9 down; at 0 the member's sprite record (taken before
// the count) released and tinted black (Sprite_SetTint(r, 0, 0, 0, 1)), the
// tint slot to +0xA, +9 8, +2 on.
S35_EXPORT void __cdecl BenedictionChild_Tint(void) {
    unsigned char* const m = PartyRecord(Sc()[4]);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(m);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(m, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4F16D0: the tint record +0xA (MoveScript_TintRecords + 12 n)
// one step brighter in +2..+4; +9 down; at 0 sound 0x101 (odd member) or
// 0x102, +2 on.
S35_EXPORT void __cdecl BenedictionChild_Brighten(void) {
    unsigned char* const s = Sc();
    for (unsigned c = 2; c < 5; ++c) Inc(MoveScript_TintRecords[s[0xA] * 12u + c]);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)((Sc()[4] & 1) != 0 ? 0x101 : 0x102);
    Inc(Sc()[2]);
}

// original 0x4F1760: the tint record one step darker; +9 down; at 0 the
// member's sprite record (taken at entry) released, the member flashed, +2 on.
S35_EXPORT void __cdecl BenedictionChild_Fade(void) {
    unsigned char* const s = Sc();
    unsigned char* const m = PartyRecord(s[4]);
    for (unsigned c = 2; c < 5; ++c) Dec(MoveScript_TintRecords[s[0xA] * 12u + c]);
    Dec(s[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(m);
    MH_CALL(BattleActor_Flash)(Sc()[4]);
    Inc(Sc()[2]);
}

// original 0x4F1800: once +0xB is 0 (every mote ended): the owner's +0xB
// down; when Battle_ActorIsOut(+4) answers non-zero, Battle_SetTargetFlag40(+4);
// a tail jmp to BattleTask_FreeCurrent.
S35_EXPORT void __cdecl BenedictionChild_End(void) {
    if (Sc()[0xB] != 0) return;
    Dec(Owner()[0xB]);
    if (MH_CALL(Battle_ActorIsOut)(Sc()[4]) != 0) MH_CALL(Battle_SetTargetFlag40)(Sc()[4]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F1850: a mote's task, a jmp through BenedictionMote_TaskTable
// (one entry) by +1, unchecked.
S35_EXPORT void __cdecl BenedictionMote_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("BenedictionMote_Task", phase, 1);
    magic_harness::Phase(bof3::addr::BenedictionMote_Run)();
}

// original 0x4F1870: a three-entry stack table by +2 (BenedictionMote_Launch,
// _Spiral, _Fade); a draw-mode packet (tpage 0x35) linked at +0x34 / +0x38;
// while +0 and +2 are set: BattleActor_UpdateScreenXY, MAGIC077's
// ReviveMote_Draw and the glow; a closing draw-mode packet (tpage 0x15).
S35_EXPORT void __cdecl BenedictionMote_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::BenedictionMote_Launch, bof3::addr::BenedictionMote_Spiral,
                                                bof3::addr::BenedictionMote_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("BenedictionMote_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    DrawMode(0x35);
    LinkAt(Sc(), 2, 0xC);
    {
        const unsigned char* const s = Sc();
        if (s[0] != 0 && s[2] != 0) {
            MH_CALL(BattleActor_UpdateScreenXY)();
            MH_CALL(ReviveMote_Draw)();
            Call0(bof3::addr::BenedictionMote_DrawGlow);
        }
    }
    DrawMode(0x15);
    LinkAt(Sc(), 2, 0xC);
}

namespace {
// The orbit of BenedictionMote_Launch / _Spiral / _Fade (ReviveMote_Launch's,
// group S17): word 0x903854 (+0xB & 0xF) << 8; +0x34 / +0x38 the owner's plus
// Math_Sin / Math_Cos of it x dword +0xC.
void BlessMoteOrbit() {
    const unsigned angle = (Sc()[0xB] & 0xFu) << 8;
    SetSW(4, angle);
    {
        const int sn = MH_CALL(Math_Sin)(static_cast<short>(angle));
        unsigned char* const s = Sc();
        SetLong(s + 0x34, Add32(Mul32(sn, Long(s + 0xC)), Long(Owner() + 0x34)));
    }
    {
        const int cs = MH_CALL(Math_Cos)(SS(4));
        unsigned char* const s = Sc();
        SetLong(s + 0x38, Add32(Mul32(cs, Long(s + 0xC)), Long(Owner() + 0x38)));
    }
}
// The rise: while dword +0x20 is below +0x14 (signed), +0x14 -= +0x20 and
// +0x3C += +0x14.
void BlessMoteRise() {
    unsigned char* const s = Sc();
    const std::int32_t d = Long(s + 0x20);
    const std::int32_t h = Long(s + 0x14);
    if (d >= h) return;
    SetLong(s + 0x14, Add32(h, -d));
    SetLong(Sc() + 0x3C, Add32(Long(Sc() + 0x3C), Long(Sc() + 0x14)));
}
}  // namespace

// original 0x4F1920: +9 down; at 0: dword +0xC 8, the orbit, +0x3C the owner's
// + 0x800000, +0x14 ((Rand & 3) + 2) << 20, +0x20 +0x14 / 16, +0x5D / 5E / 5F
// (Rand & 7) + 6 each, +9 0, +0xA 0x10, +2 on.
S35_EXPORT void __cdecl BenedictionMote_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetLong(Sc() + 0xC, 8);
    BlessMoteOrbit();
    SetLong(Sc() + 0x3C, Add32(Long(Owner() + 0x3C), 0x800000));
    {
        const std::uint32_t rnd = RandCall();
        SetLong(Sc() + 0x14, static_cast<std::int32_t>(((rnd & 3) + 2) << 20));
    }
    SetLong(Sc() + 0x20, DivPow2(Long(Sc() + 0x14), 4));
    for (unsigned k = 0x5D; k < 0x60; ++k) {
        const std::uint32_t rnd = RandCall();
        Sc()[k] = static_cast<unsigned char>((rnd & 7) + 6);
    }
    Sc()[9] = 0;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4F1A40: dword +0xC up by 2, the orbit, the rise; +9 up by 2, at
// 0x10 +2 on. Also MAGIC077's (ReviveMote_Task's entry 1).
S35_EXPORT void __cdecl BenedictionMote_Spiral(void) {
    SetLong(Sc() + 0xC, Add32(Long(Sc() + 0xC), 2));
    BlessMoteOrbit();
    BlessMoteRise();
    AddB(Sc()[9], 2);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4F1B00: dword +0xC up, the orbit, the rise; on odd frames +9
// down, at 0 the owner's +0xB down and a tail jmp to MAGIC219's 0x4F6290 (the
// pool slot's free).
S35_EXPORT void __cdecl BenedictionMote_Fade(void) {
    SetLong(Sc() + 0xC, Add32(Long(Sc() + 0xC), 1));
    BlessMoteOrbit();
    BlessMoteRise();
    if ((Frame_Counter & 1) == 0) return;
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    Call0(kFreePoolSlot);
}

// original 0x4F1BD0: a ring of eight semi-transparent gouraud quads round the
// sprite's screen point (+0x2E / +0x30, read once) between radius 0x14 (word
// 0x903852) and 8 (word 0x903850): the outer edge (1, 1, 1), the inner edge
// the signed bytes +0x5D / 5E / 5F x +9 (words 0x903858 / 5A / 5C); each
// linked at +0x34 / +0x38 layer 2. Also MAGIC077's (ReviveMote_Draw's
// neighbour in 0x4BD5E0's run).
S35_EXPORT void __cdecl BenedictionMote_DrawGlow(void) {
    int x, y;
    {
        const unsigned char* const s = Sc();
        x = S16(s + 0x2E);
        y = S16(s + 0x30);
        SetSW(0, 8);
        SetSW(2, 0x14);
        SetSW(8, static_cast<unsigned>(static_cast<signed char>(s[0x5D]) * static_cast<int>(s[9])));
        SetSW(0xA, static_cast<unsigned>(static_cast<signed char>(s[0x5E]) * static_cast<int>(s[9])));
        SetSW(0xC, static_cast<unsigned>(static_cast<signed char>(s[0x5F]) * static_cast<int>(s[9])));
    }
    int angle = 0;
    for (unsigned count = 8; count != 0; --count) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SinSS(angle, 2) + x);
        PutFloat(p + 0xC, CosSS(angle, 2) + y);
        PutFloat(p + 0x28, SinSS(angle, 0) + x);
        PutFloat(p + 0x2C, CosSS(angle, 0) + y);
        angle += 0x200;
        PutFloat(p + 0x18, SinSS(angle, 2) + x);
        PutFloat(p + 0x1C, CosSS(angle, 2) + y);
        PutFloat(p + 0x38, SinSS(angle, 0) + x);
        const int y3 = CosSS(angle, 0) + y;
        p[4] = 1;
        p[5] = 1;
        PutFloat(p + 0x3C, y3);
        p[6] = 1;
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        p[0x24] = SB(8);
        p[0x25] = SB(0xA);
        p[0x26] = SB(0xC);
        p[0x34] = SB(8);
        p[0x35] = SB(0xA);
        p[0x36] = SB(0xC);
        LinkAt(Sc(), 2, 0x44);
    }
}

// original 0x4F1DE0: the first of the 80 motes of the second pool without bit
// 0, which it sets; its number in al, or 0xFF when none is free.
S35_EXPORT unsigned __cdecl BenedictionMote_Alloc(void) {
    for (unsigned n = 0; n < kBlessCount; ++n) {
        unsigned char* const r = BlessRecord(n);
        if ((r[0] & 1) == 0) {
            r[0] = static_cast<unsigned char>(r[0] | 1);
            return n;
        }
    }
    return 0xFF;
}

void MagicS35_Inject() {
    if (bof3::WantsShadow("magic_s35")) magic_s35::SelfTest();
    BOF3_INJECT(LastResort_Task);
    BOF3_INJECT(LastResort_Start);
    BOF3_INJECT(LastResort_WaitChildren);
    BOF3_INJECT(LastResortChild_Task);
    BOF3_INJECT(LastResortRing_Run);
    BOF3_INJECT(MagicFx_WaitOwnerChildren);
    BOF3_INJECT(LastResortRing_DrawDisc);
    BOF3_INJECT(LastResortRing_DrawBand);
    BOF3_INJECT(LastResortBeam_Run);
    BOF3_INJECT(LastResortBeam_Wait);
    BOF3_INJECT(LastResortBeam_Rise);
    BOF3_INJECT(LastResortBeam_Shrink);
    BOF3_INJECT(LastResortBeam_End);
    BOF3_INJECT(LastResortBeam_DrawColumn);
    BOF3_INJECT(LastResortBeam_DrawSparks);
    BOF3_INJECT(Cure_Task);
    BOF3_INJECT(Cure_Start);
    BOF3_INJECT(CureMote_Task);
    BOF3_INJECT(CureMote_Run);
    BOF3_INJECT(CureMote_Wait);
    BOF3_INJECT(CureMote_Rise);
    BOF3_INJECT(CureMote_Fade);
    BOF3_INJECT(CureMote_DrawRays);
    BOF3_INJECT(CureMote_DrawArcs);
    BOF3_INJECT(CureMote_DrawStar);
    BOF3_INJECT(CureMote_Alloc);
    BOF3_INJECT(CureMote_Free);
    BOF3_INJECT(Benediction_Task);
    BOF3_INJECT(Benediction_Start);
    BOF3_INJECT(Benediction_Spawn);
    BOF3_INJECT(Benediction_Wait);
    BOF3_INJECT(Benediction_DrawHalo);
    BOF3_INJECT(BenedictionChild_Task);
    BOF3_INJECT(BenedictionChild_Run);
    BOF3_INJECT(BenedictionChild_Start);
    BOF3_INJECT(BenedictionChild_Tint);
    BOF3_INJECT(BenedictionChild_Brighten);
    BOF3_INJECT(BenedictionChild_Fade);
    BOF3_INJECT(BenedictionChild_End);
    BOF3_INJECT(BenedictionMote_Task);
    BOF3_INJECT(BenedictionMote_Run);
    BOF3_INJECT(BenedictionMote_Launch);
    BOF3_INJECT(BenedictionMote_Spiral);
    BOF3_INJECT(BenedictionMote_Fade);
    BOF3_INJECT(BenedictionMote_DrawGlow);
    BOF3_INJECT(BenedictionMote_Alloc);
}
