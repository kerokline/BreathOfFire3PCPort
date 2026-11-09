// Moved here by tools/move_funcs.py from battle_odds.cpp, magic_s12.cpp (docs/source-layout-round.md).
#include "game/magic/celerity_spark.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_odds.h"
#include "game/battle_odds_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/magic_harness.h"
#include "game/magic_s12.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using namespace battle_odds;

}  // namespace

namespace {

unsigned char* AtBattleOdds(battle_odds::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U SpriteCurrent() { return Long(AtBattleOdds(battle_odds::at::Sprite_CurrentAt())); }

unsigned char* Sprite() { return AtBattleOdds(SpriteCurrent()); }

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// Celerity_End's byte, and Celerity_ApplyStat's cells: the result record
// pointer, the ability word and the step byte of its NameTable_Abilities
// record (+3, records of 0x18).
// kEndByte: now ModeFlow_CountdownA (symbols.toml).
// kResultRecord: now BattleDamage_Result (symbols.toml).
constexpr ::bof3::image::Va kAbility{0x904B80};

constexpr ::bof3::image::Va kAbilityStep{0x65C4DB};

// Capcom's, unnamed, in no group: a stat change for the target
// (MagicFx_ApplyBuff's 0x44F650 calls it the same way).
constexpr ::bof3::image::CodeVa kStatChanged{bof3::addr::Battle_RecalcStats};   // rebound 2026-09-29 (round twelve, BE6): the value is unchanged, the fuzz keys on it

unsigned char* SC() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char* Slot(unsigned k) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (k & 0xFFu) * at::kTaskStride); }

std::int32_t Ptr(const void* p) { return static_cast<std::int32_t>(Low32(p)); }

std::uint32_t U32(const void* p) { return Low32(p); }

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::int32_t AddL(std::int32_t a, std::uint32_t b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + b); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

// The records as the originals index them, unchecked: a member by its index
// (stride 0x14C), an enemy by its index (stride 0x128) - the battle index
// less 3, which a party index sends below the enemy records.
unsigned char* PartyRec(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRec(int i) { return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(i) * at::kEnemyStride); }

// The scratch words SharedScratch + k and the vertex scratch
// Prim_VertexScratch + k (SVECTORs at +0, +8, +0x10, +0x18).
constexpr ::bof3::image::Va kDs{SharedScratch_addr};

// kVx: now Prim_VertexScratch (symbols.toml).
short Ds(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(kDs.v) + k))); }

void SetDs(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kDs.v) + k), v); }

unsigned char DsByte(unsigned k) { return Mem(::bof3::Img(kDs.v) + k)[0]; }

short Vx(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k))); }

void SetVx(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v); }

const short* VxP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

// `imul` (a 32-bit product that wraps) then `sar n`; `shl l` then `sar r`.
int MulSar(int a, int b, int n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

int ShlSar(int a, int l, int r) { return static_cast<int>(static_cast<std::uint32_t>(a) << l) >> r; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

float* Fl(unsigned char* p) { return reinterpret_cast<float*>(p); }

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

// This group's functions called directly, by their addresses as the originals
// call them: in the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using FnU = void (__cdecl*)(unsigned);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// The GTE calls as the originals push them: one pointer more than
// symbols.toml's prototypes carry (a flag word the callee may write).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                               long*, long*);

using Avg3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

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

// A draw-mode packet (tpage `tpage`, dithered) committed to `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

void LinkAt(unsigned dy, unsigned size) {
    const unsigned char* const sc = SC();
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z),
                                static_cast<int>(dy), size);
}

}  // namespace

#define MS12_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

void DrawDisc(unsigned radius) { MH_AT(FnU, bof3::addr::CeleritySpark_DrawDisc)(radius); }

// The sparks' shared frame: the actor's screen point, a disc of radius
// `base` + Rand() & 3, and the spark itself under the actor matrix.
void SparkFrame(unsigned base) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    const int r = MH_CALL(Rand)();
    DrawDisc(static_cast<unsigned>((r & 3) + static_cast<int>(base)));
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::CeleritySpark_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// +9 up on odd frames; the speed +0x14 plus the acceleration +0x20, and the
// height word +0x3E plus the speed's low word; +0xA down, answering whether
// it reached 0.
bool SparkFly() {
    if (Frame_Counter & 1) Bump(SC()[9]);
    unsigned char* sc = SC();
    rv::Store(rv::As<records::BattleTask>(sc)->step_y, AddL(rv::As<records::BattleTask>(sc)->step_y, static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->work_20)));
    sc = SC();
    rv::Store<unsigned>(rv::As<records::BattleTask>(sc)->height_whole, rv::As<records::BattleTask>(sc)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->step_y));
    Drop(SC()[0xA]);
    return SC()[0xA] == 0;
}

