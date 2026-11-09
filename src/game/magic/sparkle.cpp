// Moved here by tools/move_funcs.py from battle_items.cpp, magic_fx_reached.cpp, magic_s15.cpp, magic_s16.cpp (docs/source-layout-round.md).
#include "game/magic/sparkle.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_items_callees.h"
#include "game/cheats.h"
#include "game/magic_fx_reached_callees.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/sound.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

using namespace battle_items;

using namespace magic_fx_reached;

namespace battle_items {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_items

namespace {

// The sparkle being updated, and the battle actor the effect plays on - each
// read from its cell every time the original reads it.
unsigned char* CurBattleItems() { return battle_items::At(BattleItems_SparkleCurrent); }   // the slot's 32-bit word, a place

unsigned char* Actor() { return battle_items::At(BattleE2_Owner); }

// The scratch words: SharedScratch + k, and the vertex scratch + k.
unsigned char* Scr(unsigned k) { return battle_items::At(::bof3::Img(battle_items::at::kScratch.v) + k); }

short S16BattleItems(const unsigned char* at) { return static_cast<short>(battle_items::Word(at)); }

// `imul` then `sar 0xC`: a 32-bit product that wraps, shifted arithmetically.
int Mul12(int a, int b) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12;
}

// `fild dword` then `fstp dword` of an int: the original's integer vertex as
// a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// The sway both later phases share: the wave +0xC steps, x = base +
// sin((wave & 0x3F) << 6) * 24 / 4096 (the angle kept in scratch +6), y
// drifts by +0xA.
void Sway() {
    unsigned char* c = CurBattleItems();
    battle_items::SetWord(c + battle_items::at::kWave, battle_items::Word(c + battle_items::at::kWave) + 1);
    const int angle = (CurBattleItems()[battle_items::at::kWave] & 0x3F) << 6;
    battle_items::SetWord(Scr(6), static_cast<unsigned>(angle));
    const int s = battle_items::g.sin(angle);
    c = CurBattleItems();
    battle_items::SetWord(c + battle_items::at::kX, static_cast<unsigned>(Mul12(s, 24)) + battle_items::Word(c + battle_items::at::kBaseX));
    c = CurBattleItems();
    battle_items::SetWord(c + battle_items::at::kY, battle_items::Word(c + battle_items::at::kY) + battle_items::Word(c + battle_items::at::kDriftY));
}

// The three draws' common start: the sparkle's screen point to scratch +0 /
// +2 (the rays), the draw mode, and a sorted commit of it at the sparkle's
// position.
void DrawModeAtSparkle() {
    battle_items::g.set_draw_mode(Gfx_PacketNext, 0, 1, 0x35, 0);
    const unsigned char* const c = CurBattleItems();
    battle_items::g.commit_sorted(static_cast<unsigned>(battle_items::Long(c + 0x14)), static_cast<unsigned>(battle_items::Long(c + 0x18)), 2, 0xC);
}

void CommitAtSparkle(unsigned size) {
    const unsigned char* const c = CurBattleItems();
    battle_items::g.commit_sorted(static_cast<unsigned>(battle_items::Long(c + 0x14)), static_cast<unsigned>(battle_items::Long(c + 0x18)), 2, size);
}

// Scratch +8 / +0xA / +0xC: the brightness +5 times 6, three times.
void RayShade() {
    const unsigned char* const c = CurBattleItems();
    battle_items::SetWord(Scr(8), c[battle_items::at::kTimer] * 6u);
    battle_items::SetWord(Scr(0xA), c[battle_items::at::kTimer] * 6u);
    battle_items::SetWord(Scr(0xC), c[battle_items::at::kTimer] * 6u);
}

}  // namespace

namespace magic_fx_reached {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

namespace {

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace magic_fx_reached

namespace {

unsigned char* Ptr(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(magic_fx_reached::Long(magic_fx_reached::At(cell)))));
}

std::int32_t PtrValue(const unsigned char* p) {
    return static_cast<std::int32_t>(Low32(p));
}

unsigned char Byte(std::uint32_t address) { return magic_fx_reached::At(address)[0]; }

// Sprite_Current's position copied from `from`: the actor byte +8 and the
// three dwords +0x34 / +0x38 / +0x3C, Sprite_Current read again for each.
void TakePosition(const unsigned char* from) {
    Sprite_Current[8] = from[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(from)->height);
}

void Bump(unsigned char& b, int by) { b = static_cast<unsigned char>(b + by); }

}  // namespace

#define MFX_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC; words or dwords by
// function) and the four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF).
// Both are read again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned ActorIndex() { return static_cast<unsigned>(move_script::Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

unsigned TargetIndex() { return static_cast<unsigned>(move_script::Long(Mem(::bof3::Img(at::kTarget)))) & 0xFF; }

std::uint16_t SW(unsigned k) { return move_script::Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { move_script::SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3 (a party index lands below the enemy records).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

bool IsOut(unsigned actor) { return (MH_CALL(Battle_ActorIsOut)(actor & 0xFF) & 0xFFu) != 0; }

}  // namespace

#define S15_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// A marker (kind 1, 0x39, +1 2) for actor `index`: +0x80 the owner, +4 the
// markers so far, +0xB the actor; the owner's +0xB up.
void SpawnMark(unsigned index, unsigned& count) {
    const unsigned slot = NewTask(0x39);
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    unsigned char* const owner = Owner();
    tchild->owner = static_cast<std::int32_t>(Key(owner));
    child[1] = 2;
    child[4] = static_cast<unsigned char>(count);
    child[0xB] = static_cast<unsigned char>(index);
    ++count;
    Inc(owner[0xB]);
}

using magic_harness::Handler;

using move_script::SetLong;

constexpr std::uint32_t kSparkleStride = 0x2C;

constexpr ::bof3::image::Va kScratch{SharedScratch_addr}; // s16 words +0 .. +0xE (SharedScratch's neighbourhood)

// A sparkle's fields (battle_items_callees.h names them for MAGIC070's pool).
constexpr unsigned kFlags = 0, kType = 1, kPhase = 2, kShade = 3, kKind = 4, kTimer = 5, kCount = 6, kLimit = 7;

constexpr unsigned kBaseX = 8, kDriftY = 0xA, kWave = 0xC, kX = 0x20, kY = 0x22, kOwnerOf = 0x28;

using Fn0 = void (__cdecl*)();

using Draw = void (__cdecl*)(unsigned, unsigned);

using AllocFn = unsigned char (__cdecl*)();

// One overlay's copy of the sparkle code: where its pool, its current-sparkle
// cell and its tables are, and its own functions' addresses (the phases its
// update's stack table holds) and ours (what its calls reach).
struct Pool {
    std::uint32_t pool;          // count records of 0x2C
    unsigned count;
    std::uint32_t current;       // unsigned char *: the sparkle being updated
    std::uint32_t offsets;       // 32 x (s16 dx, s16 dy), by the launch row +7 & 0x1F
    std::uint32_t colours;       // rgb rows, row = shade +3 + 4 * kind +4
    std::uint32_t life;          // u8 by kind: the rise length
    std::uint32_t counts;        // u8 by kind: sparkles spawned
    std::uint32_t delays;        // u8 by kind: launch delay per four
    std::uint32_t types;         // the sparkle dispatch's table, read in place by +1
    std::uint32_t launch, rise, fade;   // the update's stack table (their original addresses)
    Fn0 disc;
    Draw rays_g2, rays_g3;
    AllocFn alloc;
    Fn0 free;
};

const Pool kPools[4] = {
    {::bof3::Img(0x685D98), 0x80, ::bof3::Img(0x687398), ::bof3::Img(0x65AE2C), ::bof3::Img(0x65AECC), ::bof3::Img(0x65AF18), ::bof3::Img(0x65AF08), ::bof3::Img(0x65AF10), ::bof3::Img(0x65AF20), bof3::addr::Magic071_SparkleLaunch, bof3::addr::Magic071_SparkleRise, bof3::addr::Magic071_SparkleFade,
     Magic071_SparkleDisc, Magic071_SparkleRaysG2, Magic071_SparkleRaysG3, Magic071_SparkleAlloc, Magic071_SparkleFree},
    {::bof3::Img(0x6873A0), 0x80, ::bof3::Img(0x6889A0), ::bof3::Img(0x65AF24), ::bof3::Img(0x65AFC4), ::bof3::Img(0x65B010), ::bof3::Img(0x65B000), ::bof3::Img(0x65B008), ::bof3::Img(0x65B018), bof3::addr::Magic072_SparkleLaunch, bof3::addr::Magic072_SparkleRise, bof3::addr::Magic072_SparkleFade,
     Magic072_SparkleDisc, Magic072_SparkleRaysG2, Magic072_SparkleRaysG3, Magic072_SparkleAlloc, Magic072_SparkleFree},
    {::bof3::Img(0x6889A8), 0xA0, ::bof3::Img(0x68A528), ::bof3::Img(0x65B01C), ::bof3::Img(0x65B0BC), ::bof3::Img(0x65B0DC), ::bof3::Img(0x65B0D4), ::bof3::Img(0x65B0D8), ::bof3::Img(0x65B0E4), bof3::addr::Magic073_SparkleLaunch, bof3::addr::Magic073_SparkleRise, bof3::addr::Magic073_SparkleFade,
     Magic073_SparkleDisc, Magic073_SparkleRaysG2, Magic073_SparkleRaysG3, Magic073_SparkleAlloc, Magic073_SparkleFree},
    {::bof3::Img(0x68A530), 0xA0, ::bof3::Img(0x68C0B0), ::bof3::Img(0x65B0E8), ::bof3::Img(0x65B188), ::bof3::Img(0x65B1A8), ::bof3::Img(0x65B1A0), ::bof3::Img(0x65B1A4), ::bof3::Img(0x65B1B0), bof3::addr::Magic074_SparkleLaunch, bof3::addr::Magic074_SparkleRise, bof3::addr::Magic074_SparkleFade,
     Magic074_SparkleDisc, Magic074_SparkleRaysG2, Magic074_SparkleRaysG3, Magic074_SparkleAlloc, Magic074_SparkleFree},
};

const Pool& k071 = kPools[0];

const Pool& k072 = kPools[1];

const Pool& k073 = kPools[2];

const Pool& k074 = kPools[3];

void BumpMagicS16(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }

std::int32_t PtrValueMagicS16(const void* p) { return static_cast<std::int32_t>(Low32(p)); }

unsigned char* Record(const Pool& p, unsigned index) { return Mem(p.pool + index * kSparkleStride); }

unsigned char* Cur(const Pool& p) { return Pointer(p.current); }

unsigned char* ScrMagicS16(unsigned k) { return Mem(::bof3::Img(kScratch.v) + k); }

short S16(const unsigned char* a) { return static_cast<short>(move_script::Word(a)); }

// `fild dword` then `fstp dword`: the integer as a float.
void PutFloatMagicS16(unsigned char* a, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(a, &f, sizeof f);
}

// A battle actor's record by index, as the originals compute it: a member
// (0x802D40, stride 0x14C) below 3, else the enemy index - 3 (0x93B960,
// stride 0x128), unchecked.
unsigned char* ActorRecord(unsigned char actor) {
    if (actor < 3) return Mem(::bof3::Img(ObjTrio_addr) + actor * at::kPartyStride);
    return Mem(::bof3::Img(Battle_Enemies_addr) + (static_cast<std::uint32_t>(actor) - 3) * at::kEnemyStride);
}

// original (e.g. 0x4B9B10): the sparkle's type +1 through the overlay's table,
// a jmp read in place, the type unchecked (the spawns store 0).
void Dispatch(const Pool& p) {
    const unsigned type = Cur(p)[kType];
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(move_script::Long(Mem(p.types + 4 * type)))))();
}

