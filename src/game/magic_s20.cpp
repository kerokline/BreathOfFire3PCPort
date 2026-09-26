// Three BMAGIC overlays compiled into the exe, spell round group S20
// (docs/magic_s20.md). Each is named by its PSX file, not by a spell: the
// ability names are the sibling's read one id down, a hypothesis.
//
//   - MAGIC087 (Magic_Rows row 48, 0x4C3490..0x4C4486): a task that walks a
//     pool of its own (80 slots of 0x84 at 0x6906D8) and a kind-1 child
//     (parameter 0x27) per living actor on the target's side: a column of
//     textured quads round the actor, then motes that circle it and motes
//     that rise, each a gouraud triangle under the actor's matrix.
//   - MAGIC088 (row 29, 0x4C4490..0x4C4FB1): a seven-step task that picks a
//     variant by the ability id, tints the source sprite dark and back,
//     applies a stat change (0x4FB6F0, the effect library's) and runs two
//     kind-1 children (parameter 0x20): a fan under the actor and a ring of
//     flat quads waving in six bands.
//   - MAGIC092 (row 49, 0x4C5680..0x4C62F6): a task that walks a second pool
//     (48 slots at 0x693018) and a kind-1 child (parameter 0x28) per living
//     actor on the target's side: the target tinted, then three pool motes -
//     a flame of four textured quads a column and sparks (0x4E9850, another
//     overlay's draw).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase / a .data
// table read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. No divergence: each is a faithful
// replacement, except that a phase past one of the three stack tables aborts
// where the original would call through its own stack (the precedent,
// docs/magic_fx_reached.md section 3). The .data dispatch tables are read in
// place and unchecked, as the originals read them.
#include "game/magic_s20.h"

#include <cstddef>
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

// A callee that is ours, called with the argument list the original pushes
// rather than the one its prototype declares.
#define MH_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&name)))

// The two pools of 0x84-byte motes: MAGIC087's 80 and MAGIC092's 48, each
// walked by its task with Sprite_Current the mote and 0x93B940 its +0x80.
constexpr std::uint32_t kPoolA = 0x6906D8;
constexpr unsigned kPoolACount = 0x50;
constexpr std::uint32_t kPoolB = 0x693018;
constexpr unsigned kPoolBCount = 0x30;
constexpr std::uint32_t kPoolStride = 0x84;

// The .data dispatch tables, read in place (docs/magic_s20.md section 2).
constexpr std::uint32_t kM087ChildTypes = 0x65B4E8;    // by +1: 0x4C3720
constexpr std::uint32_t kM087ChildPhases = 0x65B4EC;   // by +2: five
constexpr std::uint32_t kM087MoteTypes = 0x65B500;     // by +1: orbit, orbit, rise, rise
constexpr std::uint32_t kM087OrbitPhases = 0x65B510;   // by +2: four
constexpr std::uint32_t kM087RisePhases = 0x65B520;    // by +2: two
constexpr std::uint32_t kM087Shades = 0x65B528;        // u8 by +4
constexpr std::uint32_t kM088Colours = 0x65B52C;       // u8 rgb rows, row = +4 * 6 + band
constexpr std::uint32_t kM088ChildTypes = 0x65B574;    // by +1: fan, wave
constexpr std::uint32_t kM088FanPhases = 0x65B57C;     // by +2: three, other overlays'
constexpr std::uint32_t kM088WaveBands = 0x65B588;     // six dwords, stepped +1 / -1
constexpr std::uint32_t kM088WavePhases = 0x65B5A8;    // by +2: four
constexpr std::uint32_t kM092ChildTypes = 0x65B5C8;    // by +1: 0x4C58C0
constexpr std::uint32_t kM092ChildPhases = 0x65B5CC;   // by +2: four
constexpr std::uint32_t kM092MoteTypes = 0x65B5DC;     // by +1: flame, spark
constexpr std::uint32_t kM092FlamePhases = 0x65B5E4;   // by +2: four
constexpr std::uint32_t kM092SparkPhases = 0x65B5F4;   // by +2: four
// The library's stat index by MAGIC088's variant (+4): read in place.
constexpr std::uint32_t kStatByVariant = 0x65C39C;
// Magic088_Variant's byte table (in .text, after its jump table): the jump
// table's index by the ability id - 0x1E, 0xA3 entries.
constexpr std::uint32_t kVariantIndex = 0x4C488C;

// The effect's scratch (words 0x903850..0x90385E, Scratch_Swap among them)
// and the vertex scratch (Prim_VertexScratch: SVECTORs 8 bytes apart).
constexpr std::uint32_t kScratch = 0x903850;
constexpr std::uint32_t kVertex = 0x9037A0;

// The ability id (u16) and the actor's side byte 0x904B35 (4 for the party's
// commands, by MAGIC088's reading).
constexpr std::uint32_t kAbility = 0x904B80;
constexpr std::uint32_t kSide = 0x904B35;

// The CLUT strip rows the starts write (docs/magic_fx_reached.md: kClutRow).
constexpr std::uint32_t kClutRow = 0x812980;
constexpr std::uint32_t kClutRowNext = 0x8129A0;

// An enemy record's byte +0xF0 (its type) indexes a 0x8C-stride table whose
// byte at 0x8C564F lifts MAGIC087's column (read in place).
constexpr std::uint32_t kEnemyLift = 0x8C564F;
constexpr std::uint32_t kEnemyLiftStride = 0x8C;

constexpr std::uint32_t kLibStatChange = 0x4FB6F0;   // the effect library's (group L)
constexpr std::uint32_t kDrawSparks = 0x4E9420;      // MAGIC144's, reached from nine overlays
constexpr std::uint32_t kDrawSpark = 0x4E9850;       // MAGIC144's, reached from nine overlays
using StatChangeFn = unsigned char (__cdecl*)(unsigned, unsigned);
using VoidFn = void (__cdecl*)();

unsigned char* Scr(unsigned k) { return Mem(kScratch + k); }
unsigned char* Vtx(unsigned k) { return Mem(kVertex + k); }
const short* VtxP(unsigned k) { return reinterpret_cast<const short*>(Vtx(k)); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
unsigned char* Task(unsigned index) { return Mem(at::kTasks + index * at::kTaskStride); }
unsigned char* PoolA(unsigned index) { return Mem(kPoolA + index * kPoolStride); }
unsigned char* PoolB(unsigned index) { return Mem(kPoolB + index * kPoolStride); }
unsigned char* Enemy(unsigned i) { return Mem(at::kEnemies + i * at::kEnemyStride); }
unsigned char* Member(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* Tint(unsigned i) { return Mem(0x7E0700 + i * 12); }   // MoveScript_TintRecords, unchecked
unsigned char* Owner() { return Pointer(at::kOwner); }
std::int32_t PtrValue(const unsigned char* p) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)));
}
void Bump(unsigned char& b, int by) { b = static_cast<unsigned char>(b + by); }

// A .data dispatch table's entry, read as the original reads it.
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// `imul` then `sar`: a 32-bit product that wraps, shifted arithmetically.
int MulSar(int a, int b, int by) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> by;
}
// A 32-bit add that wraps.
int Add32(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }
// `shl eax, 3` then `sar eax, 0xC`.
int Shl3Sar12(int v) { return static_cast<int>(static_cast<std::uint32_t>(v) << 3) >> 12; }

// `fild dword` then `fstp dword`: the integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}
// `fld dword; fadd 8.0 (0x5C41CC); fstp dword`: exact for the integers stored.
void PutFloatPlus8(unsigned char* at, const unsigned char* from) {
    float f;
    std::memcpy(&f, from, sizeof f);
    const float g = static_cast<float>(static_cast<double>(f) + 8.0);
    std::memcpy(at, &g, sizeof g);
}

// Sprite_Current's position from `from` (+0x34 / +0x38 / +0x3C).
void CopyPosition(unsigned char* to, const unsigned char* from) {
    SetLong(to + 0x34, Long(from + 0x34));
    SetLong(to + 0x38, Long(from + 0x38));
    SetLong(to + 0x3C, Long(from + 0x3C));
}

