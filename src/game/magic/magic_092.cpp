// Moved here by tools/move_funcs.py from magic_s20.cpp, magic_s21.cpp (docs/source-layout-round.md).
#include "game/magic/magic_092.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s20.h"
#include "game/magic_s21.h"
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

constexpr ::bof3::image::Va kPoolB{0x693018};

constexpr unsigned kPoolBCount = 0x30;

constexpr std::uint32_t kPoolStride = 0x84;

constexpr ::bof3::image::Va kM092ChildTypes{0x65B5C8};    // by +1: 0x4C58C0

constexpr ::bof3::image::Va kM092ChildPhases{0x65B5CC};   // by +2: four

constexpr ::bof3::image::Va kM092MoteTypes{0x65B5DC};     // by +1: flame, spark

constexpr ::bof3::image::Va kM092FlamePhases{0x65B5E4};   // by +2: four

constexpr ::bof3::image::Va kM092SparkPhases{0x65B5F4};   // by +2: four

// The effect's scratch (words 0x903850..0x90385E, Scratch_Swap among them)
// and the vertex scratch (Prim_VertexScratch: SVECTORs 8 bytes apart).
constexpr ::bof3::image::Va kScratch{SharedScratch_addr};

// The CLUT strip rows the starts write (docs/magic_fx_reached.md: kClutRow).
constexpr std::uint32_t kClutRow = 0x812980;

// Other groups' callees, ours now, by name: the effect library's
// MagicFx_ApplyBuff, S32's WallOfFire_DrawDisc and WallOfFireSpark_Draw.
using VoidFn = void (__cdecl*)();

unsigned char* Scr(unsigned k) { return Mem(::bof3::Img(kScratch.v) + k); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

unsigned char* Task(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + index * at::kTaskStride); }

unsigned char* PoolB(unsigned index) { return Mem(::bof3::Img(kPoolB.v) + index * kPoolStride); }

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

// `fild dword` then `fstp dword`: the integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
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

}  // namespace

#define S20_EXPORT extern "C" __attribute__((disable_tail_calls))

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

// The pool of flame columns and sparks MAGIC093's task runs (32 of the task
// slots' 0x84 bytes: +0 bit 0 in use, +1 the kind, +0x80 the owner), and the
// 56 signed jitter bytes MAGIC095's start fills, straight after it.
// kPool: now FlamePool (symbols.toml).
constexpr unsigned kPoolSlots = 32;

// kVertex: now Prim_VertexScratch (symbols.toml).
unsigned char* ScrMagicS21(unsigned off) { return Mem(::bof3::Img(kScratch.v) + off); }

unsigned char* Vtx(unsigned off) { return Mem(::bof3::Img(Prim_VertexScratch_addr) + off); }

const short* VtxP(unsigned off) { return reinterpret_cast<const short*>(Vtx(off)); }

std::int32_t L(std::uint32_t address) { return Long(Mem(address)); }

// A CLUT strip's two copies: the live one at 0x812980 and the buffer 0x4000
// below it (docs/magic_fx_reached.md section 4, FxDiscFan_Start's).
constexpr std::uint32_t kClut = 0x812980;

constexpr std::uint32_t kClutEnd = 0x812B80;

// imul r32, r/m32: the low 32 bits, then an arithmetic shift.
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

void BumpMagicS21(unsigned char& b, int d = 1) { b = static_cast<unsigned char>(b + d); }

unsigned char U8(int v) { return static_cast<unsigned char>(v); }

float* F(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }

void PutF(unsigned char* prim, unsigned off, std::int32_t v) { *F(prim, off) = static_cast<float>(v); }

std::uint32_t Addr(const void* p) { return Low32(p); }

// An actor's record as the originals index them: 0..2 the party (stride
// 0x14C), else an enemy (index - 3, stride 0x128), none of it checked.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

void CallCell(std::uint32_t table, unsigned index) {
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(L(table + 4 * index))))();
}

// The link of a primitive at the task's position: MapView_LinkPrimAt(+0x34,
// +0x38, slot, size), Sprite_Current read once.
void LinkAtTask(int slot, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z),
                                slot, size);
}

}  // namespace

#define MS21_EXPORT extern "C" __attribute__((disable_tail_calls))

