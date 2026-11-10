// Moved here by tools/move_funcs.py from magic_s28.cpp, magic_s29.cpp (docs/source-layout-round.md).
#include "game/magic/breath_mote.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s29_callees.h"
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

// SharedScratch's sixteen bytes (0x903850..0x90385F), words the overlays keep
// their working values in, and Prim_VertexScratch's first two words (MAGIC124's
// bolt keeps its moving point there). All are read again after every call, as
// the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

void AddSW(unsigned k, unsigned v) { SetSW(k, SW(k) + v); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

unsigned char U8(int v) { return static_cast<unsigned char>(v); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Addr(const void* p) { return Low32(p); }

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

// The two pools the tasks of MAGIC123 and MAGIC124 run themselves: records of
// the task slots' 0x84 bytes (+0 bit 0 in use, +1 the kind, +0x80 the owner).
// kMotePool: now BreathMote_Pool (symbols.toml). BreathMote_Pool: 90 records
constexpr unsigned kMotes = 90;

// kBoltPool: now ThunderPool (symbols.toml). ThunderPool: 64 records
constexpr unsigned kBolts = 64;

unsigned char* Record(std::uint32_t pool, unsigned k) { return Mem(pool + k * at::kTaskStride); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void CallCell(const char* who, std::uint32_t table, unsigned index, unsigned entries) {
    if (index >= entries) PastTable(who, index, entries);
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))))();
}

// Callees of other units, ours now, by name: S37's MagicFx_FreeCurrentRecord
// (a pool record's free) and S38's MagicFx_CountDownFlag10 (a stack-table
// phase: +9 down, at 0 the target flags 0x10 and +1 on).
using VoidFn = void (__cdecl*)();

// A pool task's frame (Icebreath_Task, Thunderbreath_Task): every record with
// bit 0 run through `dispatch` as Sprite_Current, its +0x80 the owner, both put
// back after each. The task and the owner are read once, before the loop.
void RunPool(std::uint32_t pool, unsigned n, void (__cdecl* dispatch)()) {
    unsigned char* const task = Sc();
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const rec = Record(pool, i);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        if ((rec[0] & 1) == 0) continue;
        const std::int32_t its = static_cast<std::int32_t>(trec->owner);
        Sprite_Current = rec;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        dispatch();
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
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
    if (Mem(::bof3::Img(at::kActor))[0] < 3) {
        MH_CALL(MagicFx_FormationOffset)();
        return;
    }
    unsigned char* const sc = Sc();
    records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
    switch (sc[8]) {
    case 0: tsc->step_x = 0x29; tsc->step_z = -13; break;
    case 1: tsc->step_x = -41; tsc->step_z = -13; break;
    case 2: tsc->step_x = -38; tsc->step_z = -38; break;
    default: tsc->step_x = 0x26; tsc->step_z = -38; break;
    }
}

// The caster's position (+0x34, +0x38, +0x3C from the owner), each dword with
// the task and the owner read again.
void CopyOwnerPosition() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

}  // namespace

#define S28_EXPORT extern "C" __attribute__((disable_tail_calls))

// The angle of a mote's corner j: (BreathMote_Angles[j] + +9) & 0x3F, << 6.
unsigned MoteAngle(unsigned j) { return ((Mem(::bof3::Img(BreathMote_Angles_addr) + j)[0] + Sc()[9]) & 0x3Fu) << 6; }

