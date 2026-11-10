// Moved here by tools/move_funcs.py from magic_s20.cpp (docs/source-layout-round.md).
#include "game/magic/magic_087.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s20.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/code_bytes.h"
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

// The two pools of 0x84-byte motes: MAGIC087's 80 and MAGIC092's 48, each
// walked by its task with Sprite_Current the mote and 0x93B940 its +0x80.
constexpr ::bof3::image::Va kPoolA{0x6906D8};

constexpr unsigned kPoolACount = 0x50;

constexpr std::uint32_t kPoolStride = 0x84;

// The .data dispatch tables, read in place (docs/magic_s20.md section 2).
constexpr ::bof3::image::Va kM087ChildTypes{0x65B4E8};    // by +1: 0x4C3720

constexpr ::bof3::image::Va kM087ChildPhases{0x65B4EC};   // by +2: five

constexpr ::bof3::image::Va kM087MoteTypes{0x65B500};     // by +1: orbit, orbit, rise, rise

constexpr ::bof3::image::Va kM087OrbitPhases{0x65B510};   // by +2: four

constexpr ::bof3::image::Va kM087RisePhases{0x65B520};    // by +2: two

constexpr ::bof3::image::Va kM087Shades{0x65B528};        // u8 by +4

// The effect's scratch (words 0x903850..0x90385E, Scratch_Swap among them)
// and the vertex scratch (Prim_VertexScratch: SVECTORs 8 bytes apart).
constexpr ::bof3::image::Va kScratch{SharedScratch_addr};

// The CLUT strip rows the starts write (docs/magic_fx_reached.md: kClutRow).
constexpr std::uint32_t kClutRow = 0x812980;

// An enemy record's byte +0xF0 (its type) indexes a 0x8C-stride table whose
// byte at 0x8C564F lifts MAGIC087's column (read in place).
constexpr ::bof3::image::Va kEnemyLift{0x8C564F};

constexpr std::uint32_t kEnemyLiftStride = 0x8C;

unsigned char* Scr(unsigned k) { return Mem(::bof3::Img(kScratch.v) + k); }

unsigned char* Vtx(unsigned k) { return Mem(::bof3::Img(Prim_VertexScratch_addr) + k); }

const short* VtxP(unsigned k) { return reinterpret_cast<const short*>(Vtx(k)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

unsigned char* Task(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + index * at::kTaskStride); }

unsigned char* PoolA(unsigned index) { return Mem(::bof3::Img(kPoolA.v) + index * kPoolStride); }

unsigned char* Enemy(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

unsigned char* Member(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::int32_t PtrValue(const unsigned char* p) {
    return static_cast<std::int32_t>(Low32(p));
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
    rv::Store(rv::As<records::SpriteHead>(to)->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::As<records::SpriteHead>(to)->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::As<records::SpriteHead>(to)->height, rv::As<records::SpriteHead>(from)->height);
}

// MapView_LinkPrimAt at Sprite_Current's position (read now).
void LinkAtCurrent(unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z), 2, size);
}

// Bytes +0..+4 of Sprite_Current cleared, the pointer read again for each:
// a pool mote freed.
void FreeMote() {
    for (unsigned k = 0; k < 5; ++k) Sprite_Current[k] = 0;
}

}  // namespace

#define S20_EXPORT extern "C" __attribute__((disable_tail_calls))

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
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kPoolACount; ++i) {
        unsigned char* const m = PoolA(i);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = static_cast<std::int32_t>(tm->owner);
        Sprite_Current = m;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        MH_CALL(Magic087_MoteRun)();
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
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
    if (Mem(::bof3::Img(at::kTarget))[0] & 0x40) {
        unsigned char nine = 1;
        for (unsigned i = 0; i < 8; ++i, nine = static_cast<unsigned char>(nine + 8)) {
            if (MH_CALL(Battle_ActorIsOut)(i + 3) != 0) continue;
            const unsigned index = MH_CALL(BattleTask_Create)(1, 0x27);
            unsigned char* const sc = Sprite_Current;
            unsigned char* const t = Task(index & 0xFF);
            records::BattleTask* const tt = rv::As<records::BattleTask>(t);
            const unsigned char* const e = Enemy(i);
            const records::BattleEnemy* const te = rv::AsConst<records::BattleEnemy>(e);
            tt->owner = PtrValue(sc);
            t[4] = sc[0xB];
            t[9] = nine;
            tt->x = te->x;
            tt->z = te->z;
            rv::Store(tt->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(te->height) + 0x800000u));
            rv::Store<unsigned>(tt->height_whole, tt->height_whole + Mem(::bof3::Img(kEnemyLift.v) + e[0xF0] * kEnemyLiftStride)[0]);
            Bump(sc[0xB], 1);
        }
    } else {
        unsigned char nine = 1;
        for (unsigned i = 0; i < 3; ++i, nine = static_cast<unsigned char>(nine + 8)) {
            if (MH_CALL(Battle_ActorIsOut)(i) != 0) continue;
            const unsigned index = MH_CALL(BattleTask_Create)(1, 0x27);
            unsigned char* const sc = Sprite_Current;
            unsigned char* const t = Task(index & 0xFF);
            records::BattleTask* const tt = rv::As<records::BattleTask>(t);
            const unsigned char* const m = Member(i);
            const records::PartyMember* const m_rec = rv::AsConst<records::PartyMember>(m);
            tt->owner = PtrValue(sc);
            t[4] = sc[0xB];
            t[9] = nine;
            tt->x = m_rec->x;
            tt->z = m_rec->z;
            rv::Store(tt->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(m_rec->height) + 0x800000u));
            Bump(sc[0xB], 1);
        }
    }
    for (std::uint32_t a = ::bof3::Img(kClutRow) + 2; a < ::bof3::Img(kClutRow) + 0x20; a += 2) SetWord(Mem(a), Word(Mem(a - 0x7400)) | 0x8000u);
    SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400), Word(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 - 0x7400)));
    Gfx_ClutStripDirty = 1;
    Bump(Sprite_Current[1], 1);
}