// MapView_LinkPrimAt at Sprite_Current's position (read now).
void LinkAtCurrent(unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(sc + 0x34)), static_cast<unsigned long>(Long(sc + 0x38)), 2, size);
}

// Bytes +0..+4 of Sprite_Current cleared, the pointer read again for each:
// a pool mote freed.
void FreeMote() {
    for (unsigned k = 0; k < 5; ++k) Sprite_Current[k] = 0;
}

}  // namespace

#define S20_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC087 (Magic_Rows row 48)

// original 0x4C3490: the kind-2 task. Its phase +1 through a two-entry table
// on its stack (Magic087_Start, BattleFx_Finish); then every pool-A mote with
// +0 bit 0 is run (Magic087_MoteRun) with Sprite_Current the mote and
// 0x93B940 its +0x80, both put back after (the values read after the phase).
S20_EXPORT void __cdecl Magic087_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic087_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Magic087_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const sc = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kPoolACount; ++i) {
        unsigned char* const m = PoolA(i);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = Long(m + 0x80);
        Sprite_Current = m;
        SetLong(Mem(at::kOwner), its);
        MH_CALL(Magic087_MoteRun)();
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = sc;
    }
}

// original 0x4C3510: +0xB 0; the pool's +0 / +1 / +2 cleared; a kind-1 child
// (parameter 0x27) for every actor on the target's side (0x904B44 bit 0x40:
// the eight enemies, else the three members) that is not out: owned by this
// task, +4 its count so far, +9 = 8 i + 1, at the actor's position 0x80 higher
// (+0x3C + 0x800000) - an enemy's lifted again by its type's byte at
// 0x8C564F; the count +0xB up. Then the CLUT strip row 26: cells 1..15 from
// 0x7400 below with bit 15, cell 0 = Gfx_ClutStripSource's; dirty; on.
// BattleTask_Create's index is not tested (0xFF writes past the slots);
// Sprite_Current is read again after each create.
S20_EXPORT void __cdecl Magic087_Start(void) {
    Sprite_Current[0xB] = 0;
    for (unsigned i = 0; i < kPoolACount; ++i) {
        unsigned char* const m = PoolA(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    if (Mem(at::kTarget)[0] & 0x40) {
        unsigned char nine = 1;
        for (unsigned i = 0; i < 8; ++i, nine = static_cast<unsigned char>(nine + 8)) {
            if (MH_CALL(Battle_ActorIsOut)(i + 3) != 0) continue;
            const unsigned index = MH_CALL(BattleTask_Create)(1, 0x27);
            unsigned char* const sc = Sprite_Current;
            unsigned char* const t = Task(index & 0xFF);
            const unsigned char* const e = Enemy(i);
            SetLong(t + 0x80, PtrValue(sc));
            t[4] = sc[0xB];
            t[9] = nine;
            SetLong(t + 0x34, Long(e + 0x34));
            SetLong(t + 0x38, Long(e + 0x38));
            SetLong(t + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(e + 0x3C)) + 0x800000u));
            SetWord(t + 0x3E, Word(t + 0x3E) + Mem(kEnemyLift + e[0xF0] * kEnemyLiftStride)[0]);
            Bump(sc[0xB], 1);
        }
    } else {
        unsigned char nine = 1;
        for (unsigned i = 0; i < 3; ++i, nine = static_cast<unsigned char>(nine + 8)) {
            if (MH_CALL(Battle_ActorIsOut)(i) != 0) continue;
            const unsigned index = MH_CALL(BattleTask_Create)(1, 0x27);
            unsigned char* const sc = Sprite_Current;
            unsigned char* const t = Task(index & 0xFF);
            const unsigned char* const m = Member(i);
            SetLong(t + 0x80, PtrValue(sc));
            t[4] = sc[0xB];
            t[9] = nine;
            SetLong(t + 0x34, Long(m + 0x34));
            SetLong(t + 0x38, Long(m + 0x38));
            SetLong(t + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(m + 0x3C)) + 0x800000u));
            Bump(sc[0xB], 1);
        }
    }
    for (std::uint32_t a = kClutRow + 2; a < kClutRow + 0x20; a += 2) SetWord(Mem(a), Word(Mem(a - 0x7400)) | 0x8000u);
    SetWord(Mem(kClutRow), Word(Mem(kClutRow - 0x7400)));
    Gfx_ClutStripDirty = 1;
    Bump(Sprite_Current[1], 1);
}

// original 0x4C3700: the child (kind 1, parameter 0x27): a jmp through
// 0x65B4E8 by +1 (one entry, Magic087_ChildPhase; the index unchecked).
S20_EXPORT void __cdecl Magic087_Child(void) { Entry(kM087ChildTypes, Sprite_Current[1])(); }

// original 0x4C3720: a jmp through 0x65B4EC by +2 (five phases, unchecked).
S20_EXPORT void __cdecl Magic087_ChildPhase(void) { Entry(kM087ChildPhases, Sprite_Current[2])(); }

// original 0x4C3740: phase 0. +0xB 0; eight pool-A motes, each owned by the
// child (Sprite_Current read after each alloc) and counted in its +0xB: four
// of type 0 (+4 and +0xB = n, +9 = 2 n + 1) and four of type 1 (+4 = n, +9 =
// 2 n + 1). The alloc's 0xFF (the pool full) is not tested: the writes land
// past the pool. Then +2 on, and sound 0x100 when +4 is 0.
S20_EXPORT void __cdecl Magic087_ChildSpawn(void) {
    Sprite_Current[0xB] = 0;
    for (unsigned n = 0; n < 4; ++n) {
        const unsigned index = MH_CALL(Magic087_PoolAlloc)() & 0xFFu;
        unsigned char* const sc = Sprite_Current;
        unsigned char* const m = PoolA(index);
        SetLong(m + 0x80, PtrValue(sc));
        m[1] = 0;
        m[4] = static_cast<unsigned char>(n);
        m[0xB] = static_cast<unsigned char>(n);
        m[9] = static_cast<unsigned char>(2 * n + 1);
        Bump(sc[0xB], 1);
    }
    for (unsigned n = 0; n < 4; ++n) {
        const unsigned index = MH_CALL(Magic087_PoolAlloc)() & 0xFFu;
        unsigned char* const sc = Sprite_Current;
        unsigned char* const m = PoolA(index);
        SetLong(m + 0x80, PtrValue(sc));
        m[1] = 1;
        m[4] = static_cast<unsigned char>(n);
        m[9] = static_cast<unsigned char>(2 * n + 1);
        Bump(sc[0xB], 1);
    }
    Bump(Sprite_Current[2], 1);
    if (Sprite_Current[4] == 0) MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C3840: phase 1. When +0xB is 2: +4 0, +9 8, +0xA 0x10, on.
S20_EXPORT void __cdecl Magic087_ChildWait(void) {
    if (Sprite_Current[0xB] != 2) return;
    Sprite_Current[4] = 0;
    Sprite_Current[9] = 8;
    Sprite_Current[0xA] = 0x10;
    Bump(Sprite_Current[2], 1);
}

// original 0x4C3870: phase 2. Sound 0x101 when +4 is 0; +9 down, and at 0 two
// pool-A motes of types 2 and 3 (the alloc untested), +9 back to 8 and +0xA
// down - at 0, +0xB = 0x82, +9 0 and on. Then +4 up, the actor's screen point
// and the column drawn twice (offsets 0 and 15).
S20_EXPORT void __cdecl Magic087_ChildRing(void) {
    if (Sprite_Current[4] == 0) MH_CALL(Sound_PlayById)(0x101);
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] == 0) {
        unsigned index = MH_CALL(Magic087_PoolAlloc)() & 0xFFu;
        unsigned char* sc = Sprite_Current;
        unsigned char* m = PoolA(index);
        SetLong(m + 0x80, PtrValue(sc));
        m[1] = 2;
        index = MH_CALL(Magic087_PoolAlloc)() & 0xFFu;
        sc = Sprite_Current;
        m = PoolA(index);
        SetLong(m + 0x80, PtrValue(sc));
        m[1] = 3;
        sc[9] = 8;
        Bump(Sprite_Current[0xA], -1);
        if (Sprite_Current[0xA] == 0) {
            Sprite_Current[0xB] = 0x82;
            Sprite_Current[9] = 0;
            Bump(Sprite_Current[2], 1);
        }
    }
    Bump(Sprite_Current[4], 1);
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(Magic087_DrawColumn)(0);
    MH_CALL(Magic087_DrawColumn)(0xF);
}