// One of CeleritySpark_Draw's two rows: four flat triangles from +9 to +9 +
// 0x20 by 8, each from the centre to two points of radius 0x30 at angles
// (a & 31) << 7 and ((a + 8) & 31) << 7, the centre `height` above (the
// first row +0x50, the second the scratch word +2 negated), coloured by
// `colors`[3 * (i + 4 * +4)]; each triangle's depth into `depths` and the
// packet cursor moved past it (0x34).
void SparkRow(const unsigned char* colors, bool first, int (&depths)[4]) {
    unsigned char* sc = SC();
    int a = sc[9];
    if (a >= sc[9] + 0x20) return;
    do {
        if (first) {
            SetDs(2, 0x50);
            SetDs(0, 0x30);
        }
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
        if (k < 0 || k > 3) bof3::Fatal("CeleritySpark_Draw: depth index %d, past the four-entry array", k);
        depths[k] = static_cast<int>(depth);
        for (unsigned q : {4u, 0x14u, 0x24u}) {
            p[q] = DsByte(8);
            p[q + 1] = DsByte(0xA);
            p[q + 2] = DsByte(0xC);
        }
        Gfx_PacketNext = Gfx_PacketNext + 0x34;
        sc = SC();
    } while (a < sc[9] + 0x20);
}

}  // namespace

// original 0x4B1CB0: the kind-2 task. Its phase +1 through a six-entry stack
// table: Celerity_Start, BattleFx_TintActor, BattleFx_Brighten (round eight's),
// Buff_WaitChildren (MAGIC082's), Celerity_Apply, Celerity_End. Unchecked in
// the original; ours aborts.
MS12_EXPORT void __cdecl Celerity_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Celerity_Start,    bof3::addr::BattleFx_TintActor,
                                                 bof3::addr::BattleFx_Brighten, bof3::addr::Buff_WaitChildren,
                                                 bof3::addr::Celerity_Apply,    bof3::addr::Celerity_End};
    CallPhase(kPhases, 6, "Celerity_Task");
}

// original 0x4B1D00: the target's sprite's +8 and position to the task; +0xB
// 0, +9 0x10, the phase on. Five children BattleTask_Create(1, 0x4A), this
// task's +0xB counting them: the ring (+1 0, +9 0, the task's position) and
// four sparks (+1 1, +4 0..3, +0xB 0 / 8 / 0x10 / 0x18, +9 that + 1, +0xA the
// task's +9). The CLUT strip 0x80E980 (16 + 16 words) to 0x812980,
// Gfx_ClutStripDirty 1, Sound_PlayById(0x100).
MS12_EXPORT void __cdecl Celerity_Start(void) {
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    SC()[8] = src[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    SC()[0xB] = 0;
    SC()[9] = 0x10;
    Bump(SC()[1]);
    {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x4A));
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        unsigned char* const sc = SC();
        records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
        tt->owner = Ptr(sc);
        t[1] = 0;
        t[9] = 0;
        tt->x = tsc->x;
        tt->z = tsc->z;
        rv::Store(tt->height, tsc->height);
        Bump(sc[0xB]);
    }
    unsigned char spark = 0;
    for (unsigned b = 0; b < 0x20; b += 8) {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x4A));
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        unsigned char* const sc = SC();
        tt->owner = Ptr(sc);
        t[1] = 1;
        t[4] = spark;
        t[0xB] = static_cast<unsigned char>(b);
        t[9] = static_cast<unsigned char>(b + 1);
        t[0xA] = sc[9];
        Bump(sc[0xB]);
        ++spark;
    }
    for (unsigned i = 0; i < 0x20; i += 2) {
        SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + i), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400 + i)));
        SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + 0x20 + i), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400 + 0x20 + i)));
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4B1E70: the countdown +9 stepped (a byte); at 0 the acting
// actor's object (kActorObject, read before the countdown) loses its tints
// (Sprite_ReleaseTint) and gets a new one, Sprite_SetTint(object, 0, 0, 0, 1),
// whose index (al) becomes +0xA; then +9 = 8 and the next state. Sprite_Current
// is read again for each store. eax is Sprite_Current as last read.
extern "C" unsigned long __cdecl BattleFx_TintActor(void) {
    const U object = BattleOdds_ActorObject;
    unsigned char* s = Sprite();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Sprite();
    if (s[9] != 0) return Low32(s);
    g.release_tint(object);
    const U index = g.set_tint(object, 0, 0, 0, 1);
    Sprite()[0xA] = static_cast<unsigned char>(index);
    Sprite()[9] = 8;
    s = Sprite();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    return Low32(s);
}