// A corner at radius r (a scratch word) from the mote's point (+6 / +8) along
// the angle in the scratch +4.
void MoteCorner(unsigned char* p, unsigned off, unsigned r, bool angle_fresh, unsigned angle) {
    int s = Sin(angle_fresh ? static_cast<int>(static_cast<short>(angle)) : SS(4));
    PutF(p, off, Sar(Mul(s, SS(r)), 12) + SS(6));
    s = Cos(SS(4));
    PutF(p, off + 4, Sar(Mul(s, SS(r)), 12) + SS(8));
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

namespace {

namespace cell = magic_s29::cell;

using magic_harness::Handler;

// The original's imul: 32 bits, wrapping; `sar 0xC` after it is arithmetic.
inline int MulMagicS29(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

inline int Mul12(int a, int b) { return MulMagicS29(a, b) >> 12; }

inline std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

inline std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

inline short S16MagicS29(const unsigned char* p) { return static_cast<short>(Word(p)); }

inline std::uint32_t AddrMagicS29(const void* p) { return Low32(p); }

inline void SetPtr(unsigned char* at_, const void* p) { SetLong(at_, static_cast<std::int32_t>(AddrMagicS29(p))); }

inline void IncMagicS29(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

inline void DecMagicS29(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// `fild dword` then `fstp dword`: an integer as a float.
inline void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

inline unsigned char* ScMagicS29() { return Sprite_Current; }

inline unsigned char* OwnerMagicS29() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

inline unsigned char TargetByteMagicS29() { return Mem(::bof3::Img(at::kTarget))[0]; }

inline unsigned char* TaskSlot(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (index & 0xFFu) * at::kTaskStride); }

inline unsigned char* Mote(unsigned index) { return Mem(::bof3::Img(cell::kMotePool) + (index & 0xFFu) * cell::kMoteStride); }

// The scratch words and vertices, read and written in place.
inline std::uint16_t SWMagicS29(std::uint32_t a) { return Word(Mem(a)); }

inline short SSMagicS29(std::uint32_t a) { return static_cast<short>(Word(Mem(a))); }

inline unsigned char SB(std::uint32_t a) { return Mem(a)[0]; }

inline void Put(std::uint32_t a, int v) { SetWord(Mem(a), static_cast<unsigned>(v) & 0xFFFFu); }

inline const short* Vec(std::uint32_t a) { return reinterpret_cast<const short*>(Mem(a)); }

inline float* Xy(unsigned char* prim, unsigned offset) { return reinterpret_cast<float*>(prim + offset); }

// A .data table of handlers, read in place as the original reads it (during
// the fuzz its entries are the harness's recorders).
Handler DataPhase(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Callees.
int SinMagicS29(int angle) { return MH_CALL(Math_Sin)(angle); }

int CosMagicS29(int angle) { return MH_CALL(Math_Cos)(angle); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

void DrawModeMagicS29(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

void CommitMagicS29(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

void Link(std::int32_t x, std::int32_t z, int dy, unsigned size) {
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(x), static_cast<unsigned long>(z), dy, size);
}

void LinkAtSc(unsigned size) {
    const unsigned char* const s = ScMagicS29();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    Link(ts->x, ts->z, 2, size);
}

void Project3(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers3)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Xy(p, 8), Xy(p, 0x18), Xy(p, 0x28), &depth);
    MH_CALL(Gte_PrimDepths3_10B)(p);
}

void Project4(unsigned char* p) {
    long depth;
    MH_CALL(Gte_RotTransPers4)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x18), Xy(p, 8), Xy(p, 0x18),
                               Xy(p, 0x28), Xy(p, 0x38), &depth);
    MH_CALL(Gte_PrimDepths4_10B)(p);
}

// Three colour bytes from the scratch words' low bytes (red, green, blue).
void Shade3(unsigned char* p) {
    p[0] = SB(::bof3::Img(cell::kW6));
    p[1] = SB(::bof3::Img(cell::kW8));
    p[2] = SB(::bof3::Img(cell::kWA));
}

void Fill3(unsigned char* p, unsigned char v) { p[0] = p[1] = p[2] = v; }

using AllocFn = unsigned char (__cdecl*)();

void CallOwn(std::uint32_t address) { MH_WORD(Handler, address)(); }

}  // namespace

#define S29_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The colours both beam draws take: word 0x903856 / 58 / 5A = the signed
// colour byte +0x5D / +0x5E / +0x5F times the brightness +9.
void BeamColours() {
    const unsigned char* const s = ScMagicS29();
    Put(::bof3::Img(cell::kW6), MulMagicS29(static_cast<signed char>(s[0x5D]), s[9]));
    Put(::bof3::Img(cell::kW8), MulMagicS29(static_cast<signed char>(s[0x5E]), s[9]));
    Put(::bof3::Img(cell::kWA), MulMagicS29(static_cast<signed char>(s[0x5F]), s[9]));
}

// A mote's angle: ((+0xB & 7) x 8 + +9) << 6, a word.
std::uint16_t MoteAngleMagicS29(const unsigned char* s) {
    return static_cast<std::uint16_t>(((s[0xB] & 7u) * 8 + s[9]) << 6);
}

}  // namespace

// original 0x4DE8C0: the kind-2 task. A two-entry stack table by +1 -
// Icebreath_Start, BattleFx_Finish - then draw mode 0x35, the mote pool run
// through IcePool_Dispatch, draw mode 0x15.
S28_EXPORT void __cdecl Icebreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Icebreath_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Icebreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
    DrawMode(0x35);
    RunPool(::bof3::Img(BreathMote_Pool_addr), kMotes, MH_CALL(IcePool_Dispatch));
    DrawMode(0x15);
}