// original (e.g. 0x4B9B30): the sparkle's phase +2 through a three-entry
// stack table (launch, rise, fade; 3..255 would call through the stack, ours
// aborts); then while it lives (+0 bit 0) and is launched (+2 not 0) its
// disc, and when its sway +0xC & 3 is not 0 its rays: two-point ones of
// radius +6 at Frame_Counter / 2, three-point ones of radius +6 * 1.5 at
// Frame_Counter / 2 + 4 (pushed with stale upper halves; the draws read the
// low words).
void Update(const Pool& p, const char* who) {
    const unsigned phase = Cur(p)[kPhase];
    if (phase >= 3) bof3::Fatal("%s: phase %u, past the three-entry table", who, phase);
    const std::uint32_t table[3] = {p.launch, p.rise, p.fade};
    magic_harness::Phase(table[phase])();
    const unsigned char* c = Cur(p);
    if ((c[kFlags] & 1) == 0 || c[kPhase] == 0) return;
    MH_CALL(p.disc)();
    c = Cur(p);
    if ((c[kWave] & 3) == 0) return;
    MH_CALL(p.rays_g2)(Frame_Counter >> 1, c[kCount]);
    c = Cur(p);
    const unsigned radius = c[kCount];
    MH_CALL(p.rays_g3)((Frame_Counter >> 1) + 4, (radius >> 1) + radius);
}

// original (e.g. 0x4B9BC0): the launch. The delay +5 down; at 0 the owner's
// position (+0x34..+0x3C) and screen point (+0x2E / +0x30) to the sparkle;
// thrown by the offset row +7 & 0x1F (x by dx pushed 0..7 further from 0, y
// up by dy + 0..7), a base x, a drift Rand & 1, a sway phase Rand, a rise
// length (Rand & 6) + life[kind], brightness 0x10, count 0, phase on. The
// sparkle is read again after every call, the x pointer taken before its Rand.
void Launch(const Pool& p) {
    unsigned char* c = Cur(p);
    BumpMagicS16(c[kTimer], -1);
    c = Cur(p);
    if (c[kTimer] != 0) return;
    move_script::SetLong(c + 0x14, rv::As<records::SpriteHead>(Owner())->x);
    move_script::SetLong(Cur(p) + 0x18, rv::As<records::SpriteHead>(Owner())->z);
    move_script::SetLong(Cur(p) + 0x1C, rv::As<records::SpriteHead>(Owner())->height);
    move_script::SetWord(Cur(p) + kX, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_x));
    move_script::SetWord(Cur(p) + kY, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_y));
    c = Cur(p);
    unsigned char* const x = c + kX;
    if (S16(Mem(p.offsets + (c[kLimit] & 0x1F) * 4)) < 0) {
        const int r = MH_CALL(Rand)();
        const unsigned char* const c2 = Cur(p);
        move_script::SetWord(x, move_script::Word(x) + static_cast<std::uint16_t>(move_script::Word(Mem(p.offsets + (c2[kLimit] & 0x1F) * 4)) - (r & 7)));
    } else {
        const int r = MH_CALL(Rand)();
        const unsigned char* const c2 = Cur(p);
        move_script::SetWord(x, move_script::Word(x) + static_cast<std::uint16_t>((r & 7) + move_script::Word(Mem(p.offsets + (c2[kLimit] & 0x1F) * 4))));
    }
    unsigned char* const y = Cur(p) + kY;
    {
        const int r = MH_CALL(Rand)();
        const unsigned char* const c2 = Cur(p);
        move_script::SetWord(y, move_script::Word(y) - static_cast<std::uint16_t>((r & 7) + move_script::Word(Mem(p.offsets + 2 + (c2[kLimit] & 0x1F) * 4))));
    }
    c = Cur(p);
    move_script::SetWord(c + kBaseX, move_script::Word(c + kX));
    {
        const int r = MH_CALL(Rand)();
        move_script::SetWord(Cur(p) + kDriftY, static_cast<unsigned>(r & 1));
    }
    {
        const int r = MH_CALL(Rand)();
        move_script::SetWord(Cur(p) + kWave, static_cast<unsigned>(r));
    }
    {
        const int r = MH_CALL(Rand)();
        c = Cur(p);
        c[kLimit] = static_cast<unsigned char>((r & 6) + Mem(p.life)[c[kKind]]);
    }
    Cur(p)[kTimer] = 0x10;
    Cur(p)[kCount] = 0;
    BumpMagicS16(Cur(p)[kPhase]);
}

// The sway the rise and the fade share: +0xC up; x = base + sin((+0xC & 0x3F)
// << 6) * 24 >> 12 (the angle kept in scratch +6); y += the drift +0xA.
void SwayMagicS16(const Pool& p) {
    unsigned char* c = Cur(p);
    move_script::SetWord(c + kWave, move_script::Word(c + kWave) + 1);
    const int angle = (Cur(p)[kWave] & 0x3F) << 6;
    move_script::SetWord(ScrMagicS16(6), static_cast<unsigned>(angle));
    const int s = MH_CALL(Math_Sin)(angle);
    c = Cur(p);
    move_script::SetWord(c + kX, static_cast<unsigned>(Mul12(s, 24)) + move_script::Word(c + kBaseX));
    c = Cur(p);
    move_script::SetWord(c + kY, move_script::Word(c + kY) + move_script::Word(c + kDriftY));
}

// original (e.g. 0x4B9D30): the rise. The sway; the count +6 up; at the rise
// length +7 the phase on.
void Rise(const Pool& p) {
    SwayMagicS16(p);
    BumpMagicS16(Cur(p)[kCount]);
    unsigned char* const c = Cur(p);
    if (c[kCount] == c[kLimit]) BumpMagicS16(c[kPhase]);
}

// original (e.g. 0x4B9DA0): the fade. The sway; the count down on every
// fourth frame; the brightness +5 down, and at 0 the owner's sparkle count
// +0xB down and the sparkle freed (a tail jmp in the original).
void Fade(const Pool& p) {
    SwayMagicS16(p);
    if ((Frame_Counter & 3) == 0) BumpMagicS16(Cur(p)[kCount], -1);
    BumpMagicS16(Cur(p)[kTimer], -1);
    if (Cur(p)[kTimer] != 0) return;
    BumpMagicS16(Owner()[0xB], -1);
    MH_CALL(p.free)();
}