// original 0x4B1ED0: the red, green and blue bytes (+2, +3, +4) of tint record
// +0xA of MoveScript_TintRecords (12 bytes each, the index read again for
// each; no bound) stepped up by one; then the countdown +9 down, and at 0 the
// next state. Sprite_Current is read once for all of that and once more for
// the test. eax Sprite_Current.
extern "C" unsigned long __cdecl BattleFx_Brighten(void) {
    unsigned char* const s = Sprite();
    for (U k = 2; k <= 4; ++k) {
        unsigned char* const c = AtBattleOdds(battle_odds::at::TintRecordsAt() + s[0xA] * 12u + k);
        c[0] = static_cast<unsigned char>(c[0] + 1);
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    unsigned char* const t = Sprite();
    if (t[9] == 0) t[1] = static_cast<unsigned char>(t[1] + 1);
    return Low32(t);
}

// original 0x4B1F40: the tint record MoveScript_TintRecords[+0xA]'s three
// colour bytes (+2, +3, +4) down one; +9 down. At 0: the tint released, the
// target flashed; for each of the four stats MagicFx_BuffStats[i] the stat
// applied (Celerity_ApplyStat) and a popup child BattleTask_Create(1, 0x48)
// (+4 i, +9 5i + 1, +0xA 12i + 1), +0xB up; the phase on.
MS12_EXPORT void __cdecl Celerity_Apply(void) {
    unsigned char* const sc = SC();
    unsigned char* const tints = MoveScript_TintRecords;
    Drop(tints[sc[0xA] * 12u + 2]);
    Drop(tints[sc[0xA] * 12u + 3]);
    Drop(tints[sc[0xA] * 12u + 4]);
    Drop(sc[9]);
    if (SC()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    MH_CALL(BattleActor_Flash)(TargetByte());
    for (unsigned i = 0; i < 4; ++i) {
        MH_AT(FnU, bof3::addr::Celerity_ApplyStat)(MagicFx_BuffStats[i]);
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x48));
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        unsigned char* const owner = SC();
        tt->owner = Ptr(owner);
        t[4] = static_cast<unsigned char>(i);
        t[9] = static_cast<unsigned char>(i * 5 + 1);
        t[0xA] = static_cast<unsigned char>(i * 12 + 1);
        Bump(owner[0xB]);
    }
    Bump(SC()[1]);
}