// The column's place on its circle: radius word 0x903850 (x 1/8), angle word
// 0x90385A = (+4 & 7) << 9, round the owner.
static void ColumnPlace() {
    const unsigned char* sc = Sprite_Current;
    SetWord(ScrMagicS21(0xA), static_cast<unsigned>((sc[4] & 7) << 9));
    const int s = MH_CALL(Math_Sin)(S16(ScrMagicS21(0xA)));
    rv::Store(rv::Current<records::BattleTask>()->x, (Mul(s, S16(ScrMagicS21(0))) >> 3) + rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleE2_Owner_addr)))->x);
    const int c = MH_CALL(Math_Cos)(S16(ScrMagicS21(0xA)));
    rv::Store(rv::Current<records::BattleTask>()->z, (Mul(c, S16(ScrMagicS21(0))) >> 3) + rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleE2_Owner_addr)))->z);
}

// original 0x4C5680: the kind-2 task. Its phase +1 through a two-entry table
// on its stack (Magic092_Start, 0x4E5200 - MAGIC131's end); then every
// pool-B mote with +0 bit 0 run (Magic092_MoteRun) as MAGIC087's are.
S20_EXPORT void __cdecl Magic092_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Magic092_Start, bof3::addr::MagicFx_EndWhenChildrenDone};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Magic092_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const sc = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kPoolBCount; ++i) {
        unsigned char* const m = PoolB(i);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = static_cast<std::int32_t>(tm->owner);
        Sprite_Current = m;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        MH_CALL(Magic092_MoteRun)();
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
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
    const bool enemies = (Mem(::bof3::Img(at::kTarget))[0] & 0x40) != 0;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        const unsigned index = MH_CALL(BattleTask_Create)(1, 0x28) & 0xFFu;
        unsigned char* const sc = Sprite_Current;
        unsigned char* const t = Task(index);
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        const unsigned char* const actor = enemies ? Enemy(i) : Member(i);
        tt->owner = PtrValue(sc);
        t[3] = static_cast<unsigned char>(i);
        CopyPosition(t, actor);
        t[9] = static_cast<unsigned char>((made << 4) + 1);
        ++made;
        Bump(sc[0xB], 1);
    }
    SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400), 0);
    for (std::uint32_t a = ::bof3::Img(kClutRow) + 2; a < ::bof3::Img(kClutRow) + 0x20; a += 2) SetWord(Mem(a), Word(Mem(a - 0x4000)) | 0x8000u);
    Gfx_ClutStripDirty = 1;
    Bump(Sprite_Current[1], 1);
}

// original 0x4C58A0: the child (kind 1, parameter 0x28): a jmp through
// 0x65B5C8 by +1 (one entry, Magic092_ChildRun; unchecked).
S20_EXPORT void __cdecl Magic092_Child(void) { Entry(::bof3::Img(kM092ChildTypes.v), Sprite_Current[1])(); }

// original 0x4C58C0: its phase +2 through 0x65B5CC (four - _ChildSpawn,
// _ChildTint, 0x4E93C0 (MAGIC144's: on at +0xB bit 7), _ChildEnd -
// unchecked); then while it lives, past phase 0, 0x4E9420 (MAGIC144's draw)
// under the actor's matrix.
S20_EXPORT void __cdecl Magic092_ChildRun(void) {
    Entry(::bof3::Img(kM092ChildPhases.v), Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_AT(VoidFn, bof3::addr::WallOfFire_DrawDisc)();
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
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        tm->owner = PtrValue(sc);
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
    const bool enemy = (Mem(::bof3::Img(at::kTarget))[0] & 0x40) != 0;
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
    if (Mem(::bof3::Img(at::kTarget))[0] & 0x40) {
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
S20_EXPORT void __cdecl Magic092_MoteRun(void) { Entry(::bof3::Img(kM092MoteTypes.v), Sprite_Current[1])(); }

// original 0x4C5B30: the flame. Its phase +2 through 0x65B5E4 (four - MAGIC144's
// 0x4E9630 and 0x4E9690, _FlameGrow, MAGIC144's 0x4E96B0 - unchecked); then
// while it lives, past phase 0, the screen point and the flame (a tail jmp).
S20_EXPORT void __cdecl Magic092_FlameRun(void) {
    Entry(::bof3::Img(kM092FlamePhases.v), Sprite_Current[2])();
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
    SetWord(Scr(4), static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_x));
    SetWord(Scr(6), static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_y));
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
    Entry(::bof3::Img(kM092SparkPhases.v), Sprite_Current[2])();
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const s = Sprite_Current;
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_x, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x) + (s[0xB] == 1 ? 0xFFF0u : 0x10u));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y) + 0xFFF4u);
    MH_AT(VoidFn, bof3::addr::WallOfFireSpark_Draw)();
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