// original 0x4DE980: bytes +0..+2 of every mote cleared; the task placed as
// the beam's aim places it (the centre's screen point in the scratch +0xA /
// +0xC, the caster's moved by the offset into +0x2E / +0x30 and +6 / +8, +0x14
// the angle from there to the centre); +0xB 0, then 90 motes (+0x80 the task,
// +1 i & 1, +9 (i / 4) x 3 + 1) counted in +0xB; sound 0x102; the target
// flags 0x10; +9 0, +1 on. The pool's answer is used unchecked.
S28_EXPORT void __cdecl Icebreath_Start(void) {
    ClearPool(::bof3::Img(BreathMote_Pool_addr), kMotes);
    Sc()[8] = Owner()[8];
    MH_CALL(MagicFx_CenterOnSide)();
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetSW(0xA, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
    SetSW(0xC, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    CopyOwnerPosition();
    MH_CALL(BattleActor_UpdateScreenXY)();
    OffsetByDirection();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_z));
    const short x = S16(Sc() + 0x2E), y = S16(Sc() + 0x30);
    SetSW(6, static_cast<unsigned short>(x));
    SetSW(8, static_cast<unsigned short>(y));
    const int angle = Ratan2(SS(0xA) - x, SS(0xC) - y);
    rv::Store(rv::Current<records::BattleTask>()->step_y, angle);
    Sc()[0xB] = 0;
    for (unsigned i = 0; i < kMotes; ++i) {
        const unsigned k = MH_CALL(IcePool_Alloc)() & 0xFFu;
        unsigned char* const self = Sc();
        unsigned char* const rec = Record(::bof3::Img(BreathMote_Pool_addr), k);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        trec->owner = static_cast<std::int32_t>(Addr(self));
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
S28_EXPORT void __cdecl IcePool_Dispatch(void) { CallCell("IcePool_Dispatch", ::bof3::Img(IcePool_Kinds_addr), Sc()[1], 2); }

// original 0x4DEBA0: a jmp through BreathMote_Steps (four entries: Launch,
// Fly, Burst, Free) by +2.
S28_EXPORT void __cdecl BreathMote_Task(void) { CallCell("BreathMote_Task", ::bof3::Img(BreathMote_Steps_addr), Sc()[2], 4); }

// original 0x4DEBC0: +9 down; at 0 the mote starts at the owner's screen point
// (+0x2E / +0x30), its heading the owner's +0x14 plus or minus Rand & 0xFF
// (Rand's bit 0 says which); +3 Rand & 7, +0xC Rand & 7 + 8 (the speed), +0xB
// Rand (the phase), +9 Rand & 0xF + 8 (the flight), +0xA 2 (the size), +2 on.
S28_EXPORT void __cdecl BreathMote_Launch(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_y));
    if (MH_CALL(Rand)() & 1) {
        const int r = MH_CALL(Rand)();
        rv::Store(rv::Current<records::BattleTask>()->step_y, (r & 0xFF) + rv::As<records::SpriteHead>(Owner())->step_y);
    } else {
        const int r = MH_CALL(Rand)();
        rv::Store(rv::Current<records::BattleTask>()->step_y, rv::As<records::SpriteHead>(Owner())->step_y - (r & 0xFF));
    }
    Sc()[3] = U8(MH_CALL(Rand)() & 7);
    const int speed = MH_CALL(Rand)();
    rv::Store(rv::Current<records::BattleTask>()->step_x, (speed & 7) + 8);
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
    SetSW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->step_x));
    s = Sin(static_cast<short>(angle));
    SetWord(px, Word(px) + static_cast<unsigned>(Sar(Mul(s, SS(0)), 12)));
    unsigned char* const py = Sc() + 0x30;
    s = Cos(SS(4));
    SetWord(py, Word(py) + static_cast<unsigned>(Sar(Mul(s, SS(0)), 12)));
    sc = Sc();
    SetSW(6, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_x));
    SetSW(8, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_y));
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
        MH_AT(VoidFn, bof3::addr::MagicFx_FreeCurrentRecord)();
        return;
    }
    MH_CALL(BreathMote_DrawBurst)();
}