// original 0x4B2040: once the children have ended (+0xB 0): the effect-done
// bit, the byte 0x90465C 5, the task freed.
MS12_EXPORT void __cdecl Celerity_End(void) {
    if (SC()[0xB] != 0) return;
    Mem(::bof3::Img(magic_harness::at::kFlags))[0] |= 4;
    Mem(::bof3::Img(ModeFlow_CountdownA_addr))[0] = 5;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B2060: one stat of the target moved by the ability's step.
// 0x904B60 = the target's result record (a member's record + 0x124 below 3,
// else the enemy's + 0x104); its words +4 and +6 and byte +8 cleared; the
// byte +0x14 + stat plus the signed step (the byte at 0x65C4DB + 0x18 * the
// ability word 0x904B80): above 100 made 100, below -100 made -100, else
// stored and the engine's 0x453300(target) told.
MS12_EXPORT void __cdecl Celerity_ApplyStat(unsigned stat) {
    const unsigned char target = TargetByte();
    unsigned char* const rec = target < 3 ? PartyRec(target) + 0x124 : EnemyRec(static_cast<int>(target) - 3) + 0x104;
    SetLong(Mem(::bof3::Img(BattleDamage_Result_addr)), Ptr(rec));
    SetWord(Pointer(::bof3::Img(BattleDamage_Result_addr)) + 4, 0);
    SetWord(Pointer(::bof3::Img(BattleDamage_Result_addr)) + 6, 0);
    Pointer(::bof3::Img(BattleDamage_Result_addr))[8] = 0;
    const signed char step = static_cast<signed char>(Mem(::bof3::Img(kAbilityStep.v) + 0x18u * Word(Mem(::bof3::Img(kAbility.v))))[0]);
    unsigned char* const cell = Pointer(::bof3::Img(BattleDamage_Result_addr)) + 0x14 + (stat & 0xFFu);
    const int value = static_cast<signed char>(cell[0]) + step;
    if (value > 100) {
        cell[0] = 100;
        return;
    }
    if (value < -100) {
        cell[0] = 0x9C;
        return;
    }
    cell[0] = static_cast<unsigned char>(cell[0] + static_cast<unsigned char>(step));
    MH_AT(FnU, kStatChanged.v)(TargetByte());
}

// original 0x4B2120: the children (kind 1, parameter 0x4A): jmp
// [CelerityChild_Kinds + 4 * +1] - the ring, the sparks.
MS12_EXPORT void __cdecl CelerityChild_Task(void) { CallCell(CelerityChild_Kinds, 2, SC()[1], "CelerityChild_Task"); }

// original 0x4B2140: the ring - its three steps (BarrierRing_Grow,
// BuffRing_Wait, MagicFx_CountDownRelease); while live (+0), the disc and the
// band under the actor matrix.
MS12_EXPORT void __cdecl CelerityRing_Run(void) {
    CallCell(CelerityRing_Steps, 3, SC()[2], "CelerityRing_Run");
    if (SC()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::CelerityRing_DrawDisc);
    Call0(bof3::addr::CelerityRing_DrawBand);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4B2180: a flat disc of sixteen semi-transparent Gouraud
// triangles (tpage 0x35), radius 0x80 (sin / cos << 7 >> 12); the centre
// +9 * 5 grey, the rim (Rand & 3 + 3) * +9 per channel (one Rand each, the
// words 0x903858 / 0x90385A / 0x90385C, which the band reads).
MS12_EXPORT void __cdecl CelerityRing_DrawDisc(void) {
    DrawModeCommit(0x35, 5);
    SetDs(6, SC()[9] * 5u);
    for (unsigned k : {8u, 0xAu, 0xCu}) {
        const int r = MH_CALL(Rand)();
        SetDs(k, static_cast<unsigned>((r & 3) + 3) * SC()[9]);
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
        p[4] = DsByte(6);
        p[5] = DsByte(6);
        p[6] = DsByte(6);
        for (unsigned k : {0x14u, 0x24u}) {
            p[k] = DsByte(8);
            p[k + 1] = DsByte(0xA);
            p[k + 2] = DsByte(0xC);
        }
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawModeCommit(0x15, 5);
}

// original 0x4B2380: a flat band of sixteen semi-transparent Gouraud quads
// (tpage 0x35) from radius 0x80 to 0x100; the inner edge the scratch words
// the disc left, the outer 1 1 1.
MS12_EXPORT void __cdecl CelerityRing_DrawBand(void) {
    DrawModeCommit(0x35, 5);
    int r = Sin(0);
    SetVx(8, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Cos(0);
    SetVx(0xA, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Sin(0);
    SetVx(0x18, static_cast<unsigned>(ShlSar(r, 8, 12)));
    r = Cos(0);
    SetVx(0x1A, static_cast<unsigned>(ShlSar(r, 8, 12)));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        SetVx(0, static_cast<std::uint16_t>(Vx(8)));
        SetVx(2, static_cast<std::uint16_t>(Vx(0xA)));
        SetVx(0x10, static_cast<std::uint16_t>(Vx(0x18)));
        SetVx(0x12, static_cast<std::uint16_t>(Vx(0x1A)));
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
            p[k] = DsByte(8);
            p[k + 1] = DsByte(0xA);
            p[k + 2] = DsByte(0xC);
        }
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    DrawModeCommit(0x15, 5);
}

// original 0x4B2570: a spark - jmp [CeleritySpark_Steps + 4 * +2] (six:
// MagicFx_WaitA, ShieldSpark_Place, then the four below).
MS12_EXPORT void __cdecl CeleritySpark_Run(void) { CallCell(CeleritySpark_Steps, 6, SC()[2], "CeleritySpark_Run"); }

// original 0x4B2590: the spark's frame (disc 0x18); flies; at +0xA 0, the
// speed -16, the acceleration 4, +0xA 8, on.
MS12_EXPORT void __cdecl CeleritySpark_Rise(void) {
    SparkFrame(0x18);
    if (!SparkFly()) return;
    rv::Store(rv::Current<records::BattleTask>()->step_y, -16);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 4);
    SC()[0xA] = 8;
    Bump(SC()[2]);
}

// original 0x4B2630: the same frame and flight; at +0xA 0, +0xA 0x1C - +0xB
// and on.
MS12_EXPORT void __cdecl CeleritySpark_Fall(void) {
    SparkFrame(0x18);
    if (!SparkFly()) return;
    unsigned char* const sc = SC();
    sc[0xA] = static_cast<unsigned char>(0x1C - sc[0xB]);
    Bump(SC()[2]);
}

// original 0x4B26C0: the frame (disc 0x40 once the spin +0xC is 4 and +0xA
// below 5, else 0x18); the spin up one every eighth frame of +0x10 until 4;
// +9 (the angle) plus the spin. At spin 4, +0xA down; at 0,
// Sound_PlayById(0x101) for the first spark (+0xB 0), +0x10 +0x14 +0x20 8,
// on.
MS12_EXPORT void __cdecl CeleritySpark_Spin(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* sc = SC();
    const bool wide = rv::As<records::BattleTask>(sc)->step_x == 4 && sc[0xA] < 5;
    const int r = MH_CALL(Rand)();
    DrawDisc(static_cast<unsigned>((r & 3) + (wide ? 0x40 : 0x18)));
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::CeleritySpark_Draw);
    MH_CALL(Gte_PopMatrix)();
    sc = SC();
    if (rv::As<records::BattleTask>(sc)->step_x < 4) {
        rv::Store(rv::As<records::BattleTask>(sc)->step_z, AddL(rv::As<records::BattleTask>(sc)->step_z, 1));
        sc = SC();
        if ((sc[0x10] & 7) == 0) {
            rv::Store(rv::As<records::BattleTask>(sc)->step_x, AddL(rv::As<records::BattleTask>(sc)->step_x, 1));
            sc = SC();
        }
    }
    sc[9] = static_cast<unsigned char>(sc[9] + sc[0xC]);
    sc = SC();
    if (rv::As<records::BattleTask>(sc)->step_x != 4) return;
    Drop(sc[0xA]);
    sc = SC();
    if (sc[0xA] != 0) return;
    if (sc[0xB] == 0) {
        MH_CALL(Sound_PlayById)(0x101);
        sc = SC();
    }
    rv::Store(rv::As<records::BattleTask>(sc)->step_z, 8);
    rv::Store(rv::Current<records::BattleTask>()->step_y, 8);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 8);
    Bump(SC()[2]);
}

// original 0x4B27A0: the frame (disc 0x20); +0x10 up 4; +9 plus the spin; +0xB
// up 2; the spark at the owner's x / z plus sin / cos((+0xB & 31) << 7) times
// (+0xA * +0x10 + 0xB0) >> 3; +0xA up; at 0x10, the owner's +0xB down and
// freed.
MS12_EXPORT void __cdecl CeleritySpark_Orbit(void) {
    SparkFrame(0x20);
    unsigned char* sc = SC();
    rv::Store(rv::As<records::BattleTask>(sc)->step_z, AddL(rv::As<records::BattleTask>(sc)->step_z, 4));
    sc = SC();
    sc[9] = static_cast<unsigned char>(sc[9] + sc[0xC]);
    sc = SC();
    sc[0xB] = static_cast<unsigned char>(sc[0xB] + 2);
    sc = SC();
    SetDs(4, (sc[0xB] & 0x1Fu) << 7);
    SetDs(0, sc[0xA] * static_cast<unsigned>(Word(sc + 0x10)) + 0xB0u);
    int r = Sin(Ds(4));
    rv::Store(rv::Current<records::BattleTask>()->x, AddL(static_cast<std::int32_t>(MulSar(r, Ds(0), 3)), static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x)));
    r = Cos(Ds(4));
    rv::Store(rv::Current<records::BattleTask>()->z, AddL(static_cast<std::int32_t>(MulSar(r, Ds(0), 3)), static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z)));
    Bump(SC()[0xA]);
    if (SC()[0xA] != 0x10) return;
    Drop(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B28B0: a spark (tpage 0x15, linked at the task's x / z, +2,
// 0xC), two rows of four triangles - up from CeleritySpark_ColorsUp, down
// from CeleritySpark_ColorsDown - each row's four depths and first packet to
// MagicFx_LinkByDepth(x, z, depths, packets, 4, 0x34, 2).
MS12_EXPORT void __cdecl CeleritySpark_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    LinkAt(2, 0xC);
    int depths[4] = {};
    unsigned char* const up = Gfx_PacketNext;
    SparkRow(CeleritySpark_ColorsUp, true, depths);
    {
        const unsigned char* const sc = SC();
        const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
        MH_CALL(MagicFx_LinkByDepth)(static_cast<unsigned>(tsc->x), static_cast<unsigned>(tsc->z), depths,
                                     U32(up), 4, 0x34, 2);
    }
    unsigned char* const down = Gfx_PacketNext;
    SparkRow(CeleritySpark_ColorsDown, false, depths);
    const unsigned char* const sc = SC();
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MagicFx_LinkByDepth)(static_cast<unsigned>(tsc->x), static_cast<unsigned>(tsc->z), depths,
                                 U32(down), 4, 0x34, 2);
}