// original 0x4C6300 (Magic_Rows row 103): the kind-2 task. Its phase +1
// through a two-entry stack table - Inferno_Start, Inferno_End - then every
// pool slot with bit 0 run through FlamePool_Dispatch as Sprite_Current, its
// +0x80 the owner, both put back after each. The task and the owner are read
// after the phase, once. The index is not checked by the original: ours
// aborts past 1.
MS21_EXPORT void __cdecl Inferno_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Inferno_Start, bof3::addr::Inferno_End};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Inferno_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const task = Sprite_Current;
    const std::int32_t owner = L(::bof3::Img(BattleE2_Owner_addr));
    for (unsigned i = 0; i < kPoolSlots; ++i) {
        unsigned char* const slot = Mem(::bof3::Img(FlamePool_addr) + i * at::kTaskStride);
        if ((slot[0] & 1) == 0) continue;
        const std::int32_t its = Long(slot + 0x80);
        Sprite_Current = slot;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        MH_CALL(FlamePool_Dispatch)();
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = task;
    }
}

// original 0x4C6380: bytes +0..+2 of every pool slot cleared; the task to
// the targets' centre; +0xB and +9 0, the phase on. Then a scorch child
// (kind 1, 0x4D: +0x80 the task, +1 0, +3 the index, +9 0x10) on every
// target not out - the eight enemies when the target byte has bit 0x40, else
// the three party members - counted in the task's +9; eight flame columns
// (pool kind 0, +4 the index) and sixteen sparks (kind 1, +0xB the index, +9
// (index + 3) * 8), counted in +0xB; the CLUT strip made semi-transparent
// (every word of the buffer 0x4000 below with bit 15, into 0x812980..);
// Gfx_ClutStripDirty; sound 0x100. The pool's answer is used unchecked (0xFF
// with every slot in use: a write past the pool).
MS21_EXPORT void __cdecl Inferno_Start(void) {
    for (std::uint32_t p = ::bof3::Img(FlamePool_addr) + 1; p < ::bof3::Img(FlamePool_addr) + kPoolSlots * at::kTaskStride + 1; p += at::kTaskStride) {
        Mem(p)[-1] = 0;
        Mem(p)[0] = 0;
        Mem(p)[1] = 0;
    }
    MH_CALL(Inferno_TargetCentre)();
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0;
    BumpMagicS21(Sprite_Current[1]);
    const bool enemies = (Mem(::bof3::Img(at::kTarget))[0] & 0x40) != 0;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        const unsigned k = MH_CALL(BattleTask_Create)(1, 0x4D);
        unsigned char* const child = Mem(::bof3::Img(BattleTask_Slots_addr) + k * at::kTaskStride);
        unsigned char* const sc = Sprite_Current;
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(sc)));
        child[1] = 0;
        child[3] = U8(i);
        child[9] = 0x10;
        BumpMagicS21(sc[9]);
    }
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned k = MH_CALL(FlamePool_Alloc)();
        unsigned char* const slot = Mem(::bof3::Img(FlamePool_addr) + k * at::kTaskStride);
        unsigned char* const sc = Sprite_Current;
        SetLong(slot + 0x80, static_cast<std::int32_t>(Addr(sc)));
        slot[1] = 0;
        slot[4] = U8(i);
        BumpMagicS21(sc[0xB]);
    }
    for (unsigned i = 0; i < 16; ++i) {
        const unsigned k = MH_CALL(FlamePool_Alloc)();
        unsigned char* const slot = Mem(::bof3::Img(FlamePool_addr) + k * at::kTaskStride);
        unsigned char* const sc = Sprite_Current;
        SetLong(slot + 0x80, static_cast<std::int32_t>(Addr(sc)));
        slot[1] = 1;
        slot[0xB] = U8(i);
        slot[9] = U8((i + 3) << 3);
        BumpMagicS21(sc[0xB]);
    }
    for (std::uint32_t p = ::bof3::Img(kClut); p < ::bof3::Img(kClutEnd); p += 2) SetWord(Mem(p), Word(Mem(p - 0x4000)) | 0x8000u);
    Mem(::bof3::Img(Gfx_ClutStripDirty_addr))[0] = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C6540: held while the task's +0xB (flames) or +9 (scorches)
