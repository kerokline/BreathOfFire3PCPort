// Three spell overlays compiled into the exe, round nine group S28
// (docs/magic_s28.md): the PSX's MAGIC122..MAGIC124.EMI, Magic_Rows rows 74,
// 76 and 124. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Firebreath, Icebreath and ThundrBreath; the names below use those
// labels as hypotheses, and say what the code does.
//
//   - MAGIC122 0x4DD8B0..0x4DE8BD: a kind-2 task that makes one beam (kind-1
//     task 0x38) and makes CLUT row 26 half transparent. The beam's body
//     (BreathBeam_*) is reached from MAGIC114, 115, 118, 120 and 121 too:
//     it aims from the caster at the targets' centre, widens, hits, holds and
//     fades, drawing a textured band and a glow band of quads each frame;
//   - MAGIC123 0x4DE8C0..0x4DF4A6: a kind-2 task that runs a pool of 90 motes
//     (0x69ACC8) itself; each mote (BreathMote_*, reached from seven files)
//     leaves the caster at a random angle, flies, and bursts;
//   - MAGIC124 0x4DF4B0..0x4E0906: a kind-2 task that runs a pool of 64
//     (0x69DB30) - 32 bolts at the targets' centre and the sparks they drop -
//     and draws an orb round itself.
//
// Port_DroppedCall 0x4DF820 lies inside MAGIC124 but is no spell's: it is the
// program's empty function (a bare ret), which the linker folded every empty
// function into and happened to keep here (docs/magic_s28.md section 1).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), and a phase a
// .data table holds is called through the cell, read in place, so the start-up
// fuzz can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement, except that a phase past a
// table aborts where the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent), and the beam's widths
// abort where the original would divide by a zero +0xB.
#include "game/magic_s28.h"

#include <cstdint>
#include <cstring>

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

// DamageScratch's sixteen bytes (0x903850..0x90385F), words the overlays keep
// their working values in, and Prim_VertexScratch's first two words (MAGIC124's
// bolt keeps its moving point there). All are read again after every call, as
// the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
void AddSW(unsigned k, unsigned v) { SetSW(k, SW(k) + v); }
std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
unsigned char U8(int v) { return static_cast<unsigned char>(v); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// imul r32 (the low 32 bits), shl, sar: 32-bit arithmetic as the originals do it.
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
std::int32_t Shl(std::int32_t a, unsigned n) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << n); }
std::int32_t Sar(std::int32_t a, unsigned n) { return a >> n; }
// `fild dword` then `fstp dword`: an integer as a float, into a primitive.
void PutF(unsigned char* prim, unsigned off, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(prim + off, &f, sizeof f);
}

int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }
int Ratan2(int y, int x) { return MH_CALL(Math_Ratan2)(static_cast<float>(y), static_cast<float>(x)); }
void Commit(unsigned size) { MH_CALL(Gfx_CommitPrim)(3, size); }
// The draw-mode primitive every draw of these overlays opens and closes with:
// Gpu_SetDrawMode(Gfx_PacketNext, 0, 1, tpage, 0), committed at depth 3.
void DrawMode(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    Commit(0xC);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }

// The two pools the tasks of MAGIC123 and MAGIC124 run themselves: records of
// the task slots' 0x84 bytes (+0 bit 0 in use, +1 the kind, +0x80 the owner).
constexpr std::uint32_t kMotePool = 0x69ACC8;    // BreathMote_Pool: 90 records
constexpr unsigned kMotes = 90;
constexpr std::uint32_t kBoltPool = 0x69DB30;    // ThunderPool: 64 records
constexpr unsigned kBolts = 64;
unsigned char* Record(std::uint32_t pool, unsigned k) { return Mem(pool + k * at::kTaskStride); }

// The .data tables the dispatchers read in place, index unchecked by the
// originals (ours aborts past each).
constexpr std::uint32_t kBeamPhases = 0x65BB48;    // BreathBeam_Task, by +1: one entry
constexpr std::uint32_t kBeamSteps = 0x65BB4C;     // BreathBeam_Run, by +2: five
constexpr std::uint32_t kMoteKinds = 0x65BB60;     // IcePool_Dispatch, by +1: two
constexpr std::uint32_t kMoteSteps = 0x65BB68;     // BreathMote_Task, by +2: four
constexpr std::uint32_t kMoteAngles = 0x65BB78;    // seven angle steps (u8) the mote draws walk
constexpr std::uint32_t kBoltKinds = 0x65BB80;     // ThunderPool_Dispatch, by +1: two
constexpr std::uint32_t kBoltSteps = 0x65BB88;     // ThunderBolt_Task, by +2: four
constexpr std::uint32_t kSparkSteps = 0x65BB98;    // ThunderSpark_Task, by +2: three

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
void CallCell(const char* who, std::uint32_t table, unsigned index, unsigned entries) {
    if (index >= entries) PastTable(who, index, entries);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))))();
}

// Unnamed callees of other units, called by their addresses, never bound:
//   0x4F6290  a pool record's free (bytes +0..+4 of Sprite_Current cleared) -
//             MAGIC219's (group S37), shared by 35 overlays;
//   0x4F9F70  a stack-table phase: +9 down, at 0 the target flags 0x10 and +1
//             on - MAGIC226/227's (group S38).
constexpr std::uint32_t kRecordFree = 0x4F6290;
constexpr std::uint32_t kCountThenHit = 0x4F9F70;
using VoidFn = void (__cdecl*)();

// A pool task's frame (Icebreath_Task, Thunderbreath_Task): every record with
// bit 0 run through `dispatch` as Sprite_Current, its +0x80 the owner, both put
// back after each. The task and the owner are read once, before the loop.
void RunPool(std::uint32_t pool, unsigned n, void (__cdecl* dispatch)()) {
    unsigned char* const task = Sc();
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const rec = Record(pool, i);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t its = Long(rec + 0x80);
        Sprite_Current = rec;
        SetLong(Mem(at::kOwner), its);
        dispatch();
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = task;
    }
}

// Clears bytes +0..+2 of every record of a pool (the starts').
void ClearPool(std::uint32_t pool, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const rec = Record(pool, i);
        rec[0] = 0;
        rec[1] = 0;
        rec[2] = 0;
    }
}

// The first record of a pool without bit 0, marked, its index answered; 0xFF
// when every one is in use.
unsigned char Alloc(std::uint32_t pool, unsigned n) {
    unsigned char i = 0;
    do {
        unsigned char* const rec = Record(pool, i);
        if ((rec[0] & 1) == 0) {
            rec[0] = static_cast<unsigned char>(rec[0] | 1);
            return i;
        }
        ++i;
    } while (i < n);
    return 0xFF;
}

// The offset from the targets' centre toward the caster's side, as MAGIC122
// and MAGIC123 place their effect: the party's own formation offset for a
// party caster, else a fixed dx / dy (+0xC / +0x10) by the direction byte +8.
void OffsetByDirection() {
    if (Mem(at::kActor)[0] < 3) {
        MH_CALL(MagicFx_FormationOffset)();
        return;
    }
    unsigned char* const sc = Sc();
    switch (sc[8]) {
    case 0: SetLong(sc + 0xC, 0x29); SetLong(sc + 0x10, -13); break;
    case 1: SetLong(sc + 0xC, -41); SetLong(sc + 0x10, -13); break;
    case 2: SetLong(sc + 0xC, -38); SetLong(sc + 0x10, -38); break;
    default: SetLong(sc + 0xC, 0x26); SetLong(sc + 0x10, -38); break;
    }
}

// The caster's position (+0x34, +0x38, +0x3C from the owner), each dword with
// the task and the owner read again.
void CopyOwnerPosition() {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
}