// original 0x4DEE40: the owner's +0xB down and the record freed.
S28_EXPORT void __cdecl BreathMote_Free(void) {
    Dec(Owner()[0xB]);
    MH_AT(VoidFn, bof3::addr::MagicFx_FreeCurrentRecord)();
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
        SetSW(6, static_cast<unsigned>(Sar(Mul(s, SS(0)), 12)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
        s = Cos(a);
        SetSW(8, static_cast<unsigned>(Sar(Mul(s, SS(0)), 12)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
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
S28_EXPORT unsigned char __cdecl IcePool_Alloc(void) { return Alloc(::bof3::Img(BreathMote_Pool_addr), kMotes); }

// original 0x4DF4B0: the kind-2 task. A four-entry stack table by +1 -
// Thunderbreath_Start, MAGIC226's count-down to the target flags 0x10,
// Thunderbreath_Wait, BattleFx_Finish - then draw mode 0x35, the bolt pool
// run through ThunderPool_Dispatch, the orb, draw mode 0x15.
S28_EXPORT void __cdecl Thunderbreath_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Thunderbreath_Start, bof3::addr::MagicFx_CountDownFlag10,
                                                 bof3::addr::Thunderbreath_Wait, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Thunderbreath_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
    DrawMode(0x35);
    RunPool(::bof3::Img(ThunderPool_addr), kBolts, MH_CALL(ThunderPool_Dispatch));
    MH_CALL(Thunderbreath_DrawOrb)();
    DrawMode(0x15);
}

// original 0x4DF580: bytes +0..+2 of every bolt record cleared; the task at
// the owner's direction and position, its screen point moved by the formation
// offset (+0xC / +0x10); +0xB 0, +9 8, +0xA 0x10, +1 on; 32 bolts (+0x80 the
// task, +1 0, +4 i, +0xB 0, +9 i x 4 + 1) counted in +0xB; sound 0x100. The
// pool's answer is used unchecked.
S28_EXPORT void __cdecl Thunderbreath_Start(void) {
    ClearPool(::bof3::Img(ThunderPool_addr), kBolts);
    Sc()[8] = Owner()[8];
    CopyOwnerPosition();
    MH_CALL(BattleActor_UpdateScreenXY)();
    MH_CALL(MagicFx_FormationOffset)();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_z));
    Sc()[0xB] = 0;
    Sc()[9] = 8;
    Sc()[0xA] = 0x10;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 0x20; ++i) {
        const unsigned k = MH_CALL(ThunderPool_Alloc)() & 0xFFu;
        unsigned char* const self = Sc();
        unsigned char* const rec = Record(::bof3::Img(ThunderPool_addr), k);
        records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
        trec->owner = static_cast<std::int32_t>(Addr(self));
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
S28_EXPORT void __cdecl ThunderPool_Dispatch(void) { CallCell("ThunderPool_Dispatch", ::bof3::Img(ThunderPool_Kinds_addr), Sc()[1], 2); }

// original 0x4DF850: a call through ThunderBolt_Steps (four entries: Aim,
// Grow, Flicker, Fade) by +2; then, while +0 and +2 are set, +0xB up and the
// bolt drawn; every fourth frame of the flicker step (+2 1) with +0xA set, a
// spark (a pool record, kind 1, +0x80 the owner, at the bolt's end point, the
// scratch +8 / +0xA) counted in the owner's +0xB, unless the pool is full.
S28_EXPORT void __cdecl ThunderBolt_Task(void) {
    CallCell("ThunderBolt_Task", ::bof3::Img(ThunderBolt_Steps_addr), Sc()[2], 4);
    if (Sc()[0] == 0 || Sc()[2] == 0) return;
    Inc(Sc()[0xB]);
    MH_CALL(ThunderBolt_Draw)();
    const unsigned char* const sc = Sc();
    if ((sc[0xB] & 3) != 0 || sc[2] != 1 || sc[0xA] == 0) return;
    const unsigned char k = MH_CALL(ThunderPool_Alloc)();
    if (k == 0xFF) return;
    unsigned char* const rec = Record(::bof3::Img(ThunderPool_addr), k);
    records::BattleTask* const trec = rv::As<records::BattleTask>(rec);
    const std::uint16_t x = SW(8);
    unsigned char* const owner = Owner();
    trec->owner = static_cast<std::int32_t>(Addr(owner));
    rec[1] = 1;
    rv::Store<unsigned>(trec->screen_x, x);
    rv::Store<unsigned>(trec->screen_y, SW(0xA));
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
    SetSW(0xC, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
    SetSW(0xE, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_x));
    SetSW(8, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Owner())->screen_y));
    SetSW(0xA, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    const int angle = Ratan2(SS(0xC) - SS(8), SS(0xE) - SS(0xA));
    rv::Store(rv::Current<records::BattleTask>()->step_y, angle);
    unsigned char* const heading = Sc() + 0x14;
    const bool odd = (Sc()[4] & 1) != 0;
    const int r = MH_CALL(Rand)() & 7;
    SetLong(heading, Long(heading) + Shl(odd ? r : -r, 6));
    rv::Current<records::BattleTask>()->step_x = 0;
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
    rv::Store(rv::Current<records::BattleTask>()->step_x, rv::Current<records::BattleTask>()->step_x + 1);
    if (rv::Current<records::BattleTask>()->step_x == 0x10) Inc(Sc()[2]);
}

// original 0x4DFA90: as the flicker, then +9 down by 2; at 0 the owner's +0xB
// down and the record freed (MAGIC219's 0x4F6290).
S28_EXPORT void __cdecl ThunderBolt_Fade(void) {
    const int s = Sin((Sc()[0xB] & 0x3F) << 6);
    Sc()[0xA] = U8(Sar(Shl(s, 2), 12) + 0x10);
    Sc()[9] = U8(Sc()[9] - 2);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_AT(VoidFn, bof3::addr::MagicFx_FreeCurrentRecord)();
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
        SetSW(0xC, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_x));
        SetVW(0, static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_x));
        SetSW(0xE, Word(sc + 0x30));
        SetVW(2, Word(sc + 0x30));
        phase = sc[0xB];
        bend = static_cast<short>(Word(sc + 0x14) - 0x400);
    }
    int s = Sin((phase & 0x1F) << 7);
    int across = static_cast<short>(Sar(Shl(s, 2), 12));
    s = Sin(bend);
    SetSW(8, static_cast<unsigned>(Sar(Mul(s, across), 12) + Long(Mem(::bof3::Img(Prim_VertexScratch_addr)))));
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
        s = Sin(rv::Current<records::BattleTask>()->step_y);
        SetVW(0, VW(0) + static_cast<unsigned>(Sar(Shl(s, 4), 12)));
        s = Cos(rv::Current<records::BattleTask>()->step_y);
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
        SetSW(8, static_cast<unsigned>(Sar(Mul(s, across), 12) + Long(Mem(::bof3::Img(Prim_VertexScratch_addr)))));
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
    CallCell("ThunderSpark_Task", ::bof3::Img(ThunderSpark_Steps_addr), Sc()[2], 3);
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
    MH_AT(VoidFn, bof3::addr::MagicFx_FreeCurrentRecord)();
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
S28_EXPORT unsigned char __cdecl ThunderPool_Alloc(void) { return Alloc(::bof3::Img(ThunderPool_addr), kBolts); }

// original 0x4E0910: the kind-2 task. A two-entry stack table by +1
// (DivineBreath_Start, BattleFx_Finish), unchecked; then each live record of
// DivineMote_Pool run as Sprite_Current, its +0x80 the owner, through
// DivineMote_Task - Sprite_Current and the owner as they were after the
// phase put back after each.
S29_EXPORT void __cdecl DivineBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::DivineBreath_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = ScMagicS29()[1];
    if (phase >= 2) PastTable("DivineBreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const saved = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        if ((m[0] & 1) == 0) continue;
        const std::int32_t its = static_cast<std::int32_t>(tm->owner);
        Sprite_Current = m;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        CallOwn(bof3::addr::DivineMote_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = saved;
    }
}