// counts; then the done flag and free. Sprite_Current read once.
MS21_EXPORT void __cdecl Inferno_End(void) {
    const unsigned char* const sc = Sprite_Current;
    if (sc[0xB] != 0 || sc[9] != 0) return;
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C6560: jmp [0x65B604 + 4 * +1] - one entry, FxScorch_Run.
MS21_EXPORT void __cdecl FxScorch_Dispatch(void) { CallCell(::bof3::Img(FxScorch_Phases_addr), Sprite_Current[1]); }

// original 0x4C6580: jmp [0x65B608 + 4 * +2] - Delay, Hit, Free.
MS21_EXPORT void __cdecl FxScorch_Run(void) { CallCell(::bof3::Img(FxScorch_Steps_addr), Sprite_Current[2]); }

// original 0x4C65A0: +9 down; at 0 the record of its actor (+3: an enemy's
// index with the target byte's bit 0x40, else a party member's) has its tint
// released and set to (-8, -8, -8, 1), the actor's flags 0x10; the step on.
MS21_EXPORT void __cdecl FxScorch_Delay(void) {
    BumpMagicS21(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0) return;
    if (Mem(::bof3::Img(at::kTarget))[0] & 0x40) {
        unsigned char* const r = EnemyRecord(sc[3]);
        MH_CALL(Sprite_ReleaseTint)(r);
        MH_CALL(Sprite_SetTint)(r, 0xF8, 0xF8, 0xF8, 1);
        MH_CALL(Battle_SetTargetFlags)(static_cast<unsigned char>(Sprite_Current[3] + 3), 0x10);
    } else {
        unsigned char* const r = PartyRecord(sc[3]);
        MH_CALL(Sprite_ReleaseTint)(r);
        MH_CALL(Sprite_SetTint)(r, 0xF8, 0xF8, 0xF8, 1);
        MH_CALL(Battle_SetTargetFlags)(Sprite_Current[3], 0x10);
    }
    BumpMagicS21(Sprite_Current[2]);
}

// original 0x4C6650: held while the owner's +0xB (its flames) is 8 or more;
// then the actor's flag 0x40, its tint released, it flashed; the step on.
MS21_EXPORT void __cdecl FxScorch_Hit(void) {
    if (Pointer(::bof3::Img(BattleE2_Owner_addr))[0xB] >= 8) return;
    const bool enemies = (Mem(::bof3::Img(at::kTarget))[0] & 0x40) != 0;
    const unsigned char i = Sprite_Current[3];
    if (enemies) {
        MH_CALL(Battle_SetTargetFlag40)(static_cast<unsigned char>(i + 3));
        MH_CALL(Sprite_ReleaseTint)(EnemyRecord(Sprite_Current[3]));
        MH_CALL(BattleActor_Flash)(static_cast<unsigned char>(Sprite_Current[3] + 3));
    } else {
        MH_CALL(Battle_SetTargetFlag40)(i);
        MH_CALL(Sprite_ReleaseTint)(PartyRecord(Sprite_Current[3]));
        MH_CALL(BattleActor_Flash)(Sprite_Current[3]);
    }
    BumpMagicS21(Sprite_Current[2]);
}

// original 0x4C66F0: the owner's +9 (its scorches) down; free.
MS21_EXPORT void __cdecl FxScorch_Free(void) {
    BumpMagicS21(Pointer(::bof3::Img(BattleE2_Owner_addr))[9], -1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C6700: jmp [0x65B614 + 4 * +1] - FlameColumn_Task,
// FlameSpark_Task.
MS21_EXPORT void __cdecl FlamePool_Dispatch(void) { CallCell(::bof3::Img(FlamePool_Types_addr), Sprite_Current[1]); }

// original 0x4C6720: the column's step through [0x65B61C + 4 * +2], its +0xB
// (the flicker) up; while it lives past its first step, the disc and the
// band under the actor's matrix.
MS21_EXPORT void __cdecl FlameColumn_Task(void) {
    CallCell(::bof3::Img(FlameColumn_Phases_addr), Sprite_Current[2]);
    BumpMagicS21(Sprite_Current[0xB]);
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(FlameColumn_DrawDisc)();
    MH_CALL(FlameColumn_DrawBand)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C6770: +0xA = (Rand & 3) << 4; the radius 0xC0 - +0xA; placed
// on its circle round the owner, its height the owner's; +0xB = Rand; +9 0;
// the step on.
MS21_EXPORT void __cdecl FlameColumn_Start(void) {
    const int r = MH_CALL(Rand)();
    Sprite_Current[0xA] = U8((r & 3) << 4);
    SetWord(ScrMagicS21(0), 0xC0u - Sprite_Current[0xA]);
    ColumnPlace();
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleE2_Owner_addr)))->height);
    const int r2 = MH_CALL(Rand)();
    Sprite_Current[0xB] = U8(r2);
    Sprite_Current[9] = 0;
    BumpMagicS21(Sprite_Current[2]);
}