// The beam's half-width at a step: v (a word) divided by the task's +0xB, plus
// one. The original's idiv faults on a zero +0xB; ours aborts.
unsigned BeamWidth(int v) {
    SetSW(0, static_cast<unsigned>(v));
    const unsigned d = Sc()[0xB];
    if (d == 0) bof3::Fatal("BreathBeam: +0xB is 0 (the original divides by zero)");
    const int q = static_cast<short>(v) / static_cast<int>(d) + 1;
    SetSW(0, static_cast<unsigned>(q));
    return static_cast<unsigned>(q);
}

}  // namespace

#define S28_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC122 (row 74, Firebreath read one id down)

// original 0x4DD8B0: the kind-2 task. A two-entry stack table by +1:
// Firebreath_Start, then S17's Leech_WaitOrbs (the wait for the children).
S28_EXPORT void __cdecl Firebreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Firebreath_Start, bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Firebreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4DD8E0: the task takes the owner's direction and position; one
// beam (kind 1, 0x38: +0x80 this task, +1 0, +9 4); CLUT row 26 (0x812980)
// from its buffer 0x4000 below, words 1..15 and 0x21..0x2F with bit 15 (half
// transparent), 0x11..0x1F as they are, and words 0, 0x10, 0x20 zero;
// Gfx_ClutStripDirty; +0xB and +9 0, +1 on. The task's answer is used
// unchecked.
S28_EXPORT void __cdecl Firebreath_Start(void) {
    Sc()[8] = Owner()[8];
    CopyOwnerPosition();
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x38) & 0xFFu;
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Addr(self)));
    child[1] = 0;
    child[9] = 4;
    constexpr unsigned kRow = 0x1A00;
    for (unsigned k = 1; k < 0x10; ++k) Gfx_ClutStrip[kRow + k] = static_cast<unsigned short>(Gfx_ClutStripSource[kRow + k] | 0x8000);
    for (unsigned k = 0x11; k < 0x20; ++k) Gfx_ClutStrip[kRow + k] = Gfx_ClutStripSource[kRow + k];
    for (unsigned k = 0x21; k < 0x30; ++k) Gfx_ClutStrip[kRow + k] = static_cast<unsigned short>(Gfx_ClutStripSource[kRow + k] | 0x8000);
    Gfx_ClutStrip[kRow] = 0;
    Gfx_ClutStrip[kRow + 0x10] = 0;
    Gfx_ClutStrip[kRow + 0x20] = 0;
    Gfx_ClutStripDirty = 1;
    self[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4DD9F0: the beam's kind-1 task (0x38), a jmp through
// BreathBeam_Phases (one entry, BreathBeam_Run) by +1.
S28_EXPORT void __cdecl BreathBeam_Task(void) { CallCell("BreathBeam_Task", kBeamPhases, Sc()[1], 1); }

// original 0x4DDA10: a call through BreathBeam_Steps (five entries: Aim,
// Widen, Hit, Hold, Fade) by +2; then, unless the owner's +0xB is 0xFF or +2
// is 0, the glow band and the textured band.
S28_EXPORT void __cdecl BreathBeam_Run(void) {
    CallCell("BreathBeam_Run", kBeamSteps, Sc()[2], 5);
    if (Owner()[0xB] == 0xFF) return;
    if (Sc()[2] == 0) return;
    MH_CALL(BreathBeam_DrawGlow)();
    MH_CALL(BreathBeam_DrawTextured)();
}

// original 0x4DDA50: +9 down; at 0 the beam is aimed. The direction from the
// owner; the targets' centre (MagicFx_CenterOnSide), its screen point kept in
// the scratch words +0xC / +0xE; the caster's position from the owner, its
// screen point moved by the formation or direction offset (+0xC / +0x10) into
// +0x2E / +0x30 and the scratch +8 / +0xA; +0x14 the angle Math_Ratan2 gives
// from there to the centre; +4 0x10, +0xB 0x11, +9 0, +0xA 1, +2 on; sound
// 0x100.
S28_EXPORT void __cdecl BreathBeam_Aim(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetSW(0xC, Word(Sc() + 0x2E));
    SetSW(0xE, Word(Sc() + 0x30));
    CopyOwnerPosition();
    MH_CALL(BattleActor_UpdateScreenXY)();
    OffsetByDirection();
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) + Word(Sc() + 0xC));
    SetWord(Sc() + 0x30, Word(Sc() + 0x30) + Word(Sc() + 0x10));
    const short x = S16(Sc() + 0x2E), y = S16(Sc() + 0x30);
    SetSW(8, static_cast<unsigned short>(x));
    SetSW(0xA, static_cast<unsigned short>(y));
    const int angle = Ratan2(SS(0xC) - x, SS(0xE) - y);
    SetLong(Sc() + 0x14, angle);
    Sc()[4] = 0x10;
    Sc()[0xB] = 0x11;
    Sc()[9] = 0;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4DDC00: +0xA up by 3 and +9 up; at 8, +2 on.
S28_EXPORT void __cdecl BreathBeam_Widen(void) {
    Sc()[0xA] = U8(Sc()[0xA] + 3);
    Inc(Sc()[9]);
    if (Sc()[9] == 8) Inc(Sc()[2]);
}

// original 0x4DDC30: +0xB down, +9 up; at 0x18 the target flags 0x10 and +2 on.
S28_EXPORT void __cdecl BreathBeam_Hit(void) {
    Dec(Sc()[0xB]);
    Inc(Sc()[9]);
    if (Sc()[9] != 0x18) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[2]);
}

// original 0x4DDC70: +9 up; at 0x78, +2 on.
S28_EXPORT void __cdecl BreathBeam_Hold(void) {
    Inc(Sc()[9]);
    if (Sc()[9] == 0x78) Inc(Sc()[2]);
}