// original 0x4E0990: the pool emptied (bytes 0..2 of each record); the task
// at the side's centre (MagicFx_CenterOnSide); +0xB 0, +1 on; two kind-1
// children 0x5C, each its +0x80 this task: the beam (+1 0, +9 1) and the
// burst (+1 1, +9 9), +0xB counting them (the slot index unchecked); sound
// 0x100.
S29_EXPORT void __cdecl DivineBreath_Start(void) {
    for (unsigned i = 0; i < cell::kMotes; ++i) {
        unsigned char* const m = Mote(i);
        m[0] = 0;
        m[1] = 0;
        m[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    ScMagicS29()[0xB] = 0;
    IncMagicS29(ScMagicS29()[1]);
    static constexpr unsigned char kKind[2] = {0, 1};
    static constexpr unsigned char kDelay[2] = {1, 9};
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned slot = NewTask(0x5C);
        unsigned char* const self = ScMagicS29();
        unsigned char* const child = TaskSlot(slot);
        SetPtr(child + 0x80, self);
        child[1] = kKind[k];
        child[9] = kDelay[k];
        IncMagicS29(self[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4E0A60: the children's kind-1 task, a jmp through
// DivineBreathFx_Kinds (two entries: DivineBeam_Run, DivineBurst_Run) by +1,
// unchecked.
S29_EXPORT void __cdecl DivineBreathFx_Task(void) {
    const unsigned phase = ScMagicS29()[1];
    if (phase >= 2) PastTable("DivineBreathFx_Task", phase, 2);
    DataPhase(::bof3::Img(DivineBreathFx_Kinds_addr), phase)();
}

// original 0x4E0A80: DivineBeam_Steps (three) by +2, unchecked; then while
// +0 and +2 are set: on even frames three new colour bytes +0x5D..+0x5F
// ((Rand & 3) + 4 each), the actor's matrix pushed, the disc and the wall
// drawn, the matrix popped.
S29_EXPORT void __cdecl DivineBeam_Run(void) {
    const unsigned phase = ScMagicS29()[2];
    if (phase >= 3) PastTable("DivineBeam_Run", phase, 3);
    DataPhase(::bof3::Img(DivineBeam_Steps_addr), phase)();
    const unsigned char* const s = ScMagicS29();
    if (s[0] == 0 || s[2] == 0) return;
    if ((Frame_Counter & 1) == 0) {
        for (unsigned k = 0x5D; k <= 0x5F; ++k) {
            const std::uint32_t r = RandCall();
            ScMagicS29()[k] = static_cast<unsigned char>((r & 3) + 4);
        }
    }
    MH_CALL(MagicFx_PushActorMatrix)();
    CallOwn(bof3::addr::DivineBeam_DrawDisc);
    CallOwn(bof3::addr::DivineBeam_DrawWall);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E0B00: +9 down; at 0 the beam starts at the owner's point, its
// height 0x4000000 above; three colour bytes (Rand & 3) + 4; radius +0x14
// 0x100, +9 0x10, +0xA 8, +2 on.
S29_EXPORT void __cdecl DivineBeam_Wait(void) {
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(OwnerMagicS29())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(OwnerMagicS29())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, Add(rv::As<records::SpriteHead>(OwnerMagicS29())->height, 0x4000000));
    for (unsigned k = 0x5D; k <= 0x5F; ++k) {
        const std::uint32_t r = RandCall();
        ScMagicS29()[k] = static_cast<unsigned char>((r & 3) + 4);
    }
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x100);
    ScMagicS29()[9] = 0x10;
    ScMagicS29()[0xA] = 8;
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E0BB0: the height down 0x800000; +0xA down, at 0 +2 on.
S29_EXPORT void __cdecl DivineBeam_Descend(void) {
    rv::Store(rv::Current<records::BattleTask>()->height, Add(rv::Current<records::BattleTask>()->height, static_cast<std::int32_t>(0xFF800000u)));
    DecMagicS29(ScMagicS29()[0xA]);
    if (ScMagicS29()[0xA] == 0) IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E0BE0: the radius +0x14 up 0x20; +9 down; at 0 the owner's
// +0xB down and the task freed.
S29_EXPORT void __cdecl DivineBeam_Widen(void) {
    rv::Store(rv::Current<records::BattleTask>()->step_y, Add(rv::Current<records::BattleTask>()->step_y, 0x20));
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    DecMagicS29(OwnerMagicS29()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E0C20: the beam's disc. Gpu_SetDrawMode(.., 0x35) and
// Gfx_CommitPrim(5, 0xC); the radius the word +0x14; sixteen semi-transparent
// POLY_G3 of the centre and two rim points 0x100 apart (sin / cos x radius
// >> 12), the centre in the colours, the rim 0x10 grey, each
// Gfx_CommitPrim(5, 0x34); then the draw mode 0x15.
S29_EXPORT void __cdecl DivineBeam_DrawDisc(void) {
    DrawModeMagicS29(0x35);
    CommitMagicS29(5, 0xC);
    Put(::bof3::Img(cell::kR), static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_y));
    {
        const int s = SinMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
    }
    {
        const int c = CosMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))));
    }
    BeamColours();
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        const std::uint16_t y = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2);
        const std::uint16_t x = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x10);
        Put(::bof3::Img(Prim_VertexScratch_addr), 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, x);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, y);
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
        }
        const int c = CosMagicS29(a);
        const int ny = Mul12(c, SSMagicS29(::bof3::Img(cell::kR)));
        unsigned char* const p = Gfx_PacketNext;
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, ny);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Project3(p);
        Shade3(p + 4);
        Fill3(p + 0x14, 0x10);
        Fill3(p + 0x24, 0x10);
        CommitMagicS29(5, 0x34);
    }
    DrawModeMagicS29(0x15);
    CommitMagicS29(5, 0xC);
}