// original 0x4C6840: the radius +9 * 8 + 0xC0 - +0xA, placed again; +9 up,
// at 0x10 the step on.
MS21_EXPORT void __cdecl FlameColumn_Grow(void) {
    const unsigned char* const sc = Sprite_Current;
    SetWord(ScrMagicS21(0), sc[9] * 8u + 0xC0u - sc[0xA]);
    ColumnPlace();
    BumpMagicS21(Sprite_Current[9]);
    unsigned char* const s2 = Sprite_Current;
    if (s2[9] == 0x10) BumpMagicS21(s2[2]);
}

// original 0x4C68F0: held until the owner's +0xB (its flames) is 8.
MS21_EXPORT void __cdecl FlameColumn_Hold(void) {
    if (Pointer(::bof3::Img(BattleE2_Owner_addr))[0xB] == 8) BumpMagicS21(Sprite_Current[2]);
}

// original 0x4C6910: +9 down; at 0 the owner's +0xB down and the slot freed.
MS21_EXPORT void __cdecl FlameColumn_Fade(void) {
    BumpMagicS21(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    BumpMagicS21(Pointer(::bof3::Img(BattleE2_Owner_addr))[0xB], -1);
    MH_AT(VoidFn, bof3::addr::MagicFx_FreeCurrentRecord)();
}

// original 0x4C6940: a fan of sixteen gouraud triangles round the origin of
// the actor's matrix, radius 0x100 + sin((+0xB & 0x1F) << 7) >> 8; the
// centre (+9 * 10, +9 * 9, +9 * 2), the rim (+9 * 10, +9 * 4, +9 * 2), every
// channel a byte; each linked at the column's position.
MS21_EXPORT void __cdecl FlameColumn_DrawDisc(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtTask(2, 0xC);
    const int s = MH_CALL(Math_Sin)((Sprite_Current[0xB] & 0x1F) << 7);
    const unsigned char b = Sprite_Current[9];
    SetWord(ScrMagicS21(0), static_cast<unsigned>((Mul(s, 16) >> 12) + 0x100));
    const unsigned char red = U8(b * 10), g0 = U8(b * 9), g1 = U8(b << 2), blue = U8(b << 1);
    SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), S16(ScrMagicS21(0))) >> 12));
    SetWord(Vtx(0x12), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(0), S16(ScrMagicS21(0))) >> 12));
    SetWord(Vtx(0x14), 0);
    SetWord(Vtx(0xC), 0);
    SetWord(Vtx(4), 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::uint16_t x = Word(Vtx(0x10)), y = Word(Vtx(0x12));
        SetWord(Vtx(0), 0);
        SetWord(Vtx(2), 0);
        SetWord(Vtx(8), x);
        SetWord(Vtx(0xA), y);
        SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), S16(ScrMagicS21(0))) >> 12));
        SetWord(Vtx(0x12), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a), S16(ScrMagicS21(0))) >> 12));
        long depth;
        MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), F(p, 8), F(p, 0x18), F(p, 0x28), &depth);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        p[5] = g0;
        p[4] = red;
        p[6] = blue;
        p[0x14] = red;
        p[0x15] = g1;
        p[0x16] = blue;
        p[0x24] = red;
        p[0x25] = g1;
        p[0x26] = blue;
        LinkAtTask(2, 0x34);
    }
}