// The draws' common start: the draw mode at Gfx_PacketNext and a sorted
// commit at the sparkle's position; then each primitive's commit.
void DrawMode(const Pool& p) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    const unsigned char* const c = Cur(p);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(move_script::Long(c + 0x14)), static_cast<unsigned long>(move_script::Long(c + 0x18)), 2, 0xC);
}

void Commit(const Pool& p) {
    const unsigned char* const c = Cur(p);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(move_script::Long(c + 0x14)), static_cast<unsigned long>(move_script::Long(c + 0x18)), 2, 0x34);
}

// Scratch +0 / +2 the sparkle's screen point (read before the draw mode).
void PointToScratch(const Pool& p) {
    const unsigned char* const c = Cur(p);
    move_script::SetWord(ScrMagicS16(0), move_script::Word(c + kX));
    move_script::SetWord(ScrMagicS16(2), move_script::Word(c + kY));
}

// Scratch +8 / +0xA / +0xC: the brightness +5 times 6.
void RayShadeMagicS16(const Pool& p) {
    const unsigned char* const c = Cur(p);
    move_script::SetWord(ScrMagicS16(8), c[kTimer] * 6u);
    move_script::SetWord(ScrMagicS16(0xA), c[kTimer] * 6u);
    move_script::SetWord(ScrMagicS16(0xC), c[kTimer] * 6u);
}

// original (e.g. 0x4B9E30): four gouraud lines from the sparkle's point,
// `radius` long, at ((start + 8k) & 0x1F) << 7, bright (+5 * 6) at the centre,
// dark at the tip. Both arguments are read as their low words.
void RaysG2(const Pool& p, unsigned start, unsigned radius) {
    PointToScratch(p);
    DrawMode(p);
    RayShadeMagicS16(p);
    int n = static_cast<int>(start & 0xFFFF);
    const int end = n + 0x20;
    const int r = static_cast<int>(radius & 0xFFFF);
    do {
        unsigned char* const q = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(q);
        MH_CALL(Gpu_SetSemiTrans)(q, 1);
        PutFloatMagicS16(q + 8, S16(ScrMagicS16(0)));
        move_script::SetWord(ScrMagicS16(6), static_cast<unsigned>((n & 0x1F) << 7));
        PutFloatMagicS16(q + 0xC, S16(ScrMagicS16(2)));
        const int co = MH_CALL(Math_Cos)(S16(ScrMagicS16(6)));
        PutFloatMagicS16(q + 0x18, Mul12(co, r) + S16(ScrMagicS16(0)));
        const int si = MH_CALL(Math_Sin)(S16(ScrMagicS16(6)));
        PutFloatMagicS16(q + 0x1C, Mul12(si, r) + S16(ScrMagicS16(2)));
        q[4] = ScrMagicS16(8)[0];
        q[5] = ScrMagicS16(0xA)[0];
        q[6] = ScrMagicS16(0xC)[0];
        q[0x14] = 1;
        q[0x15] = 1;
        q[0x16] = 1;
        Commit(p);
        n += 8;
    } while (n < end);
}

// original (e.g. 0x4B9FC0): the same four rays as three-point lines - dark at
// the centre, bright (+5 * 6) at half the radius, dark at the tip.
void RaysG3(const Pool& p, unsigned start, unsigned radius) {
    PointToScratch(p);
    DrawMode(p);
    RayShadeMagicS16(p);
    int n = static_cast<int>(start & 0xFFFF);
    const int end = n + 0x20;
    const int full = static_cast<int>(radius & 0xFFFF);
    const int half = full >> 1;
    do {
        unsigned char* const q = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG3)(q);
        MH_CALL(Gpu_SetSemiTrans)(q, 1);
        PutFloatMagicS16(q + 8, S16(ScrMagicS16(0)));
        move_script::SetWord(ScrMagicS16(6), static_cast<unsigned>((n & 0x1F) << 7));
        PutFloatMagicS16(q + 0xC, S16(ScrMagicS16(2)));
        const int co = MH_CALL(Math_Cos)(S16(ScrMagicS16(6)));
        PutFloatMagicS16(q + 0x18, Mul12(co, half) + S16(ScrMagicS16(0)));
        const int si = MH_CALL(Math_Sin)(S16(ScrMagicS16(6)));
        PutFloatMagicS16(q + 0x1C, Mul12(si, half) + S16(ScrMagicS16(2)));
        const int co2 = MH_CALL(Math_Cos)(S16(ScrMagicS16(6)));
        PutFloatMagicS16(q + 0x28, Mul12(co2, full) + S16(ScrMagicS16(0)));
        const int si2 = MH_CALL(Math_Sin)(S16(ScrMagicS16(6)));
        const int y2 = Mul12(si2, full) + S16(ScrMagicS16(2));
        q[4] = 1;
        q[5] = 1;
        q[6] = 1;
        PutFloatMagicS16(q + 0x2C, y2);
        q[0x14] = ScrMagicS16(8)[0];
        q[0x15] = ScrMagicS16(0xA)[0];
        q[0x16] = ScrMagicS16(0xC)[0];
        q[0x24] = 1;
        q[0x25] = 1;
        q[0x26] = 1;
        Commit(p);
        n += 8;
    } while (n < end);
}

// original (e.g. 0x4BA1B0): the sparkle's body - eight gouraud triangles
// round its point, radius +6 + (Rand & 3), the centre the colour row (+3 +
// 4 * +4) times the brightness +5, the rim +5 grey.
void Disc(const Pool& p) {
    DrawMode(p);
    const int r = MH_CALL(Rand)();
    const unsigned char* const c = Cur(p);
    move_script::SetWord(ScrMagicS16(4), static_cast<unsigned>((r & 3) + c[kCount]));
    move_script::SetWord(ScrMagicS16(0), move_script::Word(c + kX));
    move_script::SetWord(ScrMagicS16(2), move_script::Word(c + kY));
    const unsigned char* const rgb = Mem(p.colours + (c[kShade] + c[kKind] * 4u) * 3u);
    move_script::SetWord(ScrMagicS16(8), rgb[0] * static_cast<unsigned>(c[kTimer]));
    move_script::SetWord(ScrMagicS16(0xA), rgb[1] * static_cast<unsigned>(c[kTimer]));
    move_script::SetWord(ScrMagicS16(0xC), rgb[2] * static_cast<unsigned>(c[kTimer]));
    move_script::SetWord(ScrMagicS16(0xE), c[kTimer]);
    int a = 0;
    do {
        unsigned char* const q = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(q);
        MH_CALL(Gpu_SetSemiTrans)(q, 1);
        PutFloatMagicS16(q + 8, S16(ScrMagicS16(0)));
        PutFloatMagicS16(q + 0xC, S16(ScrMagicS16(2)));
        int s = MH_CALL(Math_Sin)(a);
        PutFloatMagicS16(q + 0x18, Mul12(s, S16(ScrMagicS16(4))) + S16(ScrMagicS16(0)));
        int co = MH_CALL(Math_Cos)(a);
        PutFloatMagicS16(q + 0x1C, Mul12(co, S16(ScrMagicS16(4))) + S16(ScrMagicS16(2)));
        a += 0x200;
        s = MH_CALL(Math_Sin)(a);
        PutFloatMagicS16(q + 0x28, Mul12(s, S16(ScrMagicS16(4))) + S16(ScrMagicS16(0)));
        co = MH_CALL(Math_Cos)(a);
        PutFloatMagicS16(q + 0x2C, Mul12(co, S16(ScrMagicS16(4))) + S16(ScrMagicS16(2)));
        q[4] = ScrMagicS16(8)[0];
        q[5] = ScrMagicS16(0xA)[0];
        q[6] = ScrMagicS16(0xC)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) q[k] = ScrMagicS16(0xE)[0];
        Commit(p);
    } while (a < 0x1000);
}

// original (e.g. 0x4BA3E0): the first record of the pool without bit 0,
// marked and its index answered; 0xFF when all are in use. Only al is the
// original's answer; every caller keeps al.
unsigned char Alloc(const Pool& p) {
    unsigned char i = 0;
    do {
        unsigned char* const s = Record(p, i);
        if ((s[kFlags] & 1) == 0) {
            s[kFlags] = static_cast<unsigned char>(s[kFlags] | 1);
            return i;
        }
        ++i;
    } while (i < p.count);
    return 0xFF;
}

// original (e.g. 0x4BA430): bytes +0..+4 of the sparkle cleared, the pointer
// read again for each.
void Free(const Pool& p) {
    for (unsigned k = 0; k < 5; ++k) Cur(p)[k] = 0;
}

// The pool walk that follows each task's phase: every record in use (bit 0)
// becomes the current sparkle, its +0x28 the owner 0x93B940 for the dispatch,
// and the owner is put back after each (saved once, after the phase).
void Walk(const Pool& p, Fn0 dispatch) {
    const std::int32_t saved = move_script::Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < p.count; ++i) {
        unsigned char* const s = Record(p, i);
        if ((s[kFlags] & 1) == 0) continue;
        move_script::SetLong(Mem(p.current), PtrValueMagicS16(s));
        move_script::SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), move_script::Long(s + kOwnerOf));
        MH_CALL(dispatch)();
        move_script::SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), saved);
    }
}