// original 0x4E0E00: the beam's wall. The radius the word +0x14; 32
// semi-transparent POLY_G4, each between two rim points 0x80 apart at height
// -0x300 and the same two at 0, the top dark (1), the foot in the colours:
// each after Gpu_SetDrawMode(.., 0x35) linked by MapView_LinkPrimAt at the
// task's point plus the new rim point << 9 (0, 0xC), and again (0, 0x44).
S29_EXPORT void __cdecl DivineBeam_DrawWall(void) {
    Put(::bof3::Img(cell::kR), static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_y));
    {
        const int s = SinMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
    }
    {
        const int c = CosMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))));
    }
    BeamColours();
    for (int a = 0x80; a < 0x1080; a += 0x80) {
        const std::uint16_t y = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2);
        const std::uint16_t x = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
        Put(::bof3::Img(Prim_VertexScratch_addr), x);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, y);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 4, -0x300);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10, x);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, y);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))));
        }
        const int c = CosMagicS29(a);
        const int ny = Mul12(c, SSMagicS29(::bof3::Img(cell::kR)));
        const std::uint16_t nx = SWMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, -0x300);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18, nx);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
        const unsigned char* const s = ScMagicS29();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, ny);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, ny);
        const std::int32_t lx = Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<short>(nx)) << 9),
                                    ts->x);
        const std::int32_t lz = Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<short>(ny)) << 9),
                                    ts->z);
        DrawModeMagicS29(0x35);
        Link(lx, lz, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Fill3(p + 4, 1);
        Fill3(p + 0x14, 1);
        Shade3(p + 0x24);
        Shade3(p + 0x34);
        Project4(p);
        Link(lx, lz, 0, 0x44);
    }
}