// original 0x4DDC90: +0xB up, +4 (the shade) down, +9 up; at 0x88 the owner's
// +0xB 0xFF and the task freed.
S28_EXPORT void __cdecl BreathBeam_Fade(void) {
    Inc(Sc()[0xB]);
    Dec(Sc()[4]);
    Inc(Sc()[9]);
    if (Sc()[9] != 0x88) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// The two band draws share their walk: from the caster's screen point
// (+0x2E / +0x30) toward +0x14, step i = 1..+0xA moves the point (scratch +8 /
// +0xA, the previous one kept in +0xC / +0xE) 16 units along an angle that
// swings by the sine of (+9 + i); each step draws two quads, one either side
// of the path, whose outer edge is a half-width (BeamWidth) from it.
struct BeamStep {
    int i;
    int ang_a, ang_b;    // the two widths' phases, from i - +9
};
BeamStep BeamWalk(int i, unsigned mask, unsigned shift) {
    SetSW(0xC, SW(8));
    SetSW(0xE, SW(0xA));
    int s = Sin(((Sc()[9] + i) & 0x3F) << 6);
    const unsigned angle = static_cast<unsigned>(Sar(Shl(s, 8), 12) + SW(2)) & 0xFFF;
    SetSW(4, angle);
    s = Sin(static_cast<short>(angle));
    AddSW(8, static_cast<unsigned>(Sar(Shl(s, 4), 12)));
    s = Cos(SS(4));
    AddSW(0xA, static_cast<unsigned>(Sar(Shl(s, 4), 12)));
    const int d = i - static_cast<int>(Sc()[9]);
    return {i, static_cast<int>((static_cast<unsigned>(d) & mask) << shift),
            static_cast<int>((static_cast<unsigned>(d - 1) & mask) << shift)};
}
void BeamStart() {
    DrawMode(0x35);
    const unsigned char* const sc = Sc();
    SetSW(8, Word(sc + 0x2E));
    SetSW(0xA, Word(sc + 0x30));
    SetSW(2, Word(sc + 0x14));
}
// A vertex at the half-width S50 from the point (base x, base y) along S54.
void BeamVertex(unsigned char* p, unsigned off, unsigned bx, unsigned by) {
    int s = Sin(SS(4));
    PutF(p, off, Sar(Mul(s, SS(0)), 12) + SS(bx));
    s = Cos(SS(4));
    PutF(p, off + 4, Sar(Mul(s, SS(0)), 12) + SS(by));
}

// original 0x4DDCE0: the beam's textured band. Draw mode 0x35; for each step
// two GT4 quads (0x54 bytes), u 0x40..0x80 then 0..0x40, v ((+9 x 30 + i) x 8),
// the CLUT (0, 0x1FA) and page (0, 1, 0x380, 0x100), shade +4 x 8 on the
// outer edge and +4 on the inner; half-widths ((sin << 1) >> 12) + 9i - 5 and
// + 9i + 4 over +0xB; then draw mode 0x15.
S28_EXPORT void __cdecl BreathBeam_DrawTextured(void) {
    BeamStart();
    for (int i = 1; i < Sc()[0xA] + 1; ++i) {
        const BeamStep st = BeamWalk(i, 0x3F, 6);
        for (int side = 0; side < 2; ++side) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyGT4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetSW(4, (SW(2) + (side == 0 ? 0x400u : 0xFC00u)) & 0xFFF);
            const unsigned in0 = side == 0 ? 0x1C : 8, out0 = side == 0 ? 0x44 : 0x30;
            const unsigned in1 = side == 0 ? 8 : 0x1C, out1 = side == 0 ? 0x30 : 0x44;
            int s = Sin(st.ang_b);
            BeamWidth(Sar(Shl(s, 1), 12) + 9 * i - 5);
            BeamVertex(p, in0, 0xC, 0xE);
            s = Sin(st.ang_a);
            BeamWidth(Sar(Shl(s, 1), 12) + 9 * i + 4);
            BeamVertex(p, out0, 8, 0xA);
            PutF(p, in1, SS(0xC));
            PutF(p, in1 + 4, SS(0xE));
            PutF(p, out1, SS(8));
            PutF(p, out1 + 4, SS(0xA));
            SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
            SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100));
            p[0x14] = side == 0 ? 0x40 : 0;
            p[0x28] = side == 0 ? 0x80 : 0x40;
            p[0x15] = U8((Sc()[9] * 0x1E + i) << 3);
            p[0x29] = U8((Sc()[9] * 0x1E + i) << 3);
            p[0x3C] = side == 0 ? 0x40 : 0;
            p[0x50] = side == 0 ? 0x80 : 0x40;
            p[0x3D] = U8((Sc()[9] * 0x1E + i + 1) << 3);
            p[0x51] = U8((Sc()[9] * 0x1E + i + 1) << 3);
            // the outer pair of vertices takes +4 x 8, the inner +4 (the word
            // 0x903856 holds each while its bytes are copied)
            SetSW(6, static_cast<unsigned>(Sc()[4]) << 3);
            const unsigned char outer = U8(SW(6));
            SetSW(6, Sc()[4]);
            const unsigned char inner = U8(SW(6));
            const unsigned char first = side == 0 ? outer : inner, second = side == 0 ? inner : outer;
            // side 0: +4 / +0x2C outer, +0x18 / +0x40 inner; side 1 the other way
            p[4] = p[5] = p[6] = first;
            p[0x2C] = p[0x2D] = p[0x2E] = first;
            p[0x18] = p[0x19] = p[0x1A] = second;
            p[0x40] = p[0x41] = p[0x42] = second;
            Commit(0x54);
        }
    }
    DrawMode(0x15);
}

// original 0x4DE370: the beam's glow band. As the textured band, but G4 quads
// (0x44 bytes), phases ((i - +9) & 0xF) << 8, half-widths
// ((sin x (i - 1) x 6) >> 12) + 18i - 14 and ((sin x i x 6) >> 12) + 18i + 4
// over +0xB, shade (+4 x 8, +4 x 4, +4 x 4) on the outer edge and 1 on the
// inner.
S28_EXPORT void __cdecl BreathBeam_DrawGlow(void) {
    BeamStart();
    for (int i = 1; i < Sc()[0xA] + 1; ++i) {
        const BeamStep st = BeamWalk(i, 0xF, 8);
        for (int side = 0; side < 2; ++side) {
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetSW(4, (SW(2) + (side == 0 ? 0x400u : 0xFC00u)) & 0xFFF);
            const unsigned in0 = side == 0 ? 0x18 : 8, out0 = side == 0 ? 0x38 : 0x28;
            const unsigned in1 = side == 0 ? 8 : 0x18, out1 = side == 0 ? 0x28 : 0x38;
            int s = Sin(st.ang_b);
            BeamWidth(Sar(Mul(Mul(s, i - 1), 6), 12) + 18 * i - 14);
            BeamVertex(p, in0, 0xC, 0xE);
            s = Sin(st.ang_a);
            BeamWidth(Sar(Mul(Mul(s, i), 6), 12) + 18 * i + 4);
            BeamVertex(p, out0, 8, 0xA);
            PutF(p, in1, SS(0xC));
            PutF(p, in1 + 4, SS(0xE));
            PutF(p, out1, SS(8));
            PutF(p, out1 + 4, SS(0xA));
            // the shaded vertices: +4 x 8 red, +4 x 4 green and blue (the word
            // 0x903856); side 0 shades +4 and +0x24, side 1 +0x14 and +0x34
            SetSW(6, static_cast<unsigned>(Sc()[4]) << 2);
            const unsigned lit = side == 0 ? 4 : 0x14, lit2 = side == 0 ? 0x24 : 0x34;
            const unsigned dark = side == 0 ? 0x14 : 4, dark2 = side == 0 ? 0x34 : 0x24;
            p[lit] = U8(Sc()[4] << 3);
            p[lit + 1] = U8(SW(6));
            p[lit + 2] = U8(SW(6));
            p[dark] = p[dark + 1] = p[dark + 2] = 1;
            p[lit2] = U8(Sc()[4] << 3);
            p[lit2 + 1] = U8(SW(6));
            p[lit2 + 2] = U8(SW(6));
            p[dark2] = p[dark2 + 1] = p[dark2 + 2] = 1;
            Commit(0x44);
        }
    }
    DrawMode(0x15);
}

// ===========================================================================
// MAGIC123 (row 76, Icebreath read one id down)

// original 0x4DE8C0: the kind-2 task. A two-entry stack table by +1 -
// Icebreath_Start, BattleFx_Finish - then draw mode 0x35, the mote pool run
// through IcePool_Dispatch, draw mode 0x15.
S28_EXPORT void __cdecl Icebreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Icebreath_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Icebreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
    DrawMode(0x35);
    RunPool(kMotePool, kMotes, MH_CALL(IcePool_Dispatch));
    DrawMode(0x15);
}