// original 0x4C3700: the child (kind 1, parameter 0x27): a jmp through
// 0x65B4E8 by +1 (one entry, Magic087_ChildPhase; the index unchecked).
S20_EXPORT void __cdecl Magic087_Child(void) { Entry(::bof3::Img(kM087ChildTypes.v), Sprite_Current[1])(); }

// original 0x4C3720: a jmp through 0x65B4EC by +2 (five phases, unchecked).
S20_EXPORT void __cdecl Magic087_ChildPhase(void) { Entry(::bof3::Img(kM087ChildPhases.v), Sprite_Current[2])(); }

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
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        tm->owner = PtrValue(sc);
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
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        tm->owner = PtrValue(sc);
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
        rv::Store<std::int32_t>(rv::As<records::BattleTask>(m)->owner, PtrValue(sc));
        m[1] = 2;
        index = MH_CALL(Magic087_PoolAlloc)() & 0xFFu;
        sc = Sprite_Current;
        m = PoolA(index);
        rv::Store<std::int32_t>(rv::As<records::BattleTask>(m)->owner, PtrValue(sc));
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
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0x100));
        rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
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
S20_EXPORT void __cdecl Magic087_MoteRun(void) { Entry(::bof3::Img(kM087MoteTypes.v), Sprite_Current[1])(); }

// original 0x4C3CE0: an orbiting mote. Its phase +2 through 0x65B510 (four,
// unchecked); then while it lives, in phases 1 and 2, its triangle under its
// own matrix.
S20_EXPORT void __cdecl Magic087_OrbitRun(void) {
    Entry(::bof3::Img(kM087OrbitPhases.v), Sprite_Current[2])();
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
    rv::Current<records::BattleTask>()->step_x = 0x100;
    rv::Current<records::BattleTask>()->work_18 = 4;
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
    rv::Store(rv::As<records::BattleTask>(sc)->step_x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->step_x) - static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->work_18)));
    SetLong(Scr(0), rv::Current<records::BattleTask>()->step_x);
    const int s = MH_CALL(Math_Sin)(Long(Scr(4)));
    rv::Store(rv::Current<records::BattleTask>()->x, Add32(MulSar(s, Long(Scr(0)), 3), rv::As<records::SpriteHead>(Owner())->x));
    const int c = MH_CALL(Math_Cos)(Long(Scr(4)));
    rv::Store(rv::Current<records::BattleTask>()->z, Add32(MulSar(c, Long(Scr(0)), 3), rv::As<records::SpriteHead>(Owner())->z));
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    sc = Sprite_Current;
    if (sc[0xA] < 0x21) Bump(sc[0xA], 1);
    sc = Sprite_Current;
    if (rv::As<records::BattleTask>(sc)->step_x != 0) return;
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
    Entry(::bof3::Img(kM087RisePhases.v), Sprite_Current[2])();
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
    rv::Store(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->height) + 0xFFF80000u));
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
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    const short rot[4] = {0, 0, static_cast<short>((sc[0xB] & 0x3F) << 6), 0};
    short v[4];
    v[0] = static_cast<short>((tsc->x >> 9) - 0x4000);
    v[1] = static_cast<short>((tsc->z >> 9) - 0x4000);
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
    SetLong(Scr(8), static_cast<std::int32_t>(Mem(::bof3::Img(kM087Shades.v))[sc[4]] * static_cast<unsigned>(sc[0xA])));
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

void Magic087_Inject() {
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
}