// original 0x4E1030: DivineBurst_Steps (three) by +2, unchecked; then while
// +0 and +2 are set the wall drawn under the actor's matrix.
S29_EXPORT void __cdecl DivineBurst_Run(void) {
    const unsigned phase = ScMagicS29()[2];
    if (phase >= 3) PastTable("DivineBurst_Run", phase, 3);
    DataPhase(::bof3::Img(DivineBurst_Steps_addr), phase)();
    const unsigned char* const s = ScMagicS29();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    CallOwn(bof3::addr::DivineBeam_DrawWall);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4E1070: +9 down; at 0 the burst at the owner's point, colour
// bytes 0xF, radius 0x100, its step +0x20 0x21, +9 0x10, +0xA 0x1E, +2 on.
S29_EXPORT void __cdecl DivineBurst_Wait(void) {
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(OwnerMagicS29())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(OwnerMagicS29())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(OwnerMagicS29())->height);
    ScMagicS29()[0x5D] = 0xF;
    ScMagicS29()[0x5E] = 0xF;
    ScMagicS29()[0x5F] = 0xF;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x100);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 0x21);
    ScMagicS29()[9] = 0x10;
    ScMagicS29()[0xA] = 0x1E;
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E1110: the step +0x20 down 2 unless it is 1; the radius +0x14
// less it; once the radius is under 0x30 (signed): the targets' flag 0x10,
// sound 0x101, 64 motes (DivineMote_Alloc, the index unchecked), each its
// +0x80 the owner, +1 0, +0xB its number i, +9 (Rand & 7) + 8 (i / 6) + 1,
// counted in the owner's +0xB; +2 on.
S29_EXPORT void __cdecl DivineBurst_Shrink(void) {
    {
        unsigned char* const s = ScMagicS29();
        if (rv::As<records::BattleTask>(s)->work_20 != 1) rv::Store(rv::As<records::BattleTask>(s)->work_20, Sub(rv::As<records::BattleTask>(s)->work_20, 2));
    }
    {
        unsigned char* const s = ScMagicS29();
        rv::Store(rv::As<records::BattleTask>(s)->step_y, Sub(rv::As<records::BattleTask>(s)->step_y, rv::As<records::BattleTask>(s)->work_20));
    }
    if (rv::Current<records::BattleTask>()->step_y >= 0x30) return;
    MH_CALL(Battle_SetTargetFlags)(TargetByteMagicS29(), 0x10);
    MH_CALL(Sound_PlayById)(0x101);
    for (unsigned i = 0; i < 0x40; ++i) {
        const unsigned slot = MH_AT(AllocFn, bof3::addr::DivineMote_Alloc)();
        unsigned char* const m = Mote(slot);
        records::BattleTask* const tm = rv::As<records::BattleTask>(m);
        tm->owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
        m[1] = 0;
        m[0xB] = static_cast<unsigned char>(i);
        const std::uint32_t r = RandCall();
        m[9] = static_cast<unsigned char>((r & 7) + (i / 6) * 8 + 1);
        IncMagicS29(OwnerMagicS29()[0xB]);
    }
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E11F0: the radius down one while over 8 (signed); on odd frames
// +9 down, at 0 the owner's +0xB down and the task freed.
S29_EXPORT void __cdecl DivineBurst_Fade(void) {
    {
        unsigned char* const s = ScMagicS29();
        if (rv::As<records::BattleTask>(s)->step_y > 8) rv::Store(rv::As<records::BattleTask>(s)->step_y, Sub(rv::As<records::BattleTask>(s)->step_y, 1));
    }
    if ((Frame_Counter & 1) == 0) return;
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    DecMagicS29(OwnerMagicS29()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E1240: a mote, a jmp through DivineMote_TaskTable (one entry,
// DivineMote_Run) by +1, unchecked.
S29_EXPORT void __cdecl DivineMote_Task(void) {
    const unsigned phase = ScMagicS29()[1];
    if (phase >= 1) PastTable("DivineMote_Task", phase, 1);
    DataPhase(::bof3::Img(DivineMote_TaskTable_addr), phase)();
}

// original 0x4E1260: DivineMote_Steps (two) by +2, unchecked; then while +0
// and +2 are set: draw mode 0x35 linked at the mote (2, 0xC), its screen
// point (BattleActor_UpdateScreenXY), its star and ring, draw mode 0x15
// linked the same.
S29_EXPORT void __cdecl DivineMote_Run(void) {
    const unsigned phase = ScMagicS29()[2];
    if (phase >= 2) PastTable("DivineMote_Run", phase, 2);
    DataPhase(::bof3::Img(DivineMote_Steps_addr), phase)();
    const unsigned char* const s = ScMagicS29();
    if (s[0] == 0 || s[2] == 0) return;
    DrawModeMagicS29(0x35);
    LinkAtSc(0xC);
    MH_CALL(BattleActor_UpdateScreenXY)();
    CallOwn(bof3::addr::DivineMote_DrawStar);
    CallOwn(bof3::addr::DivineMote_DrawRing);
    DrawModeMagicS29(0x15);
    LinkAtSc(0xC);
}

// original 0x4E12F0: +9 down; at 0 the mote starts: distance +0xC 0x5000, its
// step +0x18 0x200, rise +0x14 0x80000, its step +0x20 0x40000; the point
// the owner's plus sin / cos of its angle x the distance >> 12, the owner's
// height; colour bytes (Rand & 7) + 5; +9 0, +0xA 0x18, +2 on.
S29_EXPORT void __cdecl DivineMote_Launch(void) {
    DecMagicS29(ScMagicS29()[9]);
    if (ScMagicS29()[9] != 0) return;
    {
        unsigned char* const s = ScMagicS29();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_x = 0x5000;
        ts->work_18 = 0x200;
        rv::Store(ts->step_y, 0x80000);
        rv::Store(ts->work_20, 0x40000);
        Put(::bof3::Img(cell::kR), static_cast<std::uint16_t>(ts->step_x));
        Put(::bof3::Img(cell::kW4), MoteAngleMagicS29(s));
    }
    {
        const int sn = SinMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
        const std::int32_t v = Add(Mul12(sn, SSMagicS29(::bof3::Img(cell::kR))), rv::As<records::SpriteHead>(OwnerMagicS29())->x);
        rv::Current<records::BattleTask>()->x = v;
    }
    {
        const int c = CosMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
        const std::int32_t v = Add(Mul12(c, SSMagicS29(::bof3::Img(cell::kR))), rv::As<records::SpriteHead>(OwnerMagicS29())->z);
        rv::Current<records::BattleTask>()->z = v;
    }
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(OwnerMagicS29())->height);
    for (unsigned k = 0x5D; k <= 0x5F; ++k) {
        const std::uint32_t r = RandCall();
        ScMagicS29()[k] = static_cast<unsigned char>((r & 7) + 5);
    }
    ScMagicS29()[9] = 0;
    ScMagicS29()[0xA] = 0x18;
    IncMagicS29(ScMagicS29()[2]);
}

// original 0x4E1430: +9 up; the distance +0xC less its step +0x18, the rise
// +0x14 plus its step +0x20; the point moved by sin / cos of the angle x the
// distance >> 12 (each into the record the pointer held before the call), the
// height up the rise; at distance 0 the owner's +0xB down and the record
// freed (0x4F6290, MAGIC219's).
S29_EXPORT void __cdecl DivineMote_Fly(void) {
    IncMagicS29(ScMagicS29()[9]);
    {
        unsigned char* const s = ScMagicS29();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_x = Sub(ts->step_x, ts->work_18);
    }
    {
        unsigned char* const s = ScMagicS29();
        rv::Store(rv::As<records::BattleTask>(s)->step_y, Add(rv::As<records::BattleTask>(s)->step_y, rv::As<records::BattleTask>(s)->work_20));
    }
    unsigned char* x = nullptr;
    {
        unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kR), static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_x));
        x = s + 0x34;
        Put(::bof3::Img(cell::kW4), MoteAngleMagicS29(s));
    }
    {
        const int sn = SinMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
        SetLong(x, Add(Long(x), Mul12(sn, SSMagicS29(::bof3::Img(cell::kR)))));
    }
    unsigned char* const z = ScMagicS29() + 0x38;
    {
        const int c = CosMagicS29(SSMagicS29(::bof3::Img(cell::kW4)));
        SetLong(z, Add(Long(z), Mul12(c, SSMagicS29(::bof3::Img(cell::kR)))));
    }
    {
        unsigned char* const s = ScMagicS29();
        rv::Store(rv::As<records::BattleTask>(s)->height, Add(rv::As<records::BattleTask>(s)->height, rv::As<records::BattleTask>(s)->step_y));
    }
    if (rv::Current<records::BattleTask>()->step_x != 0) return;
    DecMagicS29(OwnerMagicS29()[0xB]);
    MH_AT(Handler, bof3::addr::MagicFx_FreeCurrentRecord)();
}