// The phase +1 through a stack table of `n` (unchecked by the original).
void Step(const std::uint32_t* table, unsigned n, const char* who) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(table[phase])();
}

// A child's phase +2 through its six-entry stack table (unchecked by the
// original).
void Step2(const std::uint32_t* table, const char* who) {
    const unsigned phase = Sprite_Current[2];
    if (phase >= 6) bof3::Fatal("%s: phase %u, past the six-entry table", who, phase);
    magic_harness::Phase(table[phase])();
}

// The sparkles of one spawn: for n from 0 while n < counts[kind], a record
// from the pool (0xFF skips one) owned by Sprite_Current, type and phase 0,
// shade Rand & 3, the kind, offset row n, delay delays[kind] * (n >> 2) + 1
// (a byte), counted in Sprite_Current +0xB. `kind` reads the kind again where
// the original does (the task's own +4, or the owner's).
template <typename Kind> void MakeSparkles(const Pool& p, Kind kind) {
    unsigned char n = 0;
    if (Mem(p.counts)[kind()] == 0) return;
    do {
        const unsigned char a = MH_CALL(p.alloc)();
        if (a != 0xFF) {
            unsigned char* const s = Record(p, a);
            move_script::SetLong(s + kOwnerOf, PtrValueMagicS16(Sprite_Current));
            s[kType] = 0;
            s[kPhase] = 0;
            const int r = MH_CALL(Rand)();
            s[kShade] = static_cast<unsigned char>(r & 3);
            const unsigned k = kind();
            s[kKind] = static_cast<unsigned char>(k);
            s[kLimit] = n;
            s[kTimer] = static_cast<unsigned char>(Mem(p.delays)[k] * static_cast<unsigned>(n >> 2) + 1);
            BumpMagicS16(Sprite_Current[0xB]);
        }
        ++n;
    } while (n < Mem(p.counts)[kind()]);
}

void ClearPool(const Pool& p) {
    for (unsigned i = 0; i < p.count; ++i) {
        unsigned char* const s = Record(p, i);
        s[0] = 0;
        s[1] = 0;
        s[2] = 0;
    }
}

// original 0x4B99C0 / 0x4BA4F0 (MAGIC070's Sparkle_Spawn with its kind): the
// pool's +0..+2 cleared; the task's kind +4; the source sprite's (0x904B4C,
// read once) +8 and position to the task; its screen point; +0xB 0, +9 = 8,
// +0xA 0, the phase on; the sparkles (the kind the task's +4, read again);
// sound 0x100.
void Spawn(const Pool& p, unsigned char kind) {
    ClearPool(p);
    Sprite_Current[4] = kind;
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    Sprite_Current[8] = src[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 8;
    Sprite_Current[0xA] = 0;
    BumpMagicS16(Sprite_Current[1]);
    MakeSparkles(p, [] { return static_cast<unsigned>(Sprite_Current[4]); });
    MH_CALL(Sound_PlayEffect)(0x100);
}

// original 0x4BB000 / 0x4BBDD0: the pool's +0..+2 cleared; the task's kind
// +4; +0xB 0; the phase on. Then for every actor of the target's side - the
// enemies 3..10 when the target 0x904B44 has bit 0x40, else the party 0..2 -
// that is not out, a child task (kind 1, `parameter`): its owner the task,
// phase 0, its actor +4, its delay +9 (1, then 0x1C more for each child), and
// the task's +0xB counts it. Sound 0x100 by id.
void SpawnOnActors(const Pool& p, unsigned char kind, unsigned parameter) {
    ClearPool(p);
    Sprite_Current[4] = kind;
    Sprite_Current[0xB] = 0;
    BumpMagicS16(Sprite_Current[1]);
    const bool enemies = (Mem(::bof3::Img(at::kTarget))[0] & 0x40) != 0;
    unsigned char delay = 1;
    unsigned char i = enemies ? 3 : 0;
    do {
        if (MH_CALL(Battle_ActorIsOut)(i) == 0) {
            const unsigned char slot = MH_CALL(BattleTask_Create)(1, parameter);
            unsigned char* const t = Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride);
            unsigned char* const sc = Sprite_Current;
            move_script::SetLong(t + 0x80, PtrValueMagicS16(sc));
            t[1] = 0;
            t[4] = i;
            t[9] = delay;
            delay = static_cast<unsigned char>(delay + 0x1C);
            BumpMagicS16(sc[0xB]);
        }
        ++i;
    } while (enemies ? static_cast<unsigned char>(i - 3) < 8 : i < 3);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4BB210 / 0x4BBFB0: the child's start. Its delay +9 down; at 0 it
// takes its actor's (+4) byte +8 and position, the actor's screen point
// (BattleActor_UpdateScreenXY), +0xB 0, +9 = 8, +0xA 0, its phase +2 on; then
// the sparkles, their kind the parent's (the owner 0x93B940) +4.
void ActorStart(const Pool& p) {
    BumpMagicS16(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] != 0) return;
    const unsigned char* const rec = ActorRecord(sc[4]);
    sc[8] = rec[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(rec)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(rec)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(rec)->height);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 8;
    Sprite_Current[0xA] = 0;
    BumpMagicS16(Sprite_Current[2]);
    MakeSparkles(p, [] { return static_cast<unsigned>(Owner()[4]); });
}

}  // namespace

#define MS16_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The task's six phases: its spawn, then the shared tint, brighten and
// wait (round eight's BattleFx_*), MAGIC070's Sparkle_End, and the finish.
constexpr std::uint32_t kTask071[6] = {bof3::addr::Magic071_Spawn, bof3::addr::BattleFx_TintActor, bof3::addr::BattleFx_Brighten,
                                       bof3::addr::BattleFx_WaitStep4, bof3::addr::Sparkle_End, bof3::addr::BattleFx_Finish};

constexpr std::uint32_t kTask072[6] = {bof3::addr::Magic072_Spawn, bof3::addr::BattleFx_TintActor, bof3::addr::BattleFx_Brighten,
                                       bof3::addr::BattleFx_WaitStep4, bof3::addr::Sparkle_End, bof3::addr::BattleFx_Finish};

constexpr std::uint32_t kTask073[2] = {bof3::addr::Magic073_Spawn, bof3::addr::Magic073_Wait};

// MAGIC131's wait-and-end, group S30's MagicFx_EndWhenChildrenDone (0x4E5200).
constexpr std::uint32_t kTask074[2] = {bof3::addr::Magic074_Spawn, bof3::addr::MagicFx_EndWhenChildrenDone};

// The child's six phases: its start, the actor's tint, MAGIC213's brighten
// (group C1's ActorFx_TintUp, 0x4F4DA0), the wait for four sparkles, the fade,
// the end.
constexpr std::uint32_t kActor073[6] = {bof3::addr::Magic073_ActorStart, bof3::addr::ActorFx_Tint, bof3::addr::ActorFx_TintUp, bof3::addr::ActorFx_WaitStep4, bof3::addr::ActorFx_Untint, bof3::addr::ActorFx_End};

constexpr std::uint32_t kActor074[6] = {bof3::addr::Magic074_ActorStart, bof3::addr::ActorFx_Tint, bof3::addr::ActorFx_TintUp, bof3::addr::ActorFx_WaitStep4, bof3::addr::ActorFx_Untint, bof3::addr::ActorFx_End};

void CallCell(std::uint32_t table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(move_script::Long(Mem(table + 4 * index)))))();
}

}  // namespace

#define MS16_SPARKLES(N, P)                                                                                        \
    MS16_EXPORT void __cdecl Magic##N##_SparkleDispatch(void) { Dispatch(P); }                                     \
    MS16_EXPORT void __cdecl Magic##N##_SparkleUpdate(void) { Update(P, "Magic" #N "_SparkleUpdate"); }            \
    MS16_EXPORT void __cdecl Magic##N##_SparkleLaunch(void) { Launch(P); }                                         \
    MS16_EXPORT void __cdecl Magic##N##_SparkleRise(void) { Rise(P); }                                             \
    MS16_EXPORT void __cdecl Magic##N##_SparkleFade(void) { Fade(P); }                                             \
    MS16_EXPORT void __cdecl Magic##N##_SparkleRaysG2(unsigned start, unsigned radius) { RaysG2(P, start, radius); } \
    MS16_EXPORT void __cdecl Magic##N##_SparkleRaysG3(unsigned start, unsigned radius) { RaysG3(P, start, radius); } \
    MS16_EXPORT void __cdecl Magic##N##_SparkleDisc(void) { Disc(P); }                                             \
    MS16_EXPORT unsigned char __cdecl Magic##N##_SparkleAlloc(void) { return Alloc(P); }                           \
    MS16_EXPORT void __cdecl Magic##N##_SparkleFree(void) { Free(P); }