// original 0x4DE980: bytes +0..+2 of every mote cleared; the task placed as
// the beam's aim places it (the centre's screen point in the scratch +0xA /
// +0xC, the caster's moved by the offset into +0x2E / +0x30 and +6 / +8, +0x14
// the angle from there to the centre); +0xB 0, then 90 motes (+0x80 the task,
// +1 i & 1, +9 (i / 4) x 3 + 1) counted in +0xB; sound 0x102; the target
// flags 0x10; +9 0, +1 on. The pool's answer is used unchecked.
S28_EXPORT void __cdecl Icebreath_Start(void) {
    ClearPool(kMotePool, kMotes);
    Sc()[8] = Owner()[8];
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetSW(0xA, Word(Sc() + 0x2E));
    SetSW(0xC, Word(Sc() + 0x30));
    CopyOwnerPosition();
    MH_CALL(BattleActor_UpdateScreenXY)();
    OffsetByDirection();
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) + Word(Sc() + 0xC));
    SetWord(Sc() + 0x30, Word(Sc() + 0x30) + Word(Sc() + 0x10));
    const short x = S16(Sc() + 0x2E), y = S16(Sc() + 0x30);
    SetSW(6, static_cast<unsigned short>(x));
    SetSW(8, static_cast<unsigned short>(y));
    const int angle = Ratan2(SS(0xA) - x, SS(0xC) - y);
    SetLong(Sc() + 0x14, angle);
    Sc()[0xB] = 0;
    for (unsigned i = 0; i < kMotes; ++i) {
        const unsigned k = MH_CALL(IcePool_Alloc)() & 0xFFu;
        unsigned char* const self = Sc();
        unsigned char* const rec = Record(kMotePool, k);
        SetLong(rec + 0x80, static_cast<std::int32_t>(Addr(self)));
        rec[1] = U8(i & 1);
        rec[9] = U8((i >> 2) * 3 + 1);
        Inc(self[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x102);
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4DEB80: a jmp through IcePool_Kinds (two entries, both
// BreathMote_Task) by +1.
S28_EXPORT void __cdecl IcePool_Dispatch(void) { CallCell("IcePool_Dispatch", kMoteKinds, Sc()[1], 2); }

// original 0x4DEBA0: a jmp through BreathMote_Steps (four entries: Launch,
// Fly, Burst, Free) by +2.
S28_EXPORT void __cdecl BreathMote_Task(void) { CallCell("BreathMote_Task", kMoteSteps, Sc()[2], 4); }

// original 0x4DEBC0: +9 down; at 0 the mote starts at the owner's screen point
// (+0x2E / +0x30), its heading the owner's +0x14 plus or minus Rand & 0xFF
// (Rand's bit 0 says which); +3 Rand & 7, +0xC Rand & 7 + 8 (the speed), +0xB
// Rand (the phase), +9 Rand & 0xF + 8 (the flight), +0xA 2 (the size), +2 on.
S28_EXPORT void __cdecl BreathMote_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    SetWord(Sc() + 0x2E, Word(Owner() + 0x2E));
    SetWord(Sc() + 0x30, Word(Owner() + 0x30));
    if (MH_CALL(Rand)() & 1) {
        const int r = MH_CALL(Rand)();
        SetLong(Sc() + 0x14, (r & 0xFF) + Long(Owner() + 0x14));
    } else {
        const int r = MH_CALL(Rand)();
        SetLong(Sc() + 0x14, Long(Owner() + 0x14) - (r & 0xFF));
    }
    Sc()[3] = U8(MH_CALL(Rand)() & 7);
    const int speed = MH_CALL(Rand)();
    SetLong(Sc() + 0xC, (speed & 7) + 8);
    Sc()[0xB] = U8(MH_CALL(Rand)());
    Sc()[9] = U8((MH_CALL(Rand)() & 0xF) + 8);
    Sc()[0xA] = 2;
    Inc(Sc()[2]);
}

// original 0x4DECA0: +0xB up; the heading +0x14 swung by (sin(+0xB) << 7) >>
// 12 (into the scratch +4), the point +0x2E / +0x30 moved +0xC along it (the
// scratch +0, the points +6 / +8); +0xA grows by 2 while under +3 + 6; +9
// down, and at 0 sound 0x100 (+1 odd) or 0x101 on odd frames, +0xB 0, +2 on.
// Then, while the point is on the screen (0 < x < 0x140, 0 < y < 0xF0), its
// hexagon and ring.
S28_EXPORT void __cdecl BreathMote_Fly(void) {
    Inc(Sc()[0xB]);
    int s = Sin((Sc()[0xB] & 0x3F) << 6);
    unsigned char* sc = Sc();
    const unsigned angle = static_cast<unsigned>(Sar(Shl(s, 7), 12) + Word(sc + 0x14)) & 0xFFFF;
    unsigned char* const px = sc + 0x2E;
    SetSW(4, angle);
    SetSW(0, Word(sc + 0xC));
    s = Sin(static_cast<short>(angle));
    SetWord(px, Word(px) + static_cast<unsigned>(Sar(Mul(s, SS(0)), 12)));
    unsigned char* const py = Sc() + 0x30;
    s = Cos(SS(4));
    SetWord(py, Word(py) + static_cast<unsigned>(Sar(Mul(s, SS(0)), 12)));
    sc = Sc();
    SetSW(6, Word(sc + 0x2E));
    SetSW(8, Word(sc + 0x30));
    if (static_cast<int>(sc[0xA]) < static_cast<int>(sc[3]) + 6) {
        sc[0xA] = U8(sc[0xA] + 2);
        sc = Sc();
    }
    Dec(sc[9]);
    sc = Sc();
    if (sc[9] == 0) {
        if (Frame_Counter & 1) {
            MH_CALL(Sound_PlayById)((sc[1] & 1) ? 0x100 : 0x101);
            sc = Sc();
        }
        sc[0xB] = 0;
        Inc(Sc()[2]);
        sc = Sc();
    }
    const short x = S16(sc + 0x2E);
    if (x >= 0x140) return;
    const short y = S16(sc + 0x30);
    if (y >= 0xF0 || x <= 0 || y <= 0) return;
    MH_CALL(BreathMote_DrawHex)();
    MH_CALL(BreathMote_DrawRing)();
}

// original 0x4DEDF0: +0xB and +9 up, +0xA down; at 0 the owner's +0xB down and
// the record freed (MAGIC219's 0x4F6290), else the burst drawn.
S28_EXPORT void __cdecl BreathMote_Burst(void) {
    Inc(Sc()[0xB]);
    Inc(Sc()[9]);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] == 0) {
        Dec(Owner()[0xB]);
        MH_AT(VoidFn, kRecordFree)();
        return;
    }
    MH_CALL(BreathMote_DrawBurst)();
}

// original 0x4DEE40: the owner's +0xB down and the record freed.
S28_EXPORT void __cdecl BreathMote_Free(void) {
    Dec(Owner()[0xB]);
    MH_AT(VoidFn, kRecordFree)();
}

// The angle of a mote's corner j: (BreathMote_Angles[j] + +9) & 0x3F, << 6.
unsigned MoteAngle(unsigned j) { return ((Mem(kMoteAngles + j)[0] + Sc()[9]) & 0x3Fu) << 6; }
// A corner at radius r (a scratch word) from the mote's point (+6 / +8) along
// the angle in the scratch +4.
void MoteCorner(unsigned char* p, unsigned off, unsigned r, bool angle_fresh, unsigned angle) {
    int s = Sin(angle_fresh ? static_cast<int>(static_cast<short>(angle)) : SS(4));
    PutF(p, off, Sar(Mul(s, SS(r)), 12) + SS(6));
    s = Cos(SS(4));
    PutF(p, off + 4, Sar(Mul(s, SS(r)), 12) + SS(8));
}