// original 0x4E1510: the mote's star in screen space: radius +0xA; eight
// semi-transparent POLY_G3 of the screen point (+0x2E, +0x30) and two rim
// points 0x200 apart (sin / cos x radius >> 12 plus the point, as words),
// the centre 0xF0 grey, the rim the colour bytes << 4; each linked at the
// mote (2, 0x34).
S29_EXPORT void __cdecl DivineMote_DrawStar(void) {
    Put(::bof3::Img(cell::kR), ScMagicS29()[0xA]);
    {
        const int s = SinMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr), Mul12(s, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
    }
    {
        const int c = CosMagicS29(0);
        Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    }
    {
        const unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kW6), MulMagicS29(static_cast<signed char>(s[0x5D]), 16));
        Put(::bof3::Img(cell::kW8), MulMagicS29(static_cast<signed char>(s[0x5E]), 16));
        Put(::bof3::Img(cell::kWA), MulMagicS29(static_cast<signed char>(s[0x5F]), 16));
    }
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const unsigned char* const s = ScMagicS29();
            PutFloat(p + 8, S16MagicS29(s + 0x2E));
            PutFloat(p + 0xC, S16MagicS29(s + 0x30));
            PutFloat(p + 0x18, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr)));
            PutFloat(p + 0x1C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 2));
        }
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr), Mul12(s, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
            PutFloat(p + 0x28, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr)));
        }
        {
            const int c = CosMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
            Fill3(p + 4, 0xF0);
            PutFloat(p + 0x2C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 2));
            Shade3(p + 0x14);
            Shade3(p + 0x24);
        }
        LinkAtSc(0x34);
    }
}

// original 0x4E16E0: the mote's ring in screen space: radii +0xA and twice
// it; eight semi-transparent POLY_G4 between the two circles, 0x200 apart,
// the outer edge dark (1), the inner in the colour words the star left; each
// linked at the mote (2, 0x44).
S29_EXPORT void __cdecl DivineMote_DrawRing(void) {
    {
        const unsigned char* const s = ScMagicS29();
        Put(::bof3::Img(cell::kR), s[0xA]);
        Put(::bof3::Img(cell::kR2), s[0xA] * 2);
    }
    // the rim points: v0 on the outer circle, v1 on the inner, at angle a
    const auto rim = [](int a, std::uint32_t radius, std::uint32_t v) {
        {
            const int s = SinMagicS29(a);
            Put(v, Mul12(s, SSMagicS29(radius)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
        }
        const int c = CosMagicS29(a);
        Put(v + 2, Mul12(c, SSMagicS29(radius)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
    };
    rim(0, ::bof3::Img(cell::kR2), ::bof3::Img(Prim_VertexScratch_addr));
    rim(0, ::bof3::Img(cell::kR), ::bof3::Img(cell::kV1));
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr)));
        PutFloat(p + 0xC, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 2));
        PutFloat(p + 0x28, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
        PutFloat(p + 0x2C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr), Mul12(s, SSMagicS29(::bof3::Img(cell::kR2))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
            PutFloat(p + 0x18, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr)));
        }
        {
            const int c = CosMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR2))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
            PutFloat(p + 0x1C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 2));
        }
        {
            const int s = SinMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Mul12(s, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
            PutFloat(p + 0x38, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8));
        }
        {
            const int c = CosMagicS29(a);
            Put(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Mul12(c, SSMagicS29(::bof3::Img(cell::kR))) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
            Fill3(p + 4, 1);
            Fill3(p + 0x14, 1);
            PutFloat(p + 0x3C, SSMagicS29(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2));
            Shade3(p + 0x24);
            Shade3(p + 0x34);
        }
        LinkAtSc(0x44);
    }
}

// original 0x4E1940: the first free record of DivineMote_Pool (+0 bit 0
// clear) taken (bit 0 set); its index, or 0xFF when all 64 are in use.
S29_EXPORT unsigned char __cdecl DivineMote_Alloc(void) {
    for (unsigned n = 0; n < cell::kMotes; ++n) {
        unsigned char* const m = Mote(n);
        if (m[0] & 1) continue;
        m[0] |= 1;
        return static_cast<unsigned char>(n);
    }
    return 0xFF;
}

void BreathMote_Inject() {
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
    BOF3_INJECT(DivineBreath_Task);
    BOF3_INJECT(DivineBreath_Start);
    BOF3_INJECT(DivineBreathFx_Task);
    BOF3_INJECT(DivineBeam_Run);
    BOF3_INJECT(DivineBeam_Wait);
    BOF3_INJECT(DivineBeam_Descend);
    BOF3_INJECT(DivineBeam_Widen);
    BOF3_INJECT(DivineBeam_DrawDisc);
    BOF3_INJECT(DivineBeam_DrawWall);
    BOF3_INJECT(DivineBurst_Run);
    BOF3_INJECT(DivineBurst_Wait);
    BOF3_INJECT(DivineBurst_Shrink);
    BOF3_INJECT(DivineBurst_Fade);
    BOF3_INJECT(DivineMote_Task);
    BOF3_INJECT(DivineMote_Run);
    BOF3_INJECT(DivineMote_Launch);
    BOF3_INJECT(DivineMote_Fly);
    BOF3_INJECT(DivineMote_DrawStar);
    BOF3_INJECT(DivineMote_DrawRing);
    BOF3_INJECT(DivineMote_Alloc);
}