// original 0x4C6B10: a band of sixteen gouraud quads round the origin, the
// inner radius 0x180 + sin(..) >> 8 at A0 / A8, the outer 0x100 + .. at
// B0 / B8; inner vertices (1, 1, 1), outer (+9 * 10, +9 * 4, +9 * 2).
MS21_EXPORT void __cdecl FlameColumn_DrawBand(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtTask(2, 0xC);
    const int s = MH_CALL(Math_Sin)((Sprite_Current[0xB] & 0x1F) << 7);
    const unsigned char flicker = Sprite_Current[0xB];
    SetWord(ScrMagicS21(0), static_cast<unsigned>((Mul(s, 16) >> 12) + 0x100));
    const int s2 = MH_CALL(Math_Sin)((flicker & 0x1F) << 7);
    const unsigned char b = Sprite_Current[9];
    SetWord(ScrMagicS21(2), static_cast<unsigned>((Mul(s2, 16) >> 12) + 0x180));
    const unsigned char red = U8(b * 10), blue = U8(b << 1), green = U8(b << 2);
    SetWord(Vtx(8), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), S16(ScrMagicS21(2))) >> 12));
    SetWord(Vtx(0xA), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(0), S16(ScrMagicS21(2))) >> 12));
    SetWord(Vtx(0x18), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), S16(ScrMagicS21(0))) >> 12));
    const int c0 = MH_CALL(Math_Cos)(0);
    SetWord(Vtx(0x1C), 0);
    SetWord(Vtx(0x1A), static_cast<unsigned>(Mul(c0, S16(ScrMagicS21(0))) >> 12));
    SetWord(Vtx(0x14), 0);
    SetWord(Vtx(0xC), 0);
    SetWord(Vtx(4), 0);
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = Word(Vtx(8)), y = Word(Vtx(0xA));
            SetWord(Vtx(0), x);
            SetWord(Vtx(2), y);
        }
        SetWord(Vtx(8), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), S16(ScrMagicS21(2))) >> 12));
        const int c = MH_CALL(Math_Cos)(a);
        const std::uint16_t ox = Word(Vtx(0x18));
        SetWord(Vtx(0xA), static_cast<unsigned>(Mul(c, S16(ScrMagicS21(2))) >> 12));
        const std::uint16_t oy = Word(Vtx(0x1A));
        SetWord(Vtx(0x10), ox);
        SetWord(Vtx(0x12), oy);
        SetWord(Vtx(0x18), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), S16(ScrMagicS21(0))) >> 12));
        SetWord(Vtx(0x1A), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a), S16(ScrMagicS21(0))) >> 12));
        long depth;
        MH_CALL(Gte_RotTransPers4)(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), F(p, 8), F(p, 0x18), F(p, 0x28), F(p, 0x38),
                                   &depth);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = red;
        p[0x25] = green;
        p[0x26] = blue;
        p[0x34] = red;
        p[0x35] = green;
        p[0x36] = blue;
        LinkAtTask(2, 0x44);
    }
}

// original 0x4C6D80: the spark's step through [0x65B62C + 4 * +2]; while it
// lives past its first step, its screen point and its quads.
MS21_EXPORT void __cdecl FlameSpark_Task(void) {
    CallCell(::bof3::Img(FlameSpark_Phases_addr), Sprite_Current[2]);
    const unsigned char* const sc = Sprite_Current;
    if (sc[0] == 0 || sc[2] == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(FlameSpark_Draw)();
}

// original 0x4C6DB0: +9 (the delay Inferno_Start gave it) down; at 0 the
// owner's position, moved 0x20 out in a random eighth ((Rand & 7) << 9);
// with +0xB odd a sound by (+0xB >> 1) % 3 - 0x101, 0x102, 0x103; +0xB 0x10,
// +9 0, +0xA 0x40, the step on.
MS21_EXPORT void __cdecl FlameSpark_Start(void) {
    BumpMagicS21(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
    if (sc[9] != 0) return;
    tsc->x = rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleE2_Owner_addr)))->x;
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleE2_Owner_addr)))->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Pointer(::bof3::Img(BattleE2_Owner_addr)))->height);
    SetWord(ScrMagicS21(0), 0x100);
    const int r = MH_CALL(Rand)();
    unsigned char* const x = Sprite_Current + 0x34;
    const int angle = static_cast<short>(static_cast<std::uint16_t>((r & 7) << 9));
    SetWord(ScrMagicS21(0xA), static_cast<unsigned>(angle));
    const int s = MH_CALL(Math_Sin)(angle);
    SetLong(x, Long(x) + (Mul(s, S16(ScrMagicS21(0))) >> 3));
    unsigned char* const z = Sprite_Current + 0x38;
    const int c = MH_CALL(Math_Cos)(S16(ScrMagicS21(0xA)));
    SetLong(z, Long(z) + (Mul(c, S16(ScrMagicS21(0))) >> 3));
    const unsigned char tone = Sprite_Current[0xB];
    if (tone & 1) {
        const unsigned which = (tone >> 1) % 3;
        MH_CALL(Sound_PlayById)(which == 0 ? 0x101 : which == 1 ? 0x102 : 0x103);
    }
    Sprite_Current[0xB] = 0x10;
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0x40;
    BumpMagicS21(Sprite_Current[2]);
}