// original 0x4DEE50: the mote's hexagon: six G3 triangles (0x34 bytes) from
// the point (+6 / +8) to two corners at radius +0xA, the centre (1, 1, 0xC0),
// the corners 0xE0 grey.
S28_EXPORT void __cdecl BreathMote_DrawHex(void) {
    SetSW(0, Sc()[0xA]);
    for (unsigned j = 0; j < 6; ++j) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutF(p, 8, SS(6));
        PutF(p, 0xC, SS(8));
        unsigned a = MoteAngle(j);
        SetSW(4, a);
        MoteCorner(p, 0x18, 0, true, a);
        a = MoteAngle(j + 1);
        SetSW(4, a);
        MoteCorner(p, 0x28, 0, true, a);
        p[4] = 1;
        p[5] = 1;
        p[6] = 0xC0;
        p[0x14] = p[0x15] = p[0x16] = 0xE0;
        p[0x24] = p[0x25] = p[0x26] = 0xE0;
        Commit(0x34);
    }
}

// original 0x4DEFD0: the mote's ring: six G4 quads (0x44 bytes) between
// radius +0xA (the scratch +0) and +0xA x 2 + Rand & 7 (the scratch +0xE),
// the outer edge (0x20, 0x20, 0xC0), the inner 1.
S28_EXPORT void __cdecl BreathMote_DrawRing(void) {
    SetSW(0, Sc()[0xA]);
    const int r = MH_CALL(Rand)();
    SetSW(0xE, static_cast<unsigned>((r & 7) + Sc()[0xA] * 2));
    for (unsigned j = 0; j < 6; ++j) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        unsigned a = MoteAngle(j);
        SetSW(4, a);
        MoteCorner(p, 8, 0xE, true, a);
        MoteCorner(p, 0x28, 0, false, 0);
        a = MoteAngle(j + 1);
        SetSW(4, a);
        MoteCorner(p, 0x18, 0xE, true, a);
        MoteCorner(p, 0x38, 0, false, 0);
        p[4] = p[5] = p[6] = 1;
        p[0x14] = p[0x15] = p[0x16] = 1;
        p[0x24] = 0x20;
        p[0x25] = 0x20;
        p[0x26] = 0xC0;
        p[0x34] = 0x20;
        p[0x35] = 0x20;
        p[0x36] = 0xC0;
        Commit(0x44);
    }
}

// original 0x4DF210: the burst: eight G3 triangles (0x34 bytes), each at a
// point (the scratch +6 / +8) (+0xB x 2 + 4 + Rand & 3) from the mote along
// angle 0x200 k, with corners at radius +0xA along ((+9 + 2k) & 0xF) << 8,
// + 5 and - 6; the first corner (0x20, 0x20, 0xC0), the second 0xC0 grey, the
// third (0x20, 0x20, 0xC0).
S28_EXPORT void __cdecl BreathMote_DrawBurst(void) {
    SetSW(0, static_cast<unsigned>(Sc()[0xB] * 2 + 4));
    SetSW(0xE, Sc()[0xA]);
    int k = 0;
    for (int a = 0; a < 0x1000; a += 0x200, ++k) {
        const int r = MH_CALL(Rand)();
        SetSW(0, static_cast<unsigned>((r & 3) + Sc()[0xB] * 2 + 4));
        int s = Sin(a);
        SetSW(6, static_cast<unsigned>(Sar(Mul(s, SS(0)), 12)) + Word(Sc() + 0x2E));
        s = Cos(a);
        SetSW(8, static_cast<unsigned>(Sar(Mul(s, SS(0)), 12)) + Word(Sc() + 0x30));
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const int steps[3] = {2 * k, 2 * k + 5, 2 * k - 6};
        const unsigned offs[3] = {8, 0x18, 0x28};
        for (int c = 0; c < 3; ++c) {
            const unsigned angle = (static_cast<unsigned>(Sc()[9] + steps[c]) & 0xFu) << 8;
            SetSW(4, angle);
            MoteCorner(p, offs[c], 0xE, true, angle);
        }
        p[4] = 0x20;
        p[5] = 0x20;
        p[6] = 0xC0;
        p[0x14] = p[0x15] = p[0x16] = 0xC0;
        p[0x24] = 0x20;
        p[0x25] = 0x20;
        p[0x26] = 0xC0;
        Commit(0x34);
    }
}

// original 0x4DF450: the first mote record without bit 0, marked, its index
// answered; 0xFF when all 90 are in use. Only al is the original's answer.
S28_EXPORT unsigned char __cdecl IcePool_Alloc(void) { return Alloc(kMotePool, kMotes); }

// ===========================================================================
// MAGIC124 (row 124, ThundrBreath read one id down)

// original 0x4DF4B0: the kind-2 task. A four-entry stack table by +1 -
// Thunderbreath_Start, MAGIC226's count-down to the target flags 0x10,
// Thunderbreath_Wait, BattleFx_Finish - then draw mode 0x35, the bolt pool
// run through ThunderPool_Dispatch, the orb, draw mode 0x15.
S28_EXPORT void __cdecl Thunderbreath_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Thunderbreath_Start, kCountThenHit,
                                                 bof3::addr::Thunderbreath_Wait, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Thunderbreath_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
    DrawMode(0x35);
    RunPool(kBoltPool, kBolts, MH_CALL(ThunderPool_Dispatch));
    MH_CALL(Thunderbreath_DrawOrb)();
    DrawMode(0x15);
}