// original 0x4B8A90: a cross at the screen point - two semi-transparent flat
// lines 8 long (layer 3), horizontal then vertical, shaded 0x80.
S15_EXPORT void __cdecl Influence_DrawCross(void) {
    const unsigned char* const s = Sc();
    SetSW(6, 0x80);
    unsigned char* p = Gfx_PacketNext;
    SetSW(0xC, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(0xE, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    MH_CALL(Gpu_SetLineF2)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, SS(0xC) - 4);
    PutFloat(p + 0xC, SS(0xE));
    PutFloat(p + 0x14, SS(0xC) + 4);
    PutFloat(p + 0x18, SS(0xE));
    p[4] = SB(6);
    p[5] = SB(6);
    p[6] = SB(6);
    MH_CALL(Gfx_CommitPrim)(3, 0x20);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetLineF2)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    PutFloat(p + 8, SS(0xC));
    PutFloat(p + 0xC, SS(0xE) + 4);
    PutFloat(p + 0x14, SS(0xC));
    PutFloat(p + 0x18, SS(0xE) - 4);
    p[4] = SB(6);
    p[5] = SB(6);
    p[6] = SB(6);
    MH_CALL(Gfx_CommitPrim)(3, 0x20);
}

// original 0x4B8BD0: a marker for every enemy, then every member, that is not
// out, not the acting actor and not the target, and whose record passes: an
// enemy's word +0xBA at most 1, or its +0x92 bit 0x20, or its dword +0x114 bit
// 0x4000; a member's word +0xAA at most 1, or +0x90 bit 0x20, or its dword
// +0x134 bit 0x4000 or bit 0. BattleTask_Create's 0xFF is not tested.
S15_EXPORT void __cdecl Influence_SpawnMarks(void) {
    unsigned count = 0;
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned index = i + 3;
        if (IsOut(index)) continue;
        if (ActorIndex() == index) continue;
        if (TargetIndex() == index) continue;
        const unsigned char* const e = EnemyRecord(index);
        const records::BattleEnemy* const te = rv::AsConst<records::BattleEnemy>(e);
        if (te->intel > 1 && (e[0x92] & 0x20) == 0 && (static_cast<std::int32_t>(te->flags_114) & 0x4000) == 0) continue;
        SpawnMark(index, count);
    }
    for (unsigned i = 0; i < 3; ++i) {
        if (IsOut(i)) continue;
        if (ActorIndex() == i) continue;
        if (TargetIndex() == i) continue;
        const unsigned char* const r = PartyRecord(i);
        const records::PartyMember* const tr = rv::AsConst<records::PartyMember>(r);
        const std::uint32_t bits = static_cast<std::uint32_t>(move_script::Long(r + 0x134));
        if (tr->intel > 1 && (r[0x90] & 0x20) == 0 && (bits & 0x4000) == 0 && (bits & 1) == 0) continue;
        SpawnMark(i, count);
    }
}

// original 0x4B8D70 (PSX MAGIC070's entry): the task's phase +1 through a
// six-entry table on the original's stack (Sparkle_Spawn, 0x4B1E70,
// 0x4B1ED0, 0x4EE8A0, Sparkle_End, 0x4F7350; 6..255 would call through that
// stack, ours aborts); then every sparkle in use (bit 0 of +0) becomes the
// current one 0x685D90 with its +0x28 as 0x93B940 for Sparkle_Dispatch, and
// 0x93B940 is put back after each. The pool is walked by its own pointer;
// 0x93B940 is saved once, after the phase.
MFX_EXPORT void __cdecl Sparkle_Task(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 6) bof3::Fatal("Sparkle_Task: phase %u, past the six-entry table", phase);
    magic_fx_reached::g.sparkle_task[phase]();
    const std::int32_t saved = static_cast<std::int32_t>(BattleE2_Owner);
    for (unsigned i = 0; i < magic_fx_reached::at::kSparkles; ++i) {
        unsigned char* const s = magic_fx_reached::At(::bof3::Img(magic_fx_reached::at::kSparklePool) + i * magic_fx_reached::at::kSparkleStride);
        if ((s[0] & 1) == 0) continue;
        BattleItems_SparkleCurrent = PtrValue(s);
        BattleE2_Owner = magic_fx_reached::Long(s + 0x28);
        magic_fx_reached::g.sparkle_dispatch();
        BattleE2_Owner = saved;
    }
}

// original 0x4B8E00 (PSX MAGIC070's): the spawn. Bytes +0..+2 of all 128
// sparkles cleared; the task's kind +4 to 0; the source sprite's position;
// its screen point; +0xB 0, +9 = 8, +0xA 0, the phase on; then for each of
// Sparkle_CountByKind[kind] a sparkle: owned by the task, type and phase 0,
// colour row Rand & 3, the task's kind, offset row n, delay
// Sparkle_DelayByKind[kind] * (n >> 2) + 1 (a byte), counted in +0xB - a
// full pool skips one; sound 0x100.
//
// As the original has it: the kind is read again from the task each time
// (0 as it has just set it, unless a callee moves it); Sprite_Current is
// read again after each call; the loop's bound is re-read each round.
MFX_EXPORT void __cdecl Sparkle_Spawn(void) {
    for (unsigned i = 0; i < magic_fx_reached::at::kSparkles; ++i) {
        unsigned char* const s = magic_fx_reached::At(::bof3::Img(magic_fx_reached::at::kSparklePool) + i * magic_fx_reached::at::kSparkleStride);
        s[0] = 0;
        s[1] = 0;
        s[2] = 0;
    }
    Sprite_Current[4] = 0;
    TakePosition(Ptr(::bof3::Img(magic_fx_reached::at::kSource)));
    magic_fx_reached::g.update_screen_xy();
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 8;
    Sprite_Current[0xA] = 0;
    Bump(Sprite_Current[1], 1);
    unsigned char n = 0;
    if (Sparkle_CountByKind[Sprite_Current[4]] != 0) {
        do {
            const unsigned char a = magic_fx_reached::g.sparkle_alloc();
            if (a != 0xFF) {
                unsigned char* const s = magic_fx_reached::At(::bof3::Img(magic_fx_reached::at::kSparklePool) + a * magic_fx_reached::at::kSparkleStride);
                magic_fx_reached::SetLong(s + 0x28, PtrValue(Sprite_Current));
                s[1] = 0;
                s[2] = 0;
                const int r = magic_fx_reached::g.rand();
                unsigned char* const sc = Sprite_Current;
                s[3] = static_cast<unsigned char>(r & 3);
                s[4] = sc[4];
                s[7] = n;
                s[5] = static_cast<unsigned char>(Sparkle_DelayByKind[sc[4]] * static_cast<unsigned>(n >> 2) + 1);
                Bump(sc[0xB], 1);
            }
            ++n;
        } while (n < Sparkle_CountByKind[Sprite_Current[4]]);
    }
    magic_fx_reached::g.play_effect(0x100);
}

// original 0x4B8F50 (PSX MAGIC070's): the end of the glow. The tint record
// +0xA's three colour bytes (+2..+4 of MoveScript_TintRecords' 12-byte
// record) down one; +9 down, and at 0 the source's tints released, the
// target flashed and the phase on.
MFX_EXPORT void __cdecl Sparkle_End(void) {
    unsigned char* const sc = Sprite_Current;
    for (unsigned k = 2; k <= 4; ++k) {
        unsigned char* const tint = magic_fx_reached::At(::bof3::Img(MoveScript_TintRecords_addr) + sc[0xA] * 12u + k);
        Bump(*tint, -1);
    }
    Bump(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    magic_fx_reached::g.release_tint(Ptr(::bof3::Img(magic_fx_reached::at::kSource)));
    magic_fx_reached::g.flash(Byte(::bof3::Img(magic_fx_reached::at::kTarget)));
    Bump(Sprite_Current[1], 1);
}

// original 0x4B8FE0: the type byte +1 through Sparkle_Types, a jmp. As the
// original has it: the table's one real entry is type 0 (0x4B9000), the words
// after it are data, and nothing checks the type - 0x4B8D70 sets it to 0.
extern "C" void __cdecl Sparkle_Dispatch(void) {
    const unsigned type = CurBattleItems()[battle_items::at::kType];
    reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(battle_items::Long(battle_items::At(::bof3::Img(battle_items::at::kSparkleTypes.v) + 4 * type))))();
}