// original 0x4C6ED0: +9 (the width) up by 0x20 to 0xC0.
MS21_EXPORT void __cdecl FlameSpark_Grow(void) {
    BumpMagicS21(Sprite_Current[9], 0x20);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0xC0) BumpMagicS21(sc[2]);
}

// original 0x4C6EF0: +9 up by 2, +0xA (the height) down to 0x30.
MS21_EXPORT void __cdecl FlameSpark_Stretch(void) {
    BumpMagicS21(Sprite_Current[9], 2);
    BumpMagicS21(Sprite_Current[0xA], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[0xA] == 0x30) BumpMagicS21(sc[2]);
}

// original 0x4C6F20: +9 up, +0xA and +0xB (the shade) down; at +0xB 0 the
// owner's +0xB down and the slot freed.
MS21_EXPORT void __cdecl FlameSpark_Fade(void) {
    BumpMagicS21(Sprite_Current[9]);
    BumpMagicS21(Sprite_Current[0xA], -1);
    BumpMagicS21(Sprite_Current[0xB], -1);
    if (Sprite_Current[0xB] != 0) return;
    BumpMagicS21(Pointer(::bof3::Img(BattleE2_Owner_addr))[0xB], -1);
    MH_AT(VoidFn, bof3::addr::MagicFx_FreeCurrentRecord)();
}

// original 0x4C6F70: two textured quads at the spark's screen point (+0x2E,
// +0x30), half-width +0xA, height +9, shade +0xB * 8: the flame (v 0x40 ..
// 0xFF of page (0x340, 0x100), clut (0, 0x1FA)) and its tip 16 above (v 0x30
// .. 0x40), the tip's lower two vertices (1, 1, 1). Scratch words 0x90385C
// (+9), 0x90385E (+0xA), 0x903856 / 0x903858 (the point), read again for
// every coordinate.
MS21_EXPORT void __cdecl FlameSpark_Draw(void) {
    const unsigned char* const sc = Sprite_Current;
    SetWord(ScrMagicS21(0xC), sc[9]);
    const unsigned char shade = U8(sc[0xB] << 3);
    SetWord(ScrMagicS21(0xE), sc[0xA]);
    SetWord(ScrMagicS21(6), static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_x));
    SetWord(ScrMagicS21(8), static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_y));
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    LinkAtTask(0, 0xC);
    auto X = [] { return static_cast<int>(S16(ScrMagicS21(6))); };
    auto Y = [] { return static_cast<int>(S16(ScrMagicS21(8))); };
    auto H = [] { return static_cast<int>(S16(ScrMagicS21(0xC))); };
    auto W = [] { return static_cast<int>(S16(ScrMagicS21(0xE))); };
    unsigned char* p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutF(p, 8, X() - W());
    PutF(p, 0xC, Y() - H());
    PutF(p, 0x1C, X() + W());
    PutF(p, 0x20, Y() - H());
    PutF(p, 0x30, X() - W());
    PutF(p, 0x34, Y());
    PutF(p, 0x44, X() + W());
    PutF(p, 0x48, Y());
    for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au, 0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = shade;
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    p[0x14] = 0x40;
    p[0x15] = 0x40;
    p[0x28] = 0xC0;
    p[0x29] = 0x40;
    p[0x3C] = 0x40;
    p[0x3D] = 0xFF;
    p[0x50] = 0xC0;
    p[0x51] = 0xFF;
    LinkAtTask(0, 0x54);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyGT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutF(p, 8, X() - W());
    PutF(p, 0xC, Y() - H() - 0x10);
    PutF(p, 0x1C, X() + W());
    PutF(p, 0x20, Y() - H() - 0x10);
    PutF(p, 0x30, X() - W());
    PutF(p, 0x34, Y() - H());
    PutF(p, 0x44, X() + W());
    PutF(p, 0x48, Y() - H());
    for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = 1;
    for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = shade;
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->tpage, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
    rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(p)->clut, MH_CALL(Gpu_GetClut)(0, 0x1FA));
    p[0x14] = 0x40;
    p[0x15] = 0x30;
    p[0x28] = 0xC0;
    p[0x29] = 0x30;
    p[0x3C] = 0x40;
    p[0x3D] = 0x40;
    p[0x50] = 0xC0;
    p[0x51] = 0x40;
    LinkAtTask(0, 0x54);
}