// original 0x4C3960: phase 3. +9 up; at 0x10 on (and nothing drawn); else +4
// up, the screen point and the column twice.
S20_EXPORT void __cdecl Magic087_ChildGrow(void) {
    Bump(Sprite_Current[9], 1);
    if (Sprite_Current[9] == 0x10) {
        Bump(Sprite_Current[2], 1);
        return;
    }
    Bump(Sprite_Current[4], 1);
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(Magic087_DrawColumn)(0);
    MH_CALL(Magic087_DrawColumn)(0xF);
}

// original 0x4C39A0: phase 4. When +0xB is 0x80 (every mote gone): the
// owner's count +0xB down and the child freed (a tail jmp).
S20_EXPORT void __cdecl Magic087_ChildEnd(void) {
    if (Sprite_Current[0xB] != 0x80) return;
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C39C0: the column - up to eight textured gouraud quads stacked
// up the actor's screen point (+0x2E / +0x30), 8 wide, each 8 high, swaying
// by sin(((+4 + i + offset + 1) & 31) << 7) * 8 >> 12. Its rows from i = +9 / 2
// in phase 3 (else 0) to n = +4 / 2 + 1 (8 from +4 = 16). The shade 0x903858
// carries from each quad's top to the next's bottom: 1, then (i + 1) * 16, or
// 0x151 - 48 i from the sixth row. Texture page (0x3C0, 0x100), clut (0,
// 0x1FA), rows ((+4 + 4 i) * 2) & 31.
S20_EXPORT void __cdecl Magic087_DrawColumn(int offset) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtCurrent(0xC);
    const unsigned char* sc = Sprite_Current;
    SetLong(Scr(8), 1);
    int n = 8;
    if (sc[4] < 0x10) n = (sc[4] >> 1) + 1;
    int i = 0;
    if (sc[2] == 3) i = sc[9] >> 1;
    if (i >= n) return;
    int up = (i + 1) << 4;
    int down = 0x151 - i * 0x30;
    int row48 = i * 0x30;
    int y = i * 8;
    do {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const int a1 = static_cast<int>(((Sprite_Current[4] + static_cast<unsigned>(i) + static_cast<unsigned>(offset) + 1) & 0x1F) << 7);
        SetLong(Scr(4), a1);
        const int s1 = MH_CALL(Math_Sin)(a1);
        sc = Sprite_Current;
        PutFloat(p + 8, S16(sc + 0x2E) - Shl3Sar12(s1) - 4);
        PutFloat(p + 0xC, S16(sc + 0x30) - (y + 8));
        PutFloatPlus8(p + 0x1C, p + 8);
        PutFloat(p + 0x20, S16(sc + 0x30) - (y + 8));
        const int a2 = static_cast<int>(((sc[4] + static_cast<unsigned>(i) + static_cast<unsigned>(offset)) & 0x1F) << 7);
        SetLong(Scr(4), a2);
        const int s2 = MH_CALL(Math_Sin)(a2);
        sc = Sprite_Current;
        PutFloat(p + 0x30, S16(sc + 0x2E) - Shl3Sar12(s2) - 4);
        PutFloat(p + 0x34, S16(sc + 0x30) - y);
        PutFloatPlus8(p + 0x44, p + 0x30);
        PutFloat(p + 0x48, S16(sc + 0x30) - y);
        for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = Scr(8)[0];
        SetLong(Scr(8), row48 >= 0x120 ? down : up);
        for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = Scr(8)[0];
        SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0x100));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        p[0x14] = 0;
        const auto v = static_cast<unsigned char>(((Sprite_Current[4] + static_cast<unsigned char>(i << 2)) << 1) & 0x1F);
        p[0x28] = 8;
        p[0x15] = v;
        p[0x3C] = 0;
        p[0x29] = v;
        p[0x50] = 8;
        p[0x3D] = static_cast<unsigned char>(v + 8);
        p[0x51] = static_cast<unsigned char>(v + 8);
        LinkAtCurrent(0x54);
        ++i;
        row48 += 0x30;
        up += 0x10;
        y += 8;
        down -= 0x30;
    } while (i < n);
}

// original 0x4C3CC0: a pool-A mote: a jmp through 0x65B500 by +1 (four
// entries - types 0 and 1 Magic087_OrbitRun, 2 and 3 Magic087_RiseRun -
// unchecked).
S20_EXPORT void __cdecl Magic087_MoteRun(void) { Entry(kM087MoteTypes, Sprite_Current[1])(); }

// original 0x4C3CE0: an orbiting mote. Its phase +2 through 0x65B510 (four,
// unchecked); then while it lives, in phases 1 and 2, its triangle under its
// own matrix.
S20_EXPORT void __cdecl Magic087_OrbitRun(void) {
    Entry(kM087OrbitPhases, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0) return;
    const unsigned char phase = sc[2];
    if (phase == 0 || phase == 3) return;
    MH_CALL(Magic087_PushMatrix)();
    MH_CALL(Magic087_DrawTriangle)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C3D20: orbit phase 0. +9 down (the delay); at 0: radius +0xC
// 0x100, its step +0x18 4; type 1 starts at angle 0 (+0xB 0, +9 0), type 0
// at +0xB 8, +9 0x1F; +0xA 0; on.
S20_EXPORT void __cdecl Magic087_OrbitStart(void) {
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    SetLong(Sprite_Current + 0xC, 0x100);
    SetLong(Sprite_Current + 0x18, 4);
    if (Sprite_Current[1] != 0) {
        Sprite_Current[0xB] = 0;
        Sprite_Current[9] = 0;
    } else {
        Sprite_Current[0xB] = 8;
        Sprite_Current[9] = 0x1F;
    }
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[2], 1);
}

// original 0x4C3D90: orbit phase 1. +0xB and +9 up; the angle (+9 & 63) << 6
// to 0x903854, the radius +0xC less +0x18 to 0x903850; x and z the owner's
// plus sin / cos(angle) * radius >> 3 (each scratch read again after its
// call), y the owner's; +0xA up to 0x21. At radius 0: a mote of +4 0 goes on,
// any other is freed (the owner's count down, +0..+4 0).
S20_EXPORT void __cdecl Magic087_OrbitSpin(void) {
    Bump(Sprite_Current[0xB], 1);
    Bump(Sprite_Current[9], 1);
    unsigned char* sc = Sprite_Current;
    SetLong(Scr(4), static_cast<std::int32_t>((sc[9] & 0x3Fu) << 6));
    SetLong(sc + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(sc + 0xC)) - static_cast<std::uint32_t>(Long(sc + 0x18))));
    SetLong(Scr(0), Long(Sprite_Current + 0xC));
    const int s = MH_CALL(Math_Sin)(Long(Scr(4)));
    SetLong(Sprite_Current + 0x34, Add32(MulSar(s, Long(Scr(0)), 3), Long(Owner() + 0x34)));
    const int c = MH_CALL(Math_Cos)(Long(Scr(4)));
    SetLong(Sprite_Current + 0x38, Add32(MulSar(c, Long(Scr(0)), 3), Long(Owner() + 0x38)));
    SetLong(Sprite_Current + 0x3C, Long(Owner() + 0x3C));
    sc = Sprite_Current;
    if (sc[0xA] < 0x21) Bump(sc[0xA], 1);
    sc = Sprite_Current;
    if (Long(sc + 0xC) != 0) return;
    if (sc[4] == 0) {
        Bump(sc[2], 1);
        return;
    }
    Bump(Owner()[0xB], -1);
    FreeMote();
}

// original 0x4C3EA0: orbit phase 2: on once the owner's +0xB is 0x82.
S20_EXPORT void __cdecl Magic087_OrbitWait(void) {
    if (Owner()[0xB] == 0x82) Bump(Sprite_Current[2], 1);
}