// original 0x4B9000 (PSX MAGIC070's): the sparkle's type 0 (Sparkle_Types'
// one entry). Its phase +2 through a three-entry table on the original's
// stack (Sparkle_Launch, _Rise, _Fade); then while it lives (+0 bit 0) and
// is launched (+2 not 0) its disc, and on frames where its sway +0xC and 3 is
// not 0 its rays: two-point ones of radius +6 at Frame_Counter / 2, then
// three-point ones of radius +6 * 1.5 at Frame_Counter / 2 + 4.
//
// The phase is not checked by the original: 3 calls the dword above the
// table - its own return address into Sparkle_Task - and on up the caller's
// stack. Phases stay 0..2 (Launch and Rise step them, Sparkle_Free clears
// them); ours aborts past 2 (docs/battle_items.md section 3 left it Capcom's
// for this; the section 3 of this group's doc says why it is taken now).
// As the original has it: the rays' arguments are pushed with stale upper
// halves (their callees read the low words).
MFX_EXPORT void __cdecl Sparkle_Update(void) {
    const unsigned phase = Ptr(::bof3::Img(magic_fx_reached::at::kSparkleCurrent))[2];
    if (phase >= 3) bof3::Fatal("Sparkle_Update: phase %u, past the three-entry table", phase);
    magic_fx_reached::g.sparkle[phase]();
    const unsigned char* c = Ptr(::bof3::Img(magic_fx_reached::at::kSparkleCurrent));
    if ((c[0] & 1) == 0 || c[2] == 0) return;
    magic_fx_reached::g.sparkle_disc();
    c = Ptr(::bof3::Img(magic_fx_reached::at::kSparkleCurrent));
    if ((c[0xC] & 3) == 0) return;
    magic_fx_reached::g.rays_g2(Frame_Counter >> 1, c[6]);
    c = Ptr(::bof3::Img(magic_fx_reached::at::kSparkleCurrent));
    const unsigned radius = c[6];
    magic_fx_reached::g.rays_g3((Frame_Counter >> 1) + 4, (radius >> 1) + radius);
}

// original 0x4B9090 (PSX MAGIC070 0x801EF47C's slot): the launch. The delay +5
// counts down; at 0 the sparkle takes the actor's position (+0x34..+0x3C) and
// screen point (+0x2E / +0x30), is thrown by the offset row +7 (x by the
// row's dx pushed 0..7 further from 0, y up by dy + 0..7), gets a random sway
// and drift, a rise length from its kind, brightness 0x10 and phase 1.
// The x pointer is taken before the Rand, the sparkle read again after each
// call, as the original does.
extern "C" void __cdecl Sparkle_Launch(void) {
    unsigned char* c = CurBattleItems();
    c[battle_items::at::kTimer] = static_cast<unsigned char>(c[battle_items::at::kTimer] - 1);
    c = CurBattleItems();
    if (c[battle_items::at::kTimer] != 0) return;
    battle_items::SetLong(c + 0x14, battle_items::Long(Actor() + 0x34));
    battle_items::SetLong(CurBattleItems() + 0x18, battle_items::Long(Actor() + 0x38));
    battle_items::SetLong(CurBattleItems() + 0x1C, battle_items::Long(Actor() + 0x3C));
    battle_items::SetWord(CurBattleItems() + battle_items::at::kX, battle_items::Word(Actor() + 0x2E));
    battle_items::SetWord(CurBattleItems() + battle_items::at::kY, battle_items::Word(Actor() + 0x30));
    c = CurBattleItems();
    unsigned char* const x = c + battle_items::at::kX;
    if (S16BattleItems(battle_items::At(::bof3::Img(battle_items::at::kSparkleOffsets.v) + (c[battle_items::at::kLimit] & 0x1F) * 4)) < 0) {
        const int r = battle_items::g.rand();
        const unsigned char* const c2 = CurBattleItems();
        const auto dx = static_cast<std::uint16_t>(battle_items::Word(battle_items::At(::bof3::Img(battle_items::at::kSparkleOffsets.v) + (c2[battle_items::at::kLimit] & 0x1F) * 4)) - (r & 7));
        battle_items::SetWord(x, battle_items::Word(x) + dx);
    } else {
        const int r = battle_items::g.rand();
        const unsigned char* const c2 = CurBattleItems();
        const auto dx = static_cast<std::uint16_t>((r & 7) + battle_items::Word(battle_items::At(::bof3::Img(battle_items::at::kSparkleOffsets.v) + (c2[battle_items::at::kLimit] & 0x1F) * 4)));
        battle_items::SetWord(x, battle_items::Word(x) + dx);
    }
    unsigned char* const y = CurBattleItems() + battle_items::at::kY;
    {
        const int r = battle_items::g.rand();
        const unsigned char* const c2 = CurBattleItems();
        const auto dy = static_cast<std::uint16_t>((r & 7) + battle_items::Word(battle_items::At(::bof3::Img(battle_items::at::kSparkleOffsets.v) + 2 + (c2[battle_items::at::kLimit] & 0x1F) * 4)));
        battle_items::SetWord(y, battle_items::Word(y) - dy);
    }
    c = CurBattleItems();
    battle_items::SetWord(c + battle_items::at::kBaseX, battle_items::Word(c + battle_items::at::kX));
    {
        const int r = battle_items::g.rand();
        battle_items::SetWord(CurBattleItems() + battle_items::at::kDriftY, static_cast<unsigned>(r & 1));
    }
    {
        const int r = battle_items::g.rand();
        battle_items::SetWord(CurBattleItems() + battle_items::at::kWave, static_cast<unsigned>(r));
    }
    {
        const int r = battle_items::g.rand();
        c = CurBattleItems();
        c[battle_items::at::kLimit] = static_cast<unsigned char>((r & 6) + battle_items::At(::bof3::Img(battle_items::at::kSparkleLife.v))[c[battle_items::at::kKind]]);
    }
    CurBattleItems()[battle_items::at::kTimer] = 0x10;
    CurBattleItems()[battle_items::at::kCount] = 0;
    c = CurBattleItems();
    c[battle_items::at::kPhase] = static_cast<unsigned char>(c[battle_items::at::kPhase] + 1);
}

// original 0x4B9200 (PSX MAGIC070 0x801EF67C): the rise. The sway, then the
// count +6 up to the rise length +7, which moves the phase on.
extern "C" void __cdecl Sparkle_Rise(void) {
    Sway();
    unsigned char* c = CurBattleItems();
    c[battle_items::at::kCount] = static_cast<unsigned char>(c[battle_items::at::kCount] + 1);
    c = CurBattleItems();
    if (c[battle_items::at::kCount] == c[battle_items::at::kLimit]) c[battle_items::at::kPhase] = static_cast<unsigned char>(c[battle_items::at::kPhase] + 1);
}

// original 0x4B9270 (PSX MAGIC070 0x801EF72C): the fade. The sway; the count
// down every fourth frame; the brightness +5 down, and at 0 the actor's
// sparkle count +0xB down and the sparkle freed (a tail jmp in the original).
extern "C" void __cdecl Sparkle_Fade(void) {
    Sway();
    if ((Frame_Counter & 3) == 0) {
        unsigned char* const c = CurBattleItems();
        c[battle_items::at::kCount] = static_cast<unsigned char>(c[battle_items::at::kCount] - 1);
    }
    unsigned char* c = CurBattleItems();
    c[battle_items::at::kTimer] = static_cast<unsigned char>(c[battle_items::at::kTimer] - 1);
    if (CurBattleItems()[battle_items::at::kTimer] != 0) return;
    unsigned char* const actor = Actor();
    actor[0xB] = static_cast<unsigned char>(actor[0xB] - 1);
    battle_items::g.sparkle_free();
}

// original 0x4B9300 (PSX MAGIC070 0x801EF810): four gouraud lines from the
// sparkle's point outward, `radius` long, at angles ((start + 8k) & 0x1F) <<
// 7, bright at the centre (+5 * 6) and dark at the tip. Both arguments are
// read as their low words, as the original does (its caller pushes stale
// upper halves).
extern "C" void __cdecl Sparkle_DrawRaysG2(unsigned start, unsigned radius) {
    const unsigned char* const c = CurBattleItems();
    battle_items::SetWord(Scr(0), battle_items::Word(c + battle_items::at::kX));
    battle_items::SetWord(Scr(2), battle_items::Word(c + battle_items::at::kY));
    DrawModeAtSparkle();
    RayShade();
    int n = static_cast<int>(start & 0xFFFF);
    const int end = n + 0x20;
    const int r = static_cast<int>(radius & 0xFFFF);
    do {
        unsigned char* const p = Gfx_PacketNext;
        battle_items::g.set_line_g2(p);
        battle_items::g.set_semi(p, 1);
        PutFloat(p + 8, S16BattleItems(Scr(0)));
        battle_items::SetWord(Scr(6), static_cast<unsigned>((n & 0x1F) << 7));
        PutFloat(p + 0xC, S16BattleItems(Scr(2)));
        const int co = battle_items::g.cos(S16BattleItems(Scr(6)));
        PutFloat(p + 0x18, Mul12(co, r) + S16BattleItems(Scr(0)));
        const int si = battle_items::g.sin(S16BattleItems(Scr(6)));
        PutFloat(p + 0x1C, Mul12(si, r) + S16BattleItems(Scr(2)));
        p[4] = Scr(8)[0];
        p[5] = Scr(0xA)[0];
        p[6] = Scr(0xC)[0];
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        CommitAtSparkle(0x34);
        n += 8;
    } while (n < end);
}