// original 0x4C72C0: the first
// pool slot without bit 0, marked and its index answered; 0xFF when all 32
// are in use. Only al is the original's answer; every caller keeps al.
MS21_EXPORT unsigned char __cdecl FlamePool_Alloc(void) {
    unsigned char i = 0;
    do {
        unsigned char* const s = Mem(::bof3::Img(FlamePool_addr) + i * at::kTaskStride);
        if ((s[0] & 1) == 0) {
            s[0] = static_cast<unsigned char>(s[0] | 1);
            return i;
        }
        ++i;
    } while (i < kPoolSlots);
    return 0xFF;
}

// original 0x4C7320: the task's position to the centre of the targets not
// out - the eight enemies with the target byte's bit 0x40, else the three
// party members: +0x34 / +0x38 the mean of (x >> 9) - 0x4000 put back
// (+ 0x4000) << 9, the word +0x3E the mean height (+0x3E), idiv each. With
// every one out the original divides by zero (a #DE, the game's end); ours
// aborts with a Fatal.
MS21_EXPORT void __cdecl Inferno_TargetCentre(void) {
    int n = 0, x = 0, z = 0, y = 0;
    const bool enemies = (Mem(::bof3::Img(at::kTarget))[0] & 0x40) != 0;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        const unsigned char* const r = enemies ? EnemyRecord(i) : PartyRecord(i);
        x += (Long(r + 0x34) >> 9) - 0x4000;
        y += S16(r + 0x3E);
        z += (Long(r + 0x38) >> 9) - 0x4000;
        ++n;
    }
    if (n == 0) bof3::Fatal("Inferno_TargetCentre: every target is out (the original divides by zero)");
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(x / n + 0x4000) << 9));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(z / n + 0x4000) << 9));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, static_cast<unsigned>(y / n));
}

void Magic092_Inject() {
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
    BOF3_INJECT(Inferno_Task);
    BOF3_INJECT(Inferno_Start);
    BOF3_INJECT(Inferno_End);
    BOF3_INJECT(FxScorch_Dispatch);
    BOF3_INJECT(FxScorch_Run);
    BOF3_INJECT(FxScorch_Delay);
    BOF3_INJECT(FxScorch_Hit);
    BOF3_INJECT(FxScorch_Free);
    BOF3_INJECT(FlamePool_Dispatch);
    BOF3_INJECT(FlameColumn_Task);
    BOF3_INJECT(FlameColumn_Start);
    BOF3_INJECT(FlameColumn_Grow);
    BOF3_INJECT(FlameColumn_Hold);
    BOF3_INJECT(FlameColumn_Fade);
    BOF3_INJECT(FlameColumn_DrawDisc);
    BOF3_INJECT(FlameColumn_DrawBand);
    BOF3_INJECT(FlameSpark_Task);
    BOF3_INJECT(FlameSpark_Start);
    BOF3_INJECT(FlameSpark_Grow);
    BOF3_INJECT(FlameSpark_Stretch);
    BOF3_INJECT(FlameSpark_Fade);
    BOF3_INJECT(FlameSpark_Draw);
    BOF3_INJECT(FlamePool_Alloc);
    BOF3_INJECT(Inferno_TargetCentre);
}