// original 0x4B2D70: a flat disc of eight semi-transparent Gouraud triangles
// on screen (tpage 0x35), at the task's screen point (+0x2E / +0x30), radius
// the argument's low word; the centre CeleritySpark_DiscColors[+4] (through
// the scratch words 0x903858 / 0x90385A / 0x90385C), the rim 1 1 1; each
// linked at the task's x / z, +2 (0xC for the mode, 0x34 for the triangle).
MS12_EXPORT void __cdecl CeleritySpark_DrawDisc(unsigned radius) {
    SetDs(0, radius & 0xFFFFu);
    {
        const unsigned char* const rgb = CeleritySpark_DiscColors + 3u * SC()[4];
        SetDs(8, rgb[0]);
        SetDs(0xA, rgb[1]);
        SetDs(0xC, rgb[2]);
    }
    int a = 0;
    do {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        LinkAt(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(SC() + 0x2E));
        PutFloat(p + 0xC, S16(SC() + 0x30));
        int r = Sin(a);
        PutFloat(p + 0x18, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(S16(SC() + 0x2E))));
        r = Cos(a);
        PutFloat(p + 0x1C, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(S16(SC() + 0x30))));
        a += 0x200;
        r = Sin(a);
        PutFloat(p + 0x28, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(S16(SC() + 0x2E))));
        r = Cos(a);
        PutFloat(p + 0x2C, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(S16(SC() + 0x30))));
        p[4] = DsByte(8);
        p[5] = DsByte(0xA);
        p[6] = DsByte(0xC);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        LinkAt(2, 0x34);
    } while (a < 0x1000);
}

void CeleritySpark_Inject() {
    BOF3_INJECT(Celerity_Task);
    BOF3_INJECT(Celerity_Start);
    BOF3_INJECT(BattleFx_TintActor);
    BOF3_INJECT(BattleFx_Brighten);
    BOF3_INJECT(Celerity_Apply);
    BOF3_INJECT(Celerity_End);
    BOF3_INJECT(Celerity_ApplyStat);
    BOF3_INJECT(CelerityChild_Task);
    BOF3_INJECT(CelerityRing_Run);
    BOF3_INJECT(CelerityRing_DrawDisc);
    BOF3_INJECT(CelerityRing_DrawBand);
    BOF3_INJECT(CeleritySpark_Run);
    BOF3_INJECT(CeleritySpark_Rise);
    BOF3_INJECT(CeleritySpark_Fall);
    BOF3_INJECT(CeleritySpark_Spin);
    BOF3_INJECT(CeleritySpark_Orbit);
    BOF3_INJECT(CeleritySpark_Draw);
    BOF3_INJECT(CeleritySpark_DrawDisc);
}