// original 0x4C3EC0: orbit phase 3: +0xA down; at 0 the owner's count down
// and the mote freed.
S20_EXPORT void __cdecl Magic087_OrbitFade(void) {
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    Bump(Owner()[0xB], -1);
    FreeMote();
}

// original 0x4C3F20: a rising mote. Its phase +2 through 0x65B520 (two,
// unchecked); then while it lives, past phase 0, its triangle.
S20_EXPORT void __cdecl Magic087_RiseRun(void) {
    Entry(kM087RisePhases, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(Magic087_PushMatrix)();
    MH_CALL(Magic087_DrawTriangle)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C3F60: rise phase 0: the owner's position; type 2 at angle 0
// (+0xB 0), type 3 at +0xB 8; +9 0, +0xA 0x20; on.
S20_EXPORT void __cdecl Magic087_RiseStart(void) {
    CopyPosition(Sprite_Current, Owner());
    if (Sprite_Current[1] == 2) {
        Sprite_Current[0xB] = 0;
        Sprite_Current[9] = 0;
    } else {
        Sprite_Current[0xB] = 8;
        Sprite_Current[9] = 0;
    }
    Sprite_Current[0xA] = 0x20;
    Bump(Sprite_Current[2], 1);
}

// original 0x4C3FE0: rise phase 1: 8 up (+0x3C less 0x80000), +9 up, +0xA
// down; at 0 on and freed (the phase written, then cleared with +0..+4).
S20_EXPORT void __cdecl Magic087_RiseStep(void) {
    SetLong(Sprite_Current + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sprite_Current + 0x3C)) + 0xFFF80000u));
    Bump(Sprite_Current[9], 1);
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    Bump(Sprite_Current[2], 1);
    FreeMote();
}