// original 0x4B9490 (PSX MAGIC070 0x801EFA44): the same four rays as
// three-point lines - dark at the centre, bright (+5 * 6) at half the
// radius, dark at the tip.
extern "C" void __cdecl Sparkle_DrawRaysG3(unsigned start, unsigned radius) {
    const unsigned char* const c = CurBattleItems();
    battle_items::SetWord(Scr(0), battle_items::Word(c + battle_items::at::kX));
    battle_items::SetWord(Scr(2), battle_items::Word(c + battle_items::at::kY));
    DrawModeAtSparkle();
    RayShade();
    int n = static_cast<int>(start & 0xFFFF);
    const int end = n + 0x20;
    const int full = static_cast<int>(radius & 0xFFFF);
    const int half = full >> 1;
    do {
        unsigned char* const p = Gfx_PacketNext;
        battle_items::g.set_line_g3(p);
        battle_items::g.set_semi(p, 1);
        PutFloat(p + 8, S16BattleItems(Scr(0)));
        battle_items::SetWord(Scr(6), static_cast<unsigned>((n & 0x1F) << 7));
        PutFloat(p + 0xC, S16BattleItems(Scr(2)));
        const int co = battle_items::g.cos(S16BattleItems(Scr(6)));
        PutFloat(p + 0x18, Mul12(co, half) + S16BattleItems(Scr(0)));
        const int si = battle_items::g.sin(S16BattleItems(Scr(6)));
        PutFloat(p + 0x1C, Mul12(si, half) + S16BattleItems(Scr(2)));
        const int co2 = battle_items::g.cos(S16BattleItems(Scr(6)));
        PutFloat(p + 0x28, Mul12(co2, full) + S16BattleItems(Scr(0)));
        const int si2 = battle_items::g.sin(S16BattleItems(Scr(6)));
        const int y2 = Mul12(si2, full) + S16BattleItems(Scr(2));
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        PutFloat(p + 0x2C, y2);
        p[0x14] = Scr(8)[0];
        p[0x15] = Scr(0xA)[0];
        p[0x16] = Scr(0xC)[0];
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        CommitAtSparkle(0x34);
        n += 8;
    } while (n < end);
}

// original 0x4B9680 (PSX MAGIC070 0x801EFCDC): the sparkle's body - eight
// gouraud triangles round its point, radius +6 + Rand & 3, the centre in the
// colour row (+3 + 4 * +4) of 0x65ADD4 times the brightness +5, the rim
// +5 grey.
extern "C" void __cdecl Sparkle_DrawDisc(void) {
    DrawModeAtSparkle();
    const int r = battle_items::g.rand();
    const unsigned char* const c = CurBattleItems();
    battle_items::SetWord(Scr(4), static_cast<unsigned>((r & 3) + c[battle_items::at::kCount]));
    battle_items::SetWord(Scr(0), battle_items::Word(c + battle_items::at::kX));
    battle_items::SetWord(Scr(2), battle_items::Word(c + battle_items::at::kY));
    const unsigned char* const rgb = battle_items::At(::bof3::Img(battle_items::at::kSparkleColours.v) + (c[battle_items::at::kShade] + c[battle_items::at::kKind] * 4u) * 3u);
    battle_items::SetWord(Scr(8), rgb[0] * static_cast<unsigned>(c[battle_items::at::kTimer]));
    battle_items::SetWord(Scr(0xA), rgb[1] * static_cast<unsigned>(c[battle_items::at::kTimer]));
    battle_items::SetWord(Scr(0xC), rgb[2] * static_cast<unsigned>(c[battle_items::at::kTimer]));
    battle_items::SetWord(Scr(0xE), c[battle_items::at::kTimer]);
    int a = 0;
    do {
        unsigned char* const p = Gfx_PacketNext;
        battle_items::g.set_poly_g3(p);
        battle_items::g.set_semi(p, 1);
        PutFloat(p + 8, S16BattleItems(Scr(0)));
        PutFloat(p + 0xC, S16BattleItems(Scr(2)));
        int s = battle_items::g.sin(a);
        PutFloat(p + 0x18, Mul12(s, S16BattleItems(Scr(4))) + S16BattleItems(Scr(0)));
        int co = battle_items::g.cos(a);
        PutFloat(p + 0x1C, Mul12(co, S16BattleItems(Scr(4))) + S16BattleItems(Scr(2)));
        a += 0x200;
        s = battle_items::g.sin(a);
        PutFloat(p + 0x28, Mul12(s, S16BattleItems(Scr(4))) + S16BattleItems(Scr(0)));
        co = battle_items::g.cos(a);
        PutFloat(p + 0x2C, Mul12(co, S16BattleItems(Scr(4))) + S16BattleItems(Scr(2)));
        p[4] = Scr(8)[0];
        p[5] = Scr(0xA)[0];
        p[6] = Scr(0xC)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = Scr(0xE)[0];
        CommitAtSparkle(0x34);
    } while (a < 0x1000);
}

// original 0x4B98B0 (PSX MAGIC070 0x801F0000): the first of the 128 sparkles
// without bit 0, marked and its index answered; 0xFF when all are in use.
// Only al is defined in the original's answer (its upper bytes are the
// caller's eax); every caller keeps al.
extern "C" unsigned char __cdecl Sparkle_Alloc(void) {
    unsigned char i = 0;
    do {
        unsigned char* const s = battle_items::At(::bof3::Img(battle_items::at::kSparklePool.v) + i * battle_items::at::kSparkleStride);
        if ((s[battle_items::at::kFlags] & 1) == 0) {
            s[battle_items::at::kFlags] = static_cast<unsigned char>(s[battle_items::at::kFlags] | 1);
            return i;
        }
        ++i;
    } while (i < battle_items::at::kSparkles);
    return 0xFF;
}

// original 0x4B9900 (PSX MAGIC070 0x801F006C): bytes +0..+4 of the sparkle
// cleared, the pointer read again for each.
extern "C" void __cdecl Sparkle_Free(void) {
    for (unsigned k = 0; k < 5; ++k) CurBattleItems()[k] = 0;
}

// original 0x4B9930 (Magic_Rows row 45): the phase +1 through its six-entry
// stack table, then the pool walk.
MS16_EXPORT void __cdecl Magic071_Task(void) {
    Step(kTask071, 6, "Magic071_Task");
    Walk(k071, Magic071_SparkleDispatch);
}

// original 0x4B99C0: the spawn, kind 1.
MS16_EXPORT void __cdecl Magic071_Spawn(void) { Spawn(k071, 1); }

MS16_SPARKLES(071, k071)

// original 0x4BA460 (Magic_Rows row 113): as Magic071_Task.
MS16_EXPORT void __cdecl Magic072_Task(void) {
    Step(kTask072, 6, "Magic072_Task");
    Walk(k072, Magic072_SparkleDispatch);
}

// original 0x4BA4F0: the spawn, kind 2.
MS16_EXPORT void __cdecl Magic072_Spawn(void) { Spawn(k072, 2); }

MS16_SPARKLES(072, k072)

// original 0x4BAF90 (Magic_Rows row 47): the phase +1 through a two-entry
// stack table (the spawn, the wait), then the pool walk (160 records).
MS16_EXPORT void __cdecl Magic073_Task(void) {
    Step(kTask073, 2, "Magic073_Task");
    Walk(k073, Magic073_SparkleDispatch);
}

// original 0x4BB000: a child (kind 1, parameter 0x26) per actor, kind 0.
MS16_EXPORT void __cdecl Magic073_Spawn(void) { SpawnOnActors(k073, 0, 0x26); }