// original 0x4DF580: bytes +0..+2 of every bolt record cleared; the task at
// the owner's direction and position, its screen point moved by the formation
// offset (+0xC / +0x10); +0xB 0, +9 8, +0xA 0x10, +1 on; 32 bolts (+0x80 the
// task, +1 0, +4 i, +0xB 0, +9 i x 4 + 1) counted in +0xB; sound 0x100. The
// pool's answer is used unchecked.
S28_EXPORT void __cdecl Thunderbreath_Start(void) {
    ClearPool(kBoltPool, kBolts);
    Sc()[8] = Owner()[8];
    CopyOwnerPosition();
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(MagicFx_FormationOffset)();
    SetWord(Sc() + 0x2E, Word(Sc() + 0x2E) + Word(Sc() + 0xC));
    SetWord(Sc() + 0x30, Word(Sc() + 0x30) + Word(Sc() + 0x10));
    Sc()[0xB] = 0;
    Sc()[9] = 8;
    Sc()[0xA] = 0x10;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 0x20; ++i) {
        const unsigned k = MH_CALL(ThunderPool_Alloc)() & 0xFFu;
        unsigned char* const self = Sc();
        unsigned char* const rec = Record(kBoltPool, k);
        SetLong(rec + 0x80, static_cast<std::int32_t>(Addr(self)));
        rec[1] = 0;
        rec[4] = U8(i);
        rec[0xB] = 0;
        rec[9] = U8((i << 2) + 1);
        Inc(self[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4DF6A0: once +0xB (the bolts alive) is 2 or fewer, +0xA down; at
// 0, +1 on.
S28_EXPORT void __cdecl Thunderbreath_Wait(void) {
    if (Sc()[0xB] > 2) return;
    Dec(Sc()[0xA]);
    if (Sc()[0xA] == 0) Inc(Sc()[1]);
}

// original 0x4DF6D0: the orb round the task: eight G3 triangles (0x34 bytes)
// from +0x2E / +0x30 to radius 0x20 + Rand & 3, 0x200 apart; the centre +0xA
// x 15 grey, the rim 1.
S28_EXPORT void __cdecl Thunderbreath_DrawOrb(void) {
    const unsigned char shade = U8(Sc()[0xA] * 0xF);
    const int radius = (MH_CALL(Rand)() & 3) + 0x20;
    int a = 0;
    for (int n = 0; n < 8; ++n) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutF(p, 8, S16(Sc() + 0x2E));
        PutF(p, 0xC, S16(Sc() + 0x30));
        int s = Sin(a);
        PutF(p, 0x18, Sar(Mul(s, radius), 12) + S16(Sc() + 0x2E));
        s = Cos(a);
        PutF(p, 0x1C, Sar(Mul(s, radius), 12) + S16(Sc() + 0x30));
        a += 0x200;
        s = Sin(a);
        PutF(p, 0x28, Sar(Mul(s, radius), 12) + S16(Sc() + 0x2E));
        s = Cos(a);
        PutF(p, 0x2C, Sar(Mul(s, radius), 12) + S16(Sc() + 0x30));
        p[4] = p[5] = p[6] = shade;
        p[0x14] = p[0x15] = p[0x16] = 1;
        p[0x24] = p[0x25] = p[0x26] = 1;
        Commit(0x34);
    }
}

// original 0x4DF820: the program's empty function - a bare ret. Every empty
// function of the port (the PSX calls it dropped: op B8's, the sprite flag
// bit 8's, the menu frames DIV-0011 restores, kind-1 task 0x5B's handler) was
// folded into this one copy, which the linker kept inside MAGIC124. Ours is a
// ret too, padded to the five bytes BOF3X_ORIGINAL's jmp back writes over it.
extern "C" __attribute__((naked)) void __cdecl Port_DroppedCall(unsigned char) {
    asm volatile("ret\n\tint3\n\tint3\n\tint3\n\tint3\n\tint3\n\tint3\n\tint3");
}

// original 0x4DF830: a jmp through ThunderPool_Kinds (two entries:
// ThunderBolt_Task, ThunderSpark_Task) by +1.
S28_EXPORT void __cdecl ThunderPool_Dispatch(void) { CallCell("ThunderPool_Dispatch", kBoltKinds, Sc()[1], 2); }

// original 0x4DF850: a call through ThunderBolt_Steps (four entries: Aim,
// Grow, Flicker, Fade) by +2; then, while +0 and +2 are set, +0xB up and the
// bolt drawn; every fourth frame of the flicker step (+2 1) with +0xA set, a
// spark (a pool record, kind 1, +0x80 the owner, at the bolt's end point, the
// scratch +8 / +0xA) counted in the owner's +0xB, unless the pool is full.
S28_EXPORT void __cdecl ThunderBolt_Task(void) {
    CallCell("ThunderBolt_Task", kBoltSteps, Sc()[2], 4);
    if (Sc()[0] == 0 || Sc()[2] == 0) return;
    Inc(Sc()[0xB]);
    MH_CALL(ThunderBolt_Draw)();
    const unsigned char* const sc = Sc();
    if ((sc[0xB] & 3) != 0 || sc[2] != 1 || sc[0xA] == 0) return;
    const unsigned char k = MH_CALL(ThunderPool_Alloc)();
    if (k == 0xFF) return;
    unsigned char* const rec = Record(kBoltPool, k);
    const std::uint16_t x = SW(8);
    unsigned char* const owner = Owner();
    SetLong(rec + 0x80, static_cast<std::int32_t>(Addr(owner)));
    rec[1] = 1;
    SetWord(rec + 0x2E, x);
    SetWord(rec + 0x30, SW(0xA));
    Inc(owner[0xB]);
}

// original 0x4DF8F0: +9 down; at 0 the bolt is aimed from the owner's screen
// point (into +0x2E / +0x30 and the scratch +8 / +0xA) at the targets' centre
// (the scratch +0xC / +0xE): +0x14 that angle, turned by (Rand & 7) << 6, left
// for an even +4 and right for an odd; +0xC 0, +0xB Rand & 0x3F, +9 0x10, +0xA
// 0, +2 on.
S28_EXPORT void __cdecl ThunderBolt_Aim(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetSW(0xC, Word(Sc() + 0x2E));
    SetSW(0xE, Word(Sc() + 0x30));
    SetWord(Sc() + 0x2E, Word(Owner() + 0x2E));
    SetSW(8, Word(Sc() + 0x2E));
    SetWord(Sc() + 0x30, Word(Owner() + 0x30));
    SetSW(0xA, Word(Sc() + 0x30));
    const int angle = Ratan2(SS(0xC) - SS(8), SS(0xE) - SS(0xA));
    SetLong(Sc() + 0x14, angle);
    unsigned char* const heading = Sc() + 0x14;
    const bool odd = (Sc()[4] & 1) != 0;
    const int r = MH_CALL(Rand)() & 7;
    SetLong(heading, Long(heading) + Shl(odd ? r : -r, 6));
    SetLong(Sc() + 0xC, 0);
    Sc()[0xB] = U8(MH_CALL(Rand)() & 0x3F);
    Sc()[9] = 0x10;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4DFA20: +0xA up by 4; at 0x10, +2 on.
S28_EXPORT void __cdecl ThunderBolt_Grow(void) {
    Sc()[0xA] = U8(Sc()[0xA] + 4);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4DFA40: +0xA 0x10 + (sin(+0xB) << 2) >> 12; the dword +0xC up; at
// 0x10, +2 on.
S28_EXPORT void __cdecl ThunderBolt_Flicker(void) {
    const int s = Sin((Sc()[0xB] & 0x3F) << 6);
    Sc()[0xA] = U8(Sar(Shl(s, 2), 12) + 0x10);
    SetLong(Sc() + 0xC, Long(Sc() + 0xC) + 1);
    if (Long(Sc() + 0xC) == 0x10) Inc(Sc()[2]);
}

// original 0x4DFA90: as the flicker, then +9 down by 2; at 0 the owner's +0xB
// down and the record freed (MAGIC219's 0x4F6290).
S28_EXPORT void __cdecl ThunderBolt_Fade(void) {
    const int s = Sin((Sc()[0xB] & 0x3F) << 6);
    Sc()[0xA] = U8(Sar(Shl(s, 2), 12) + 0x10);
    Sc()[9] = U8(Sc()[9] - 2);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_AT(VoidFn, kRecordFree)();
}

// A quad of the bolt (G4, 0x44 bytes): its first pair of corners at the
// half-widths S50 (the step before) round the previous point +0xC / +0xE along
// S56, its second pair at S52 round the point +8 / +0xA along S54 - `wide`
// adds 8 to both widths for the outer corner of each pair.
void BoltSide(unsigned char* p, unsigned in_prev, unsigned in_next, unsigned out_prev, unsigned out_next, bool wide) {
    // corner (prev, S56, S50) and (next, S54, S52)
    int s = Sin(SS(6));
    PutF(p, out_prev, Sar(Mul(s, SS(0) + (wide ? 8 : 0)), 12) + SS(0xC));
    s = Cos(SS(6));
    PutF(p, out_prev + 4, Sar(Mul(s, SS(0) + (wide ? 8 : 0)), 12) + SS(0xE));
    s = Sin(SS(4));
    PutF(p, out_next, Sar(Mul(s, SS(2) + (wide ? 8 : 0)), 12) + SS(8));
    s = Cos(SS(4));
    PutF(p, out_next + 4, Sar(Mul(s, SS(2) + (wide ? 8 : 0)), 12) + SS(0xA));
    if (!wide) {
        PutF(p, in_prev, SS(0xC));
        PutF(p, in_prev + 4, SS(0xE));
        PutF(p, in_next, SS(8));
        PutF(p, in_next + 4, SS(0xA));
    }
}
void BoltSideWide(unsigned char* p, unsigned mid_prev, unsigned mid_next, unsigned out_prev, unsigned out_next) {
    BoltSide(p, 0, 0, out_prev, out_next, true);
    int s = Sin(SS(6));
    PutF(p, mid_prev, Sar(Mul(s, SS(0)), 12) + SS(0xC));
    s = Cos(SS(6));
    PutF(p, mid_prev + 4, Sar(Mul(s, SS(0)), 12) + SS(0xE));
    s = Sin(SS(4));
    PutF(p, mid_next, Sar(Mul(s, SS(2)), 12) + SS(8));
    s = Cos(SS(4));
    PutF(p, mid_next + 4, Sar(Mul(s, SS(2)), 12) + SS(0xA));
}
void BoltEnd(unsigned char* p, unsigned char i) {
    if (Sc()[0xA] != i) return;
    p[0x24] = p[0x25] = p[0x26] = 1;
    p[0x34] = p[0x35] = p[0x36] = 1;
}

// original 0x4DFAE0: the bolt. Its width S52 3 + Rand & 1 (2 + Rand & 3 every
// eighth step); its point walks from the task's screen point (the vertex
// scratch +0 / +2) along +0x14, 16 units a step, bent off it by the sine of 0x800
// / +0xA x i (a bow) times (Rand & 0xF) + 0x18 times the sine of ((+0xB + i) &
// 0x1F) << 7 (a wobble) across (+0x14 - 0x400); each of the +0xA steps draws
// four G4 quads round the segment from the previous point, two either side,
// the inner ones shaded (+9 x 8, +9 x 4, +9 x 15) at the path and +9 x 15
// grey at the edge, the outer ones fading to 1; the last step's far corners 1.
// The loop counter is a byte: with +0xA 0xFF it never ends, as the original's.
S28_EXPORT void __cdecl ThunderBolt_Draw(void) {
    SetSW(2, static_cast<unsigned>((MH_CALL(Rand)() & 1) + 3));
    unsigned char b8, b4, grey, phase;
    int bend;
    {
        const unsigned char* const sc = Sc();
        const unsigned char c9 = sc[9];
        b8 = U8(c9 << 3);
        b4 = U8(c9 << 2);
        grey = U8(c9 * 0xF);
        SetSW(0xC, Word(sc + 0x2E));
        SetVW(0, Word(sc + 0x2E));
        SetSW(0xE, Word(sc + 0x30));
        SetVW(2, Word(sc + 0x30));
        phase = sc[0xB];
        bend = static_cast<short>(Word(sc + 0x14) - 0x400);
    }
    int s = Sin((phase & 0x1F) << 7);
    int across = static_cast<short>(Sar(Shl(s, 2), 12));
    s = Sin(bend);
    SetSW(8, static_cast<unsigned>(Sar(Mul(s, across), 12) + Long(Mem(kV))));
    s = Cos(bend);
    SetSW(0xA, static_cast<unsigned>(Sar(Mul(s, across), 12)) + VW(2));
    SetSW(4, static_cast<unsigned>(Ratan2(SS(0xC) - SS(8), SS(0xE) - SS(0xA)) - 0x400));
    unsigned char i = 1;
    if (Sc()[0xA] + 1 <= 1) return;
    unsigned step = 1;
    do {
        SetSW(0, SW(2));
        if ((i & 7) == 0) SetSW(2, static_cast<unsigned>((MH_CALL(Rand)() & 3) + 2));
        SetSW(0xC, SW(8));
        SetSW(0xE, SW(0xA));
        s = Sin(Long(Sc() + 0x14));
        SetVW(0, VW(0) + static_cast<unsigned>(Sar(Shl(s, 4), 12)));
        s = Cos(Long(Sc() + 0x14));
        SetVW(2, VW(2) + static_cast<unsigned>(Sar(Shl(s, 4), 12)));
        const unsigned count = Sc()[0xA];
        if (count == 0) bof3::Fatal("ThunderBolt_Draw: +0xA is 0 (the original divides by zero)");
        int bow = Sin(Mul(0x800 / static_cast<int>(count), static_cast<int>(step)));
        const int r = MH_CALL(Rand)();
        bow = Mul(bow, (r & 0xF) + 0x18);
        const unsigned wobble_at = ((Sc()[0xB] + step) & 0x1Fu) << 7;
        bow = Sar(bow, 12);
        s = Sin(static_cast<int>(wobble_at));
        across = static_cast<short>(Sar(Mul(bow, s), 12));
        s = Sin(bend);
        SetSW(8, static_cast<unsigned>(Sar(Mul(s, across), 12) + Long(Mem(kV))));
        s = Cos(bend);
        const unsigned prev_heading = SW(4);
        SetSW(0xA, static_cast<unsigned>(Sar(Mul(s, across), 12)) + VW(2));
        SetSW(6, prev_heading + 0x400);
        const int heading = Ratan2(SS(0xC) - SS(8), SS(0xE) - SS(0xA));
        unsigned char* p = Gfx_PacketNext;
        SetSW(4, static_cast<unsigned>(heading));
        // the two quads on the +0x400 side: inner, then outer
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const unsigned s56 = SW(6) + 0x400;
            AddSW(4, 0x400);
            SetSW(6, s56);
        }
        BoltSide(p, 8, 0x28, 0x18, 0x38, false);
        p[4] = p[5] = p[6] = grey;
        p[0x24] = p[0x25] = p[0x26] = grey;
        p[0x14] = b8;
        p[0x15] = b4;
        p[0x16] = grey;
        p[0x34] = b8;
        p[0x35] = b4;
        p[0x36] = grey;
        BoltEnd(p, i);
        Commit(0x44);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        BoltSideWide(p, 8, 0x28, 0x18, 0x38);
        p[4] = b8;
        p[5] = b4;
        p[6] = grey;
        p[0x24] = b8;
        p[0x25] = b4;
        p[0x26] = grey;
        p[0x14] = p[0x15] = p[0x16] = 1;
        p[0x34] = p[0x35] = p[0x36] = 1;
        BoltEnd(p, i);
        Commit(0x44);
        // the two on the other side (the angles turned back by 0x800)
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const unsigned s56 = SW(6) + 0xF800;
            AddSW(4, 0xF800);
            SetSW(6, s56);
        }
        BoltSide(p, 0x18, 0x38, 8, 0x28, false);
        p[0x14] = p[0x15] = p[0x16] = grey;
        p[0x34] = p[0x35] = p[0x36] = grey;
        p[4] = b8;
        p[5] = b4;
        p[6] = grey;
        p[0x24] = b8;
        p[0x25] = b4;
        p[0x26] = grey;
        BoltEnd(p, i);
        Commit(0x44);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        BoltSideWide(p, 0x18, 0x38, 8, 0x28);
        p[0x14] = b8;
        p[0x15] = b4;
        p[0x16] = grey;
        p[0x34] = b8;
        p[0x35] = b4;
        p[0x36] = grey;
        p[4] = p[5] = p[6] = 1;
        p[0x24] = p[0x25] = p[0x26] = 1;
        BoltEnd(p, i);
        Commit(0x44);
        ++i;
        step = i;
    } while (step < Sc()[0xA] + 1u);
}

// original 0x4E0470: a call through ThunderSpark_Steps (three entries: Start,
// Grow, Fade) by +2; then, while +0 and +2 are set, the fan and the ring.
S28_EXPORT void __cdecl ThunderSpark_Task(void) {
    CallCell("ThunderSpark_Task", kSparkSteps, Sc()[2], 3);
    if (Sc()[0] == 0 || Sc()[2] == 0) return;
    MH_CALL(ThunderSpark_DrawFan)();
    MH_CALL(ThunderSpark_DrawRing)();
}

// original 0x4E04A0: the colour (+0x5D 8, +0x5E 6, +0x5F 0xF), +9 0, +0xA 2,
// +2 on.
S28_EXPORT void __cdecl ThunderSpark_Start(void) {
    Sc()[0x5D] = 8;
    Sc()[0x5E] = 6;
    Sc()[0x5F] = 0xF;
    Sc()[9] = 0;
    Sc()[0xA] = 2;
    Inc(Sc()[2]);
}

// original 0x4E04E0: +0xA up by 2, +9 up by 4; at 0x10, +2 on.
S28_EXPORT void __cdecl ThunderSpark_Grow(void) {
    Sc()[0xA] = U8(Sc()[0xA] + 2);
    Sc()[9] = U8(Sc()[9] + 4);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4E0510: +0xA up, +9 down by 4; at 0 the owner's +0xB down and the
// record freed.
S28_EXPORT void __cdecl ThunderSpark_Fade(void) {
    Inc(Sc()[0xA]);
    Sc()[9] = U8(Sc()[9] - 4);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_AT(VoidFn, kRecordFree)();
}

// original 0x4E0550: the spark's fan: eight G3 triangles (0x34 bytes) from
// +0x2E / +0x30 to radius +0xA x 2, 0x200 apart; the centre +9 x 15 grey, the
// rim the colour (+0x5D, +0x5E, +0x5F) x +9.
S28_EXPORT void __cdecl ThunderSpark_DrawFan(void) {
    const unsigned char* const sc = Sc();
    const unsigned char c9 = sc[9];
    const unsigned char grey = U8(c9 * 0xF);
    const unsigned char r = U8(sc[0x5D] * c9), g = U8(sc[0x5E] * c9), b = U8(sc[0x5F] * c9);
    const int radius = static_cast<int>((static_cast<unsigned>(sc[0xA]) << 1) & 0xFFFF);
    int a = 0;
    for (int n = 0; n < 8; ++n) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutF(p, 8, S16(Sc() + 0x2E));
        PutF(p, 0xC, S16(Sc() + 0x30));
        int s = Sin(a);
        PutF(p, 0x18, Sar(Mul(s, radius), 12) + S16(Sc() + 0x2E));
        s = Cos(a);
        PutF(p, 0x1C, Sar(Mul(s, radius), 12) + S16(Sc() + 0x30));
        a += 0x200;
        s = Sin(a);
        PutF(p, 0x28, Sar(Mul(s, radius), 12) + S16(Sc() + 0x2E));
        s = Cos(a);
        PutF(p, 0x2C, Sar(Mul(s, radius), 12) + S16(Sc() + 0x30));
        p[4] = p[5] = p[6] = grey;
        p[0x14] = r;
        p[0x15] = g;
        p[0x16] = b;
        p[0x24] = r;
        p[0x25] = g;
        p[0x26] = b;
        Commit(0x34);
    }
}

// original 0x4E06C0: the spark's ring: eight G4 quads (0x44 bytes) between
// radius +0xA x 3 (the colour (+0x5D, +0x5E, +0x5F) x +9) and +0xA x 2 (1),
// 0x200 apart.
S28_EXPORT void __cdecl ThunderSpark_DrawRing(void) {
    const unsigned char* const sc = Sc();
    const unsigned char c9 = sc[9];
    const unsigned char r = U8(sc[0x5D] * c9), g = U8(sc[0x5E] * c9), b = U8(sc[0x5F] * c9);
    const int inner = static_cast<int>((static_cast<unsigned>(sc[0xA]) * 2) & 0xFFFF);
    const int outer = static_cast<int>((static_cast<unsigned>(sc[0xA]) * 3) & 0xFFFF);
    int a = 0;
    for (int n = 0; n < 8; ++n) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const int a2 = a + 0x200;
        int s = Sin(a);
        PutF(p, 8, Sar(Mul(s, outer), 12) + S16(Sc() + 0x2E));
        s = Cos(a);
        PutF(p, 0xC, Sar(Mul(s, outer), 12) + S16(Sc() + 0x30));
        s = Sin(a2);
        PutF(p, 0x18, Sar(Mul(s, outer), 12) + S16(Sc() + 0x2E));
        s = Cos(a2);
        PutF(p, 0x1C, Sar(Mul(s, outer), 12) + S16(Sc() + 0x30));
        s = Sin(a);
        PutF(p, 0x28, Sar(Mul(s, inner), 12) + S16(Sc() + 0x2E));
        s = Cos(a);
        PutF(p, 0x2C, Sar(Mul(s, inner), 12) + S16(Sc() + 0x30));
        s = Sin(a2);
        PutF(p, 0x38, Sar(Mul(s, inner), 12) + S16(Sc() + 0x2E));
        s = Cos(a2);
        PutF(p, 0x3C, Sar(Mul(s, inner), 12) + S16(Sc() + 0x30));
        p[4] = p[5] = p[6] = 1;
        p[0x14] = p[0x15] = p[0x16] = 1;
        p[0x24] = r;
        p[0x25] = g;
        p[0x26] = b;
        p[0x34] = r;
        p[0x35] = g;
        p[0x36] = b;
        Commit(0x44);
        a = a2;
    }
}

// original 0x4E08B0: the first bolt record without bit 0, marked, its index
// answered; 0xFF when all 64 are in use. Only al is the original's answer.
S28_EXPORT unsigned char __cdecl ThunderPool_Alloc(void) { return Alloc(kBoltPool, kBolts); }

void MagicS28_Inject() {
    if (bof3::WantsShadow("magic_s28")) magic_s28::SelfTest();
    BOF3_INJECT(Firebreath_Task);
    BOF3_INJECT(Firebreath_Start);
    BOF3_INJECT(BreathBeam_Task);
    BOF3_INJECT(BreathBeam_Run);
    BOF3_INJECT(BreathBeam_Aim);
    BOF3_INJECT(BreathBeam_Widen);
    BOF3_INJECT(BreathBeam_Hit);
    BOF3_INJECT(BreathBeam_Hold);
    BOF3_INJECT(BreathBeam_Fade);
    BOF3_INJECT(BreathBeam_DrawTextured);
    BOF3_INJECT(BreathBeam_DrawGlow);
    BOF3_INJECT(Icebreath_Task);
    BOF3_INJECT(Icebreath_Start);
    BOF3_INJECT(IcePool_Dispatch);
    BOF3_INJECT(BreathMote_Task);
    BOF3_INJECT(BreathMote_Launch);
    BOF3_INJECT(BreathMote_Fly);
    BOF3_INJECT(BreathMote_Burst);
    BOF3_INJECT(BreathMote_Free);
    BOF3_INJECT(BreathMote_DrawHex);
    BOF3_INJECT(BreathMote_DrawRing);
    BOF3_INJECT(BreathMote_DrawBurst);
    BOF3_INJECT(IcePool_Alloc);
    BOF3_INJECT(Thunderbreath_Task);
    BOF3_INJECT(Thunderbreath_Start);
    BOF3_INJECT(Thunderbreath_Wait);
    BOF3_INJECT(Thunderbreath_DrawOrb);
    BOF3_INJECT(Port_DroppedCall);
    BOF3_INJECT(ThunderPool_Dispatch);
    BOF3_INJECT(ThunderBolt_Task);
    BOF3_INJECT(ThunderBolt_Aim);
    BOF3_INJECT(ThunderBolt_Grow);
    BOF3_INJECT(ThunderBolt_Flicker);
    BOF3_INJECT(ThunderBolt_Fade);
    BOF3_INJECT(ThunderBolt_Draw);
    BOF3_INJECT(ThunderSpark_Task);
    BOF3_INJECT(ThunderSpark_Start);
    BOF3_INJECT(ThunderSpark_Grow);
    BOF3_INJECT(ThunderSpark_Fade);
    BOF3_INJECT(ThunderSpark_DrawFan);
    BOF3_INJECT(ThunderSpark_DrawRing);
    BOF3_INJECT(ThunderPool_Alloc);
}