// original 0x4C4050: pushes the matrix and loads Camera_Matrix x the mote's:
// translation RotTrans of (x >> 9 - 0x4000, z >> 9 - 0x4000, -((height +
// 0x200) / 2)), rotation (0, 0, (+0xB & 63) << 6). As MagicFx_PushActorMatrix
// (0x4B7D40) with the turn and the lift; the MATRIX one block, RotTrans
// writing straight into its translation. The original pushes a flag pointer
// to RotTrans that its prototype does not declare.
S20_EXPORT void __cdecl Magic087_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const sc = Sprite_Current;
    const short rot[4] = {0, 0, static_cast<short>((sc[0xB] & 0x3F) << 6), 0};
    short v[4];
    v[0] = static_cast<short>((Long(sc + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(sc + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-((S16(sc + 0x3E) + 0x200) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(offsetof(Matrix, t) == 0x14 && sizeof(Matrix) == 0x20, "MATRIX layout");
    MH_CALL(Gte_RotTrans)(v, m.t);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

namespace {
// A corner of Magic087_DrawTriangle: (sin, cos)(angle) * radius >> 12, 0.
void Corner(unsigned k, int angle) {
    int v = MH_CALL(Math_Sin)(angle);
    SetWord(Vtx(k), static_cast<unsigned>(MulSar(v, Long(Scr(0)), 12)));
    v = MH_CALL(Math_Cos)(angle);
    SetWord(Vtx(k + 2), static_cast<unsigned>(MulSar(v, Long(Scr(0)), 12)));
    SetWord(Vtx(k + 4), 0);
}
// The line's shades from 0x903858: the far ends its byte, the green of the
// first and third points and the blue of the second 1.
void TriangleShade(unsigned char* p) {
    p[5] = 1;
    p[4] = Scr(8)[0];
    p[6] = Scr(8)[0];
    p[0x14] = Scr(8)[0];
    p[0x15] = Scr(8)[0];
    p[0x16] = 1;
    p[0x25] = 1;
    p[0x24] = Scr(8)[0];
    p[0x26] = Scr(8)[0];
}
}  // namespace

// original 0x4C4110: the mote - a triangle outline in two three-point gouraud
// lines (angles 0x200 / 0x600 / 0xA00, then 0xA00 / 0xE00 / 0x200) of radius
// 0x80 (types 0 and 1) or +9 * 4 + 0x80, perspective-projected, the shade
// 0x65B528[+4] * +0xA.
S20_EXPORT void __cdecl Magic087_DrawTriangle(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtCurrent(0xC);
    unsigned char* p = Gfx_PacketNext;
    MH_CALL(Gpu_SetLineG3)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char* sc = Sprite_Current;
    SetLong(Scr(0), sc[1] < 2 ? 0x80 : static_cast<std::int32_t>(sc[9] * 4u + 0x80));
    Corner(0, 0x200);
    Corner(8, 0x600);
    Corner(0x10, 0xA00);
    long depth;
    MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                               reinterpret_cast<float*>(p + 0x28), &depth);
    MH_CALL(Gte_PrimDepths3_10C)(p);
    sc = Sprite_Current;
    SetLong(Scr(8), static_cast<std::int32_t>(Mem(kM087Shades)[sc[4]] * static_cast<unsigned>(sc[0xA])));
    TriangleShade(p);
    LinkAtCurrent(0x34);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetLineG3)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    Corner(0, 0xA00);
    Corner(8, 0xE00);
    Corner(0x10, 0x200);
    MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                               reinterpret_cast<float*>(p + 0x28), &depth);
    MH_CALL(Gte_PrimDepths3_10C)(p);
    TriangleShade(p);
    LinkAtCurrent(0x34);
}

// original 0x4C4430: the first of pool A's 80 motes without +0 bit 0, marked
// and its index answered; 0xFF when all are in use. Only al is defined in the
// original's answer.
S20_EXPORT unsigned char __cdecl Magic087_PoolAlloc(void) {
    for (unsigned i = 0; i < kPoolACount; ++i) {
        unsigned char* const m = PoolA(i);
        if ((m[0] & 1) == 0) {
            m[0] = static_cast<unsigned char>(m[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC088 (Magic_Rows row 29)

// original 0x4C4490: the kind-2 task: its phase +1 through a seven-entry
// table on its stack - Magic088_Start, _TintOn, _Darken, 0x4EF7C0 (MAGIC167's:
// on once +0xB is 1 or less), _Lighten, _Apply, 0x4E5200 (MAGIC131's: the
// done flag and free once +0xB is 0) - unchecked; ours aborts past it.
S20_EXPORT void __cdecl Magic088_Task(void) {
    static constexpr std::uint32_t kPhases[7] = {bof3::addr::Magic088_Start, bof3::addr::Magic088_TintOn,
                                                 bof3::addr::Magic088_Darken, 0x4EF7C0,
                                                 bof3::addr::Magic088_Lighten, bof3::addr::Magic088_Apply,
                                                 0x4E5200};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 7) bof3::Fatal("Magic088_Task: phase %u, past the seven-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4C44E0: the source sprite's (0x904B4C, read once) position; +4
// the variant (Magic088_Variant), +0xB 0, +9 0x10, on; two kind-1 children
// (parameter 0x20) owned by this task and counted in +0xB, at its position:
// type 0 (+9 and +0xA 0) and type 1 (+4 and +9 this task's, +0xA 0x10).
// Then CLUT strip rows 26 and 27 copied from 0x4000 below; dirty; sound
// 0x100. BattleTask_Create's index is not tested.
S20_EXPORT void __cdecl Magic088_Start(void) {
    const unsigned char* const src = Pointer(at::kSource);
    CopyPosition(Sprite_Current, src);
    const unsigned char variant = MH_CALL(Magic088_Variant)();
    Sprite_Current[4] = variant;
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0x10;
    Bump(Sprite_Current[1], 1);
    unsigned index = MH_CALL(BattleTask_Create)(1, 0x20) & 0xFFu;
    unsigned char* sc = Sprite_Current;
    unsigned char* t = Task(index);
    SetLong(t + 0x80, PtrValue(sc));
    t[1] = 0;
    t[9] = 0;
    t[0xA] = 0;
    CopyPosition(t, sc);
    Bump(sc[0xB], 1);
    index = MH_CALL(BattleTask_Create)(1, 0x20) & 0xFFu;
    sc = Sprite_Current;
    t = Task(index);
    SetLong(t + 0x80, PtrValue(sc));
    t[1] = 1;
    t[4] = sc[4];
    t[9] = sc[9];
    t[0xA] = 0x10;
    CopyPosition(t, sc);
    Bump(sc[0xB], 1);
    for (std::uint32_t k = 0; k < 0x20; k += 2) {
        SetWord(Mem(kClutRow + k), Word(Mem(kClutRow + k - 0x4000)));
        SetWord(Mem(kClutRowNext + k), Word(Mem(kClutRowNext + k - 0x4000)));
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C4650: phase 1. +9 down; at 0 the source (read once, first)
// marked +0x5C = 1, its tints released and a new one set (0, 0, 0, 1), whose
// index goes to +0xA; +9 8; on.
S20_EXPORT void __cdecl Magic088_TintOn(void) {
    unsigned char* const src = Pointer(at::kSource);
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    src[0x5C] = 1;
    MH_CALL(Sprite_ReleaseTint)(src);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(src, 0, 0, 0, 1);
    Sprite_Current[0xA] = tint;
    Sprite_Current[9] = 8;
    Bump(Sprite_Current[1], 1);
}

// original 0x4C46B0: phase 2. The tint record +0xA's (unchecked) bytes +2
// and +3 down; +9 down; at 0 on.
S20_EXPORT void __cdecl Magic088_Darken(void) {
    unsigned char* const sc = Sprite_Current;
    Bump(Tint(sc[0xA])[2], -1);
    Bump(Tint(sc[0xA])[3], -1);
    Bump(sc[9], -1);
    if (Sprite_Current[9] == 0) Bump(Sprite_Current[1], 1);
}

// original 0x4C4700: phase 4. The tint record's +2 and +3 up; when +2 is back
// to 0: the source (read once, first) +0x5C 0, its tints released, the target
// flashed (BattleActor_Flash), on.
S20_EXPORT void __cdecl Magic088_Lighten(void) {
    unsigned char* const sc = Sprite_Current;
    unsigned char* const src = Pointer(at::kSource);
    Bump(Tint(sc[0xA])[2], 1);
    Bump(Tint(sc[0xA])[3], 1);
    if (Tint(sc[0xA])[2] != 0) return;
    src[0x5C] = 0;
    MH_CALL(Sprite_ReleaseTint)(src);
    MH_CALL(BattleActor_Flash)(Mem(at::kTarget)[0]);
    Bump(Sprite_Current[1], 1);
}

// original 0x4C4780: phase 5. Once the children are gone (+0xB 0): the stat
// change 0x4FB6F0(0x65C39C[+4], the target) - the library's; a kind-1 child
// (parameter 0x48) owned by this task with +4 = +4 + 4 when it answers
// non-zero, else 8; +9 1, +0xA 0; the count up; on. The index untested.
S20_EXPORT void __cdecl Magic088_Apply(void) {
    if (Sprite_Current[0xB] != 0) return;
    const unsigned char target = Mem(at::kTarget)[0];
    const unsigned char stat = Mem(kStatByVariant)[Sprite_Current[4]];
    const bool took = MH_AT(StatChangeFn, kLibStatChange)(stat, target) != 0;
    const unsigned index = MH_CALL(BattleTask_Create)(1, 0x48) & 0xFFu;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const t = Task(index);
    SetLong(t + 0x80, PtrValue(sc));
    t[4] = took ? static_cast<unsigned char>(sc[4] + 4) : 8;
    t[9] = 1;
    t[0xA] = 0;
    Bump(sc[0xB], 1);
    Bump(Sprite_Current[1], 1);
}

// original 0x4C4830: the variant, by the ability id (word 0x904B80). With the
// side byte 0x904B35 at 4: ids 0x1E..0xC0 index a byte table in .text
// (0x4C488C, read in place) choosing one of four answers - 1, 2, 0, 3 - and
// any other id is 3. On the other side: id 0x112 becomes 0x5A and answers 0,
// any other 3. Only al is defined.
S20_EXPORT unsigned char __cdecl Magic088_Variant(void) {
    unsigned char* const id = Mem(kAbility);
    if (Mem(kSide)[0] == 4) {
        const unsigned k = Word(id) - 0x1Eu;
        if (k > 0xA2) return 3;
        switch (Mem(kVariantIndex)[k]) {
        case 0: return 1;
        case 1: return 2;
        case 2: return 0;
        case 3: return 3;
        default: bof3::Fatal("Magic088_Variant: index %u past the four-entry jump table", (unsigned)Mem(kVariantIndex)[k]);
        }
    }
    if (Word(id) != 0x112) return 3;
    SetWord(id, 0x5A);
    return 0;
}

// original 0x4C4930: the child (kind 1, parameter 0x20): a jmp through
// 0x65B574 by +1 (two entries, the fan and the wave, unchecked).
S20_EXPORT void __cdecl Magic088_Child(void) { Entry(kM088ChildTypes, Sprite_Current[1])(); }

// original 0x4C4950: the fan. Its phase +2 through 0x65B57C (three - other
// overlays' 0x4C2D10, 0x4EF840, 0x4B1740 - unchecked); then while it lives
// the fan under the actor's matrix.
S20_EXPORT void __cdecl Magic088_FanRun(void) {
    Entry(kM088FanPhases, Sprite_Current[2])();
    if (Sprite_Current[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(Magic088_DrawFan)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C4980: sixteen gouraud triangles round the matrix's origin,
// radius 0xF0 (0x903850), the centre grey +9 * 12 (0x90385A), the rim dark;
// between two draw-mode packets (tpage 0x55, then 0x15), all at slot 5.
S20_EXPORT void __cdecl Magic088_DrawFan(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    const unsigned char* const sc = Sprite_Current;
    SetWord(Scr(0), 0xF0);
    SetWord(Scr(0xA), sc[9] * 12u);
    int v = MH_CALL(Math_Sin)(0);
    SetWord(Vtx(0x10), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
    v = MH_CALL(Math_Cos)(0);
    SetWord(Vtx(0x12), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        const std::uint16_t y = Word(Vtx(0x12));
        const std::uint16_t x = Word(Vtx(0x10));
        SetWord(Vtx(0), 0);
        SetWord(Vtx(2), 0);
        SetWord(Vtx(8), x);
        SetWord(Vtx(0xA), y);
        v = MH_CALL(Math_Sin)(a);
        SetWord(Vtx(0x10), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        v = MH_CALL(Math_Cos)(a);
        unsigned char* const p = Gfx_PacketNext;
        SetWord(Vtx(0x14), 0);
        SetWord(Vtx(0x12), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        SetWord(Vtx(0xC), 0);
        SetWord(Vtx(4), 0);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        long depth;
        MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                                   reinterpret_cast<float*>(p + 0x28), &depth);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[4] = Scr(0xA)[0];
        p[5] = Scr(0xA)[0];
        p[6] = Scr(0xA)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4C4B30: the wave. Its phase +2 through 0x65B5A8 (four - 0x4E47F0,
// another overlay's, then _WaveGrow, _WaveHold, _WaveFade - unchecked); then
// while it lives, past phase 0, the bands stepped and drawn under the
// actor's matrix.
S20_EXPORT void __cdecl Magic088_WaveRun(void) {
    Entry(kM088WavePhases, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(Magic088_WaveStep)();
    MH_CALL(Magic088_DrawWave)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C4B70: wave phase 1: +9 up; at 0x20 on.
S20_EXPORT void __cdecl Magic088_WaveGrow(void) {
    Bump(Sprite_Current[9], 1);
    if (Sprite_Current[9] == 0x20) Bump(Sprite_Current[2], 1);
}

// original 0x4C4B90: wave phase 2: +0xA down; at 0 the owner's count down and
// on.
S20_EXPORT void __cdecl Magic088_WaveHold(void) {
    Bump(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    Bump(Owner()[0xB], -1);
    Bump(Sprite_Current[2], 1);
}

// original 0x4C4BC0: wave phase 3: +9 down by 2; at 0 freed (a tail jmp).
S20_EXPORT void __cdecl Magic088_WaveFade(void) {
    Bump(Sprite_Current[9], -2);
    if (Sprite_Current[9] == 0) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C4BE0: the six band phases at 0x65B588 (dwords): the even ones
// up by one, the odd ones down.
S20_EXPORT void __cdecl Magic088_WaveStep(void) {
    for (unsigned i = 0; i < 6; ++i) {
        unsigned char* const d = Mem(kM088WaveBands + 4 * i);
        SetLong(d, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(d)) + ((i & 1) ? 0xFFFFFFFFu : 1u)));
    }
}

namespace {
// Magic088_DrawWave's z: -(sin(0x903856) * 0x903858 >> 12 + the dword
// 0x903858) - the dword, so the high word is 0x90385A's, and only the low
// word is kept.
std::uint16_t WaveZ(int s) {
    return static_cast<std::uint16_t>(0u - static_cast<std::uint32_t>(Add32(MulSar(s, S16(Scr(8)), 12), Long(Scr(8)))));
}
}  // namespace

// original 0x4C4C00: six bands of 64 flat quads round the matrix's origin,
// band b from radius 32 b to 32 (b + 1), each point lifted by the band's
// phase (0x65B588 + 4 b, low nibble) plus its column, as sin((phase + column)
// & 15 << 8) * +9 >> 12 + +9; the band's colour 0x65B52C row +4 * 6 + b
// times +9 / 2. One draw-mode packet first.
S20_EXPORT void __cdecl Magic088_DrawWave(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtCurrent(0xC);
    SetWord(Scr(8), Sprite_Current[9]);
    for (unsigned b = 0; b < 6; ++b) {
        const unsigned char* const phase = Mem(kM088WaveBands + 4 * b);
        const unsigned char* const sc = Sprite_Current;
        SetWord(Scr(2), (b + 1) << 5);
        SetWord(Scr(0), b << 5);
        const unsigned row = (b + sc[4] * 6u) * 3u;
        const unsigned half = sc[9] >> 1;
        SetWord(Scr(0xA), Mem(kM088Colours)[row] * half);
        SetWord(Scr(0xC), Mem(kM088Colours + 1)[row] * (sc[9] >> 1));
        SetWord(Scr(0xE), Mem(kM088Colours + 2)[row] * (sc[9] >> 1));
        SetWord(Scr(6), (phase[0] & 0xFu) << 8);
        int v = MH_CALL(Math_Cos)(0);
        SetWord(Vtx(8), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
        v = MH_CALL(Math_Sin)(0);
        SetWord(Vtx(0xA), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
        v = MH_CALL(Math_Sin)(S16(Scr(6)));
        SetWord(Vtx(0xC), WaveZ(v));
        SetWord(Scr(6), ((phase[0] + 1u) & 0xFu) << 8);
        v = MH_CALL(Math_Cos)(0);
        SetWord(Vtx(0x18), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        v = MH_CALL(Math_Sin)(0);
        SetWord(Vtx(0x1A), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        v = MH_CALL(Math_Sin)(S16(Scr(6)));
        SetWord(Vtx(0x1C), WaveZ(v));
        for (unsigned c = 1; c < 0x41; ++c) {
            SetWord(Vtx(0), Word(Vtx(8)));
            SetWord(Vtx(2), Word(Vtx(0xA)));
            SetWord(Vtx(4), Word(Vtx(0xC)));
            SetWord(Vtx(0x14), Word(Vtx(0x1C)));
            SetWord(Vtx(0x10), Word(Vtx(0x18)));
            SetWord(Vtx(0x12), Word(Vtx(0x1A)));
            SetWord(Scr(4), c << 6);
            v = MH_CALL(Math_Cos)(S16(Scr(4)));
            SetWord(Vtx(8), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
            v = MH_CALL(Math_Sin)(S16(Scr(4)));
            SetWord(Vtx(0xA), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
            v = MH_CALL(Math_Sin)(S16(Scr(6)));
            SetWord(Vtx(0xC), WaveZ(v));
            SetWord(Scr(6), ((c + phase[0] + 1u) & 0xFu) << 8);
            v = MH_CALL(Math_Cos)(S16(Scr(4)));
            SetWord(Vtx(0x18), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
            v = MH_CALL(Math_Sin)(S16(Scr(4)));
            SetWord(Vtx(0x1A), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
            v = MH_CALL(Math_Sin)(S16(Scr(6)));
            unsigned char* const p = Gfx_PacketNext;
            SetWord(Vtx(0x1C), WaveZ(v));
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            long depth;
            MH_CALL(Gte_RotTransPers4)(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), reinterpret_cast<float*>(p + 8),
                                       reinterpret_cast<float*>(p + 0x14), reinterpret_cast<float*>(p + 0x20),
                                       reinterpret_cast<float*>(p + 0x2C), &depth);
            MH_CALL(Gte_PrimDepths4_0C)(p);
            p[4] = Scr(0xA)[0];
            p[5] = Scr(0xC)[0];
            p[6] = Scr(0xE)[0];
            LinkAtCurrent(0x38);
        }
    }
}

// ===========================================================================
// MAGIC092 (Magic_Rows row 49)

// original 0x4C5680: the kind-2 task. Its phase +1 through a two-entry table
// on its stack (Magic092_Start, 0x4E5200 - MAGIC131's end); then every
// pool-B mote with +0 bit 0 run (Magic092_MoteRun) as MAGIC087's are.
S20_EXPORT void __cdecl Magic092_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic092_Start, 0x4E5200};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Magic092_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const sc = Sprite_Current;
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kPoolBCount; ++i) {
        unsigned char* const m = PoolB(i);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = Long(m + 0x80);
        Sprite_Current = m;
        SetLong(Mem(at::kOwner), its);
        MH_CALL(Magic092_MoteRun)();
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = sc;
    }
}

// original 0x4C5700: the pool's +0 / +1 / +2 cleared; +0xB 0; a kind-1 child
// (parameter 0x28) for every actor on the target's side that is not out
// (bit 0x40: the eight enemies, else the three members): owned by this task,
// +3 the actor's index on its side, at its position, +9 = 16 n + 1 by the
// count n made so far; the count +0xB up. Then CLUT strip row 26: cell 0
// cleared, cells 1..15 from 0x4000 below with bit 15; dirty; on. The index
// untested; Sprite_Current read again after each create.
S20_EXPORT void __cdecl Magic092_Start(void) {
    for (unsigned i = 0; i < kPoolBCount; ++i) {
        unsigned char* const m = PoolB(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    Sprite_Current[0xB] = 0;
    unsigned made = 0;
    const bool enemies = (Mem(at::kTarget)[0] & 0x40) != 0;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        const unsigned index = MH_CALL(BattleTask_Create)(1, 0x28) & 0xFFu;
        unsigned char* const sc = Sprite_Current;
        unsigned char* const t = Task(index);
        const unsigned char* const actor = enemies ? Enemy(i) : Member(i);
        SetLong(t + 0x80, PtrValue(sc));
        t[3] = static_cast<unsigned char>(i);
        CopyPosition(t, actor);
        t[9] = static_cast<unsigned char>((made << 4) + 1);
        ++made;
        Bump(sc[0xB], 1);
    }
    SetWord(Mem(kClutRow), 0);
    for (std::uint32_t a = kClutRow + 2; a < kClutRow + 0x20; a += 2) SetWord(Mem(a), Word(Mem(a - 0x4000)) | 0x8000u);
    Gfx_ClutStripDirty = 1;
    Bump(Sprite_Current[1], 1);
}

// original 0x4C58A0: the child (kind 1, parameter 0x28): a jmp through
// 0x65B5C8 by +1 (one entry, Magic092_ChildRun; unchecked).
S20_EXPORT void __cdecl Magic092_Child(void) { Entry(kM092ChildTypes, Sprite_Current[1])(); }

// original 0x4C58C0: its phase +2 through 0x65B5CC (four - _ChildSpawn,
// _ChildTint, 0x4E93C0 (MAGIC144's: on at +0xB bit 7), _ChildEnd -
// unchecked); then while it lives, past phase 0, 0x4E9420 (MAGIC144's draw)
// under the actor's matrix.
S20_EXPORT void __cdecl Magic092_ChildRun(void) {
    Entry(kM092ChildPhases, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_AT(VoidFn, kDrawSparks)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C5900: child phase 0. +9 down (the delay); at 0: +0xB 0 and
// three pool-B motes, each (when the pool answers one) owned by the child,
// +0 |= 0x41, +1 = type (0 for the first, 1 after), +2 0, +0xB = n, and
// counted; sound 0x100; on.
S20_EXPORT void __cdecl Magic092_ChildSpawn(void) {
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    Sprite_Current[0xB] = 0;
    for (unsigned n = 0; n < 3; ++n) {
        const unsigned char index = MH_CALL(Magic092_PoolAlloc)();
        if (index == 0xFF) continue;
        unsigned char* const sc = Sprite_Current;
        unsigned char* const m = PoolB(index);
        SetLong(m + 0x80, PtrValue(sc));
        m[0] = static_cast<unsigned char>(m[0] | 0x41);
        m[1] = n != 0 ? 1 : 0;
        m[2] = 0;
        m[0xB] = static_cast<unsigned char>(n);
        Bump(sc[0xB], 1);
    }
    MH_CALL(Sound_PlayById)(0x100);
    Bump(Sprite_Current[2], 1);
}

// original 0x4C59B0: child phase 1. +9 up by 2; at 0x10 the actor +3 (an
// enemy when 0x904B44 has bit 0x40, else a member; unchecked) has its tints
// released and a new one set (-8, -8, -8, 1), and the target flags 0x10 (by
// its battle index); on.
S20_EXPORT void __cdecl Magic092_ChildTint(void) {
    Bump(Sprite_Current[9], 2);
    const unsigned char* sc = Sprite_Current;
    if (sc[9] != 0x10) return;
    const bool enemy = (Mem(at::kTarget)[0] & 0x40) != 0;
    unsigned char* const rec = enemy ? Enemy(sc[3]) : Member(sc[3]);
    MH_CALL(Sprite_ReleaseTint)(rec);
    MH_CALL(Sprite_SetTint)(rec, 0xF8, 0xF8, 0xF8, 1);
    sc = Sprite_Current;
    MH_CALL(Battle_SetTargetFlags)(enemy ? static_cast<unsigned char>(sc[3] + 3) : sc[3], 0x10);
    Bump(Sprite_Current[2], 1);
}

// original 0x4C5A60: child phase 3. +9 down to 0; once +0xB is 0x80 (the
// motes gone): the actor's tints released, it flashed and its flag 0x40 set,
// the owner's count down, the child freed (a tail jmp).
S20_EXPORT void __cdecl Magic092_ChildEnd(void) {
    unsigned char* sc = Sprite_Current;
    if (sc[9] != 0) {
        Bump(sc[9], -1);
        sc = Sprite_Current;
    }
    if (sc[0xB] != 0x80) return;
    if (Mem(at::kTarget)[0] & 0x40) {
        MH_CALL(Sprite_ReleaseTint)(Enemy(sc[3]));
        MH_CALL(BattleActor_Flash)(static_cast<unsigned char>(Sprite_Current[3] + 3));
        MH_CALL(Battle_SetTargetFlag40)(static_cast<unsigned char>(Sprite_Current[3] + 3));
    } else {
        MH_CALL(Sprite_ReleaseTint)(Member(sc[3]));
        MH_CALL(BattleActor_Flash)(Sprite_Current[3]);
        MH_CALL(Battle_SetTargetFlag40)(Sprite_Current[3]);
    }
    Bump(Owner()[0xB], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C5B10: a pool-B mote: a jmp through 0x65B5DC by +1 (two: the
// flame, the spark; unchecked).
S20_EXPORT void __cdecl Magic092_MoteRun(void) { Entry(kM092MoteTypes, Sprite_Current[1])(); }

// original 0x4C5B30: the flame. Its phase +2 through 0x65B5E4 (four - MAGIC144's
// 0x4E9630 and 0x4E9690, _FlameGrow, MAGIC144's 0x4E96B0 - unchecked); then
// while it lives, past phase 0, the screen point and the flame (a tail jmp).
S20_EXPORT void __cdecl Magic092_FlameRun(void) {
    Entry(kM092FlamePhases, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(Magic092_DrawFlame)();
}

// original 0x4C5B60: flame phase 2: +9 and +0xA up; at +9 0x19 the owner's
// +0xB gets bit 7, on.
S20_EXPORT void __cdecl Magic092_FlameGrow(void) {
    Bump(Sprite_Current[9], 1);
    Bump(Sprite_Current[0xA], 1);
    if (Sprite_Current[9] != 0x19) return;
    Owner()[0xB] = static_cast<unsigned char>(Owner()[0xB] | 0x80);
    Bump(Sprite_Current[2], 1);
}

namespace {
// One of the flame's quads: the texture (clut (0, 0x1FA), page (0x340,
// 0x100)) after the corners and shades, then its texture rows.
void FlameTexture(unsigned char* p) {
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    SetWord(p + 0x2A, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
}
// The four x corners of a quad (+8, +0x1C, +0x30, +0x44) and the two rows
// (y - near at +0xC / +0x20, y - far at +0x34 / +0x48).
void FlameCorners(unsigned char* p, int x0, int x1, int x2, int x3, int top, int bottom) {
    PutFloat(p + 8, x0);
    PutFloat(p + 0x1C, x1);
    PutFloat(p + 0x30, x2);
    PutFloat(p + 0x44, x3);
    PutFloat(p + 0x20, top);
    PutFloat(p + 0xC, top);
    PutFloat(p + 0x48, bottom);
    PutFloat(p + 0x34, bottom);
}
void Shades(unsigned char* p, const unsigned char (&v)[12]) {
    static constexpr unsigned kAt[12] = {4, 5, 6, 0x18, 0x19, 0x1A, 0x2C, 0x2D, 0x2E, 0x40, 0x41, 0x42};
    for (unsigned k = 0; k < 12; ++k) p[kAt[k]] = v[k];
}
}  // namespace

// original 0x4C5BA0: the flame - 28 rows of four textured gouraud quads up
// from the actor's screen point (0x903854 / 0x903856), row r at angle a =
// 0x100 + 0x40 r: half-widths sin(a + 0x40) * +9 >> 12 (0x903858), sin(a) *
// +9 >> 12 (0x90385A), sin(a) * (+9 * 3 + the last row's Rand & 15) >> 12
// (0x90385E), sin(a + 0x40) * (+9 * 3 + Rand & 15) >> 12 (0x90385C); each row
// +0xA high. The two inner quads orange-red (0xA0 / 0x80, 0x20), the two outer
// fading to 1; the texture rows from the row number and +0xA (the stack bytes
// the original reuses across quads, kept as it keeps them).
S20_EXPORT void __cdecl Magic092_DrawFlame(void) {
    const unsigned char* sc = Sprite_Current;
    SetWord(Scr(4), Word(sc + 0x2E));
    SetWord(Scr(6), Word(sc + 0x30));
    const unsigned step = sc[0xA];
    unsigned counter = 4;
    SetWord(Scr(0), sc[9]);
    SetWord(Scr(2), sc[9] * 3u);
    int y = 0;
    const unsigned char g = 0x20;
    unsigned char row_a = 0, row_b = 0;   // the original's stack bytes +0x12 / +0x13
    for (int a = 0x100; a < 0x800; a += 0x40) {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        LinkAtCurrent(0xC);
        int v = MH_CALL(Math_Sin)(a + 0x40);
        SetWord(Scr(8), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        v = MH_CALL(Math_Sin)(a);
        SetWord(Scr(0xA), static_cast<unsigned>(MulSar(v, S16(Scr(0)), 12)));
        v = MH_CALL(Math_Sin)(a);
        SetWord(Scr(0xE), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
        const int r = MH_CALL(Rand)();
        SetWord(Scr(2), (static_cast<unsigned>(r) & 0xFu) + Sprite_Current[9] * 3u);
        v = MH_CALL(Math_Sin)(a + 0x40);
        unsigned char* p = Gfx_PacketNext;
        SetWord(Scr(0xC), static_cast<unsigned>(MulSar(v, S16(Scr(2)), 12)));
        const int x = S16(Scr(4));
        const int top = S16(Scr(6)) - (y + static_cast<int>(step));
        const int bottom = S16(Scr(6)) - y;
        // the inner left quad
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        FlameCorners(p, x - S16(Scr(8)), x, x - S16(Scr(0xA)), x, top, bottom);
        Shades(p, {0xA0, g, g, 0x80, 0x80, g, 0xA0, g, g, 0x80, 0x80, g});
        FlameTexture(p);
        p[0x28] = 0x80;
        p[0x14] = static_cast<unsigned char>(0x80 - Scr(8)[0]);
        const auto base6 = static_cast<unsigned char>(static_cast<unsigned char>(counter * 6) + step);
        row_a = static_cast<unsigned char>(base6 + 0xE);
        row_b = static_cast<unsigned char>(base6 + 8);
        p[0x15] = row_a;
        p[0x29] = row_a;
        p[0x3D] = row_b;
        p[0x3C] = static_cast<unsigned char>(0x80 - Scr(0xA)[0]);
        p[0x50] = 0x80;
        p[0x51] = row_b;
        LinkAtCurrent(0x54);
        // the inner right quad
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        FlameCorners(p, x, S16(Scr(8)) + x, x, S16(Scr(0xA)) + x, top, bottom);
        Shades(p, {0x80, 0x80, g, 0xA0, g, g, 0x80, 0x80, g, 0xA0, g, g});
        FlameTexture(p);
        p[0x14] = 0x80;
        p[0x15] = row_a;
        p[0x29] = row_a;
        p[0x28] = static_cast<unsigned char>(Scr(8)[0] + 0x80);
        p[0x3C] = 0x80;
        p[0x3D] = row_b;
        p[0x51] = row_b;
        p[0x50] = static_cast<unsigned char>(Scr(0xA)[0] + 0x80);
        LinkAtCurrent(0x54);
        // the outer left quad
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        FlameCorners(p, x - S16(Scr(0xC)), x - S16(Scr(8)), x - S16(Scr(0xE)), x - S16(Scr(0xA)), top, bottom);
        Shades(p, {1, 1, 1, 0x80, g, g, 1, 1, 1, 0x80, g, g});
        FlameTexture(p);
        p[0x14] = static_cast<unsigned char>(0x80 - Scr(0xC)[0]);
        const auto base3 = static_cast<unsigned char>(static_cast<unsigned char>(counter * 3) + step);
        row_b = static_cast<unsigned char>((base3 + 7) << 1);
        row_a = static_cast<unsigned char>((base3 + 4) << 1);
        p[0x15] = row_b;
        p[0x29] = row_b;
        p[0x28] = static_cast<unsigned char>(0x80 - Scr(8)[0]);
        p[0x3C] = static_cast<unsigned char>(0x80 - Scr(0xE)[0]);
        p[0x3D] = row_a;
        p[0x51] = row_a;
        p[0x50] = static_cast<unsigned char>(0x80 - Scr(0xA)[0]);
        LinkAtCurrent(0x54);
        // the outer right quad
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyGT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        FlameCorners(p, S16(Scr(8)) + x, S16(Scr(0xC)) + x, S16(Scr(0xA)) + x, S16(Scr(0xE)) + x, top, bottom);
        Shades(p, {0x80, g, g, 1, 1, 1, 0x80, g, g, 1, 1, 1});
        FlameTexture(p);
        p[0x15] = row_b;
        p[0x14] = static_cast<unsigned char>(0x80 - Scr(8)[0]);
        p[0x29] = row_b;
        p[0x28] = static_cast<unsigned char>(0x80 - Scr(0xC)[0]);
        p[0x3C] = static_cast<unsigned char>(0x80 - Scr(0xA)[0]);
        p[0x3D] = row_a;
        p[0x51] = row_a;
        p[0x50] = static_cast<unsigned char>(0x80 - Scr(0xE)[0]);
        LinkAtCurrent(0x54);
        ++counter;
        y += static_cast<int>(step);
    }
}

// original 0x4C6250: the spark. Its phase +2 through 0x65B5F4 (four, MAGIC144's
// and MAGIC131's; unchecked); then while it lives, past phase 0, the screen
// point moved 16 left (type +0xB 1) or right and 12 up, and 0x4E9850
// (MAGIC144's draw; a tail jmp).
S20_EXPORT void __cdecl Magic092_SparkRun(void) {
    Entry(kM092SparkPhases, Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const s = Sprite_Current;
    SetWord(s + 0x2E, Word(s + 0x2E) + (s[0xB] == 1 ? 0xFFF0u : 0x10u));
    SetWord(Sprite_Current + 0x30, Word(Sprite_Current + 0x30) + 0xFFF4u);
    MH_AT(VoidFn, kDrawSpark)();
}

// original 0x4C62A0: pool B's alloc, as Magic087_PoolAlloc over 48 motes.
S20_EXPORT unsigned char __cdecl Magic092_PoolAlloc(void) {
    for (unsigned i = 0; i < kPoolBCount; ++i) {
        unsigned char* const m = PoolB(i);
        if ((m[0] & 1) == 0) {
            m[0] = static_cast<unsigned char>(m[0] | 1);
            return static_cast<unsigned char>(i);
        }
    }
    return 0xFF;
}

void MagicS20_Inject() {
    if (bof3::WantsShadow("magic_s20")) magic_s20::SelfTest();
    BOF3_INJECT(Magic087_Task);
    BOF3_INJECT(Magic087_Start);
    BOF3_INJECT(Magic087_Child);
    BOF3_INJECT(Magic087_ChildPhase);
    BOF3_INJECT(Magic087_ChildSpawn);
    BOF3_INJECT(Magic087_ChildWait);
    BOF3_INJECT(Magic087_ChildRing);
    BOF3_INJECT(Magic087_ChildGrow);
    BOF3_INJECT(Magic087_ChildEnd);
    BOF3_INJECT(Magic087_DrawColumn);
    BOF3_INJECT(Magic087_MoteRun);
    BOF3_INJECT(Magic087_OrbitRun);
    BOF3_INJECT(Magic087_OrbitStart);
    BOF3_INJECT(Magic087_OrbitSpin);
    BOF3_INJECT(Magic087_OrbitWait);
    BOF3_INJECT(Magic087_OrbitFade);
    BOF3_INJECT(Magic087_RiseRun);
    BOF3_INJECT(Magic087_RiseStart);
    BOF3_INJECT(Magic087_RiseStep);
    BOF3_INJECT(Magic087_PushMatrix);
    BOF3_INJECT(Magic087_DrawTriangle);
    BOF3_INJECT(Magic087_PoolAlloc);
    BOF3_INJECT(Magic088_Task);
    BOF3_INJECT(Magic088_Start);
    BOF3_INJECT(Magic088_TintOn);
    BOF3_INJECT(Magic088_Darken);
    BOF3_INJECT(Magic088_Lighten);
    BOF3_INJECT(Magic088_Apply);
    BOF3_INJECT(Magic088_Variant);
    BOF3_INJECT(Magic088_Child);
    BOF3_INJECT(Magic088_FanRun);
    BOF3_INJECT(Magic088_DrawFan);
    BOF3_INJECT(Magic088_WaveRun);
    BOF3_INJECT(Magic088_WaveGrow);
    BOF3_INJECT(Magic088_WaveHold);
    BOF3_INJECT(Magic088_WaveFade);
    BOF3_INJECT(Magic088_WaveStep);
    BOF3_INJECT(Magic088_DrawWave);
    BOF3_INJECT(Magic092_Task);
    BOF3_INJECT(Magic092_Start);
    BOF3_INJECT(Magic092_Child);
    BOF3_INJECT(Magic092_ChildRun);
    BOF3_INJECT(Magic092_ChildSpawn);
    BOF3_INJECT(Magic092_ChildTint);
    BOF3_INJECT(Magic092_ChildEnd);
    BOF3_INJECT(Magic092_MoteRun);
    BOF3_INJECT(Magic092_FlameRun);
    BOF3_INJECT(Magic092_FlameGrow);
    BOF3_INJECT(Magic092_DrawFlame);
    BOF3_INJECT(Magic092_SparkRun);
    BOF3_INJECT(Magic092_PoolAlloc);
}