// original 0x4BB170: the parent's wait. While children live (+0xB) or any
// actor reacts (Magic073_CountReacting), nothing; then the effect-done flag
// (0x904AA8 bit 2) and the task freed (a tail jmp).
MS16_EXPORT void __cdecl Magic073_Wait(void) {
    if (Sprite_Current[0xB] != 0) return;
    if (MH_CALL(Magic073_CountReacting)() != 0) return;
    Mem(::bof3::Img(magic_harness::at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(magic_harness::at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4BB1A0: the child (kind 1, parameter 0x26): a jmp through
// 0x65B0E0 by +1, read in place, unchecked (+1 is 0 as the spawn sets it;
// entry 1 is the sparkle table's).
MS16_EXPORT void __cdecl Magic073_ActorDispatch(void) { CallCell(::bof3::Img(Magic073_ActorPhases_addr), Sprite_Current[1]); }

// original 0x4BB1C0: the child's phase +2 through a six-entry stack table.
MS16_EXPORT void __cdecl Magic073_ActorTask(void) { Step2(kActor073, "Magic073_ActorTask"); }

// original 0x4BB210: the child's start (sparkles from 0x6889A8's pool).
MS16_EXPORT void __cdecl Magic073_ActorStart(void) { ActorStart(k073); }

// original 0x4BB370 (shared: reached from MAGIC073, 074, 168, 169): with the
// task's +0xB (its live sparkles) 4 or fewer, +9 = 8 and the phase +2 on.
MS16_EXPORT void __cdecl ActorFx_WaitStep4(void) {
    unsigned char* const sc = Sprite_Current;
    if (sc[0xB] > 4) return;
    sc[9] = 8;
    BumpMagicS16(Sprite_Current[2]);
}

MS16_SPARKLES(073, k073)

// original 0x4BBCE0: how many actors are in the reaction state 6 and not out:
// the enemies (Battle_ActorIsOut(i + 3), the state byte at 0x93BCD9 +
// 0x128 i - three records past actor i + 3's own, as the original indexes
// it) for i 0..7, then the members (+1 of 0x802D40 + 0x14C i) for i 0..2.
MS16_EXPORT unsigned char __cdecl Magic073_CountReacting(void) {
    unsigned char n = 0;
    for (unsigned char i = 0; i < 8; ++i)
        if (MH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i + 3)) == 0 && Mem(::bof3::Img(Battle_Enemies_addr) + 0x379 + i * magic_harness::at::kEnemyStride)[0] == 6)
            ++n;
    for (unsigned char i = 0; i < 3; ++i)
        if (MH_CALL(Battle_ActorIsOut)(i) == 0 && Mem(::bof3::Img(ObjTrio_addr) + 0x1 + i * magic_harness::at::kPartyStride)[0] == 6) ++n;
    return n;
}

// original 0x4BBD60 (Magic_Rows row 114): the phase +1 through a two-entry
// stack table (the spawn, 0x4E5200), then the pool walk (160 records).
MS16_EXPORT void __cdecl Magic074_Task(void) {
    Step(kTask074, 2, "Magic074_Task");
    Walk(k074, Magic074_SparkleDispatch);
}

// original 0x4BBDD0: a child (kind 1, parameter 0x24) per actor, kind 1.
MS16_EXPORT void __cdecl Magic074_Spawn(void) { SpawnOnActors(k074, 1, 0x24); }

// original 0x4BBF40: the child (parameter 0x24): a jmp through 0x65B1AC by +1.
MS16_EXPORT void __cdecl Magic074_ActorDispatch(void) { CallCell(::bof3::Img(Magic074_ActorPhases_addr), Sprite_Current[1]); }

// original 0x4BBF60: the child's phase +2 through a six-entry stack table.
MS16_EXPORT void __cdecl Magic074_ActorTask(void) { Step2(kActor074, "Magic074_ActorTask"); }

// original 0x4BBFB0: the child's start (sparkles from 0x68A530's pool).
MS16_EXPORT void __cdecl Magic074_ActorStart(void) { ActorStart(k074); }

// original 0x4BC110 (shared by MAGIC073 and 074): the delay +9 down; at 0 the
// actor's (+4) tints released, a tint (0, 0, 0, 1) set on it and its record
// kept in +0xA, +9 = 8, the phase +2 on.
MS16_EXPORT void __cdecl ActorFx_Tint(void) {
    unsigned char* const sc = Sprite_Current;
    unsigned char* const rec = ActorRecord(sc[4]);
    BumpMagicS16(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(rec);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(rec, 0, 0, 0, 1);
    Sprite_Current[0xA] = tint;
    Sprite_Current[9] = 8;
    BumpMagicS16(Sprite_Current[2]);
}

// original 0x4BC1A0 (shared by MAGIC073 and 074): the tint record +0xA's
// colour (+2..+4 of the 12-byte MoveScript_TintRecords, the index unbounded)
// down one; +9 down, and at 0 the actor's tints released, the actor flashed
// and the phase +2 on.
MS16_EXPORT void __cdecl ActorFx_Untint(void) {
    unsigned char* const sc = Sprite_Current;
    unsigned char* const rec = ActorRecord(sc[4]);
    for (unsigned k = 2; k <= 4; ++k) BumpMagicS16(Mem(::bof3::Img(MoveScript_TintRecords_addr) + sc[0xA] * 12u + k)[0], -1);
    BumpMagicS16(sc[9], -1);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(rec);
    MH_CALL(BattleActor_Flash)(Sprite_Current[4]);
    BumpMagicS16(Sprite_Current[2]);
}

// original 0x4BC260 (shared by MAGIC073 and 074): once the child's sparkles
// are gone (+0xB 0), the parent's count +0xB down, the actor's flag 0x40, and
// the child freed (a tail jmp).
MS16_EXPORT void __cdecl ActorFx_End(void) {
    if (Sprite_Current[0xB] != 0) return;
    BumpMagicS16(Owner()[0xB], -1);
    MH_CALL(Battle_SetTargetFlag40)(Sprite_Current[4]);
    MH_CALL(BattleTask_FreeCurrent)();
}

MS16_SPARKLES(074, k074)

#undef MS16_SPARKLES

void Sparkle_Inject() {
    BOF3_INJECT(Influence_DrawCross);
    BOF3_INJECT(Influence_SpawnMarks);
    BOF3_INJECT(Sparkle_Task);
    BOF3_INJECT(Sparkle_Spawn);
    BOF3_INJECT(Sparkle_End);
    BOF3_INJECT(Sparkle_Dispatch);
    BOF3_INJECT(Sparkle_Update);
    BOF3_INJECT(Sparkle_Launch);
    BOF3_INJECT(Sparkle_Rise);
    BOF3_INJECT(Sparkle_Fade);
    BOF3_INJECT(Sparkle_DrawRaysG2);
    BOF3_INJECT(Sparkle_DrawRaysG3);
    BOF3_INJECT(Sparkle_DrawDisc);
    BOF3_INJECT(Sparkle_Alloc);
    BOF3_INJECT(Sparkle_Free);
    BOF3_INJECT(Magic071_Task);
    BOF3_INJECT(Magic071_Spawn);
    BOF3_INJECT(Magic071_SparkleDispatch);
    BOF3_INJECT(Magic071_SparkleUpdate);
    BOF3_INJECT(Magic071_SparkleLaunch);
    BOF3_INJECT(Magic071_SparkleRise);
    BOF3_INJECT(Magic071_SparkleFade);
    BOF3_INJECT(Magic071_SparkleRaysG2);
    BOF3_INJECT(Magic071_SparkleRaysG3);
    BOF3_INJECT(Magic071_SparkleDisc);
    BOF3_INJECT(Magic071_SparkleAlloc);
    BOF3_INJECT(Magic071_SparkleFree);
    BOF3_INJECT(Magic072_Task);
    BOF3_INJECT(Magic072_Spawn);
    BOF3_INJECT(Magic072_SparkleDispatch);
    BOF3_INJECT(Magic072_SparkleUpdate);
    BOF3_INJECT(Magic072_SparkleLaunch);
    BOF3_INJECT(Magic072_SparkleRise);
    BOF3_INJECT(Magic072_SparkleFade);
    BOF3_INJECT(Magic072_SparkleRaysG2);
    BOF3_INJECT(Magic072_SparkleRaysG3);
    BOF3_INJECT(Magic072_SparkleDisc);
    BOF3_INJECT(Magic072_SparkleAlloc);
    BOF3_INJECT(Magic072_SparkleFree);
    BOF3_INJECT(Magic073_Task);
    BOF3_INJECT(Magic073_Spawn);
    BOF3_INJECT(Magic073_Wait);
    BOF3_INJECT(Magic073_ActorDispatch);
    BOF3_INJECT(Magic073_ActorTask);
    BOF3_INJECT(Magic073_ActorStart);
    BOF3_INJECT(ActorFx_WaitStep4);
    BOF3_INJECT(Magic073_SparkleDispatch);
    BOF3_INJECT(Magic073_SparkleUpdate);
    BOF3_INJECT(Magic073_SparkleLaunch);
    BOF3_INJECT(Magic073_SparkleRise);
    BOF3_INJECT(Magic073_SparkleFade);
    BOF3_INJECT(Magic073_SparkleRaysG2);
    BOF3_INJECT(Magic073_SparkleRaysG3);
    BOF3_INJECT(Magic073_SparkleDisc);
    BOF3_INJECT(Magic073_SparkleAlloc);
    BOF3_INJECT(Magic073_SparkleFree);
    BOF3_INJECT(Magic073_CountReacting);
    BOF3_INJECT(Magic074_Task);
    BOF3_INJECT(Magic074_Spawn);
    BOF3_INJECT(Magic074_ActorDispatch);
    BOF3_INJECT(Magic074_ActorTask);
    BOF3_INJECT(Magic074_ActorStart);
    BOF3_INJECT(ActorFx_Tint);
    BOF3_INJECT(ActorFx_Untint);
    BOF3_INJECT(ActorFx_End);
    BOF3_INJECT(Magic074_SparkleDispatch);
    BOF3_INJECT(Magic074_SparkleUpdate);
    BOF3_INJECT(Magic074_SparkleLaunch);
    BOF3_INJECT(Magic074_SparkleRise);
    BOF3_INJECT(Magic074_SparkleFade);
    BOF3_INJECT(Magic074_SparkleRaysG2);
    BOF3_INJECT(Magic074_SparkleRaysG3);
    BOF3_INJECT(Magic074_SparkleDisc);
    BOF3_INJECT(Magic074_SparkleAlloc);
    BOF3_INJECT(Magic074_SparkleFree);
}
