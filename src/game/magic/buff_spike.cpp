// Moved here by tools/move_funcs.py from magic_s18.cpp (docs/source-layout-round.md).
#include "game/magic/buff_spike.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Handler;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::SetWord;

using move_script::Word;

// The two blocks' objects (docs/battle_actions.md): the acting block's +8 and
// the target block's +8 (the harness's kSource).
// kActorObject: now MagicS23_ActorSprite (symbols.toml).
// kTargetObject: now BattleOdds_ActorObject (symbols.toml).
// The acting block's command kind, and the action's id (docs/battle_actions.md).
constexpr ::bof3::image::Va kCommandKind{0x904B35};

constexpr ::bof3::image::Va kActionId{0x904B80};

// The effect library's (queue group L, docs/magic_s18.md section 5), by name:
// MagicFx_LinkByDepth (through LinkDepthsFn: the depths as const long *),
// MagicFx_ApplyBuff.
using LinkDepthsFn = void (__cdecl*)(unsigned long, unsigned long, const long*, unsigned char*, unsigned, unsigned, unsigned);

// The GTE calls as the originals push them: one pointer more than
// symbols.toml's prototypes carry (a flag word the callee may write).
using Rtp3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                               long*, long*);

using Avg3Fn = long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*);

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

unsigned char* SC() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Slot(unsigned char k) { return Mem(::bof3::Img(BattleTask_Slots_addr) + k * at::kTaskStride); }

std::int32_t Ptr(const unsigned char* p) { return static_cast<std::int32_t>(Low32(p)); }

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::int32_t AddL(std::int32_t a, std::uint32_t b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + b); }

// The scratch words SharedScratch + k (the PSX scratchpad's) and the vertex
// scratch Prim_VertexScratch + k: three SVECTORs at +0, +8, +0x10, +0x18.
constexpr ::bof3::image::Va kDs{SharedScratch_addr};

// kVx: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch
short Ds(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(kDs.v) + k))); }

void SetDs(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kDs.v) + k), v); }

short Vx(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k))); }

void SetVx(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v); }

const short* VxP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

// `imul` (a 32-bit product that wraps) then `sar n`; `shl l` then `sar r`.
int MulSar(int a, int b, int n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

int ShlSar(int a, int l, int r) { return static_cast<int>(static_cast<std::uint32_t>(a) << l) >> r; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
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

void LinkAt(unsigned dy, unsigned size) {
    const unsigned char* const sc = SC();
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z),
                                static_cast<int>(dy), size);
}

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

}  // namespace

#define MS18_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The spikes' shared frame: the actor's screen point, a disc of radius
// `base` + Rand() & 3 (MagicFx_DrawDiscRadius), and the spike itself under the
// actor matrix.
void SpikeFrame(unsigned base) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    const int r = MH_CALL(Rand)();
    MH_CALL(MagicFx_DrawDiscRadius)(static_cast<unsigned>((r & 3) + static_cast<int>(base)));
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(BuffSpike_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// +9 up on odd frames; the speed +0x14 plus the acceleration +0x20, and the
// height word +0x3E plus the speed's low word; +0xA down, answering whether
// it reached 0.
bool SpikeFly() {
    if (Frame_Counter & 1) Bump(SC()[9]);
    unsigned char* sc = SC();
    rv::Store(rv::As<records::BattleTask>(sc)->step_y, AddL(rv::As<records::BattleTask>(sc)->step_y, static_cast<std::uint32_t>(rv::As<records::BattleTask>(sc)->work_20)));
    sc = SC();
    rv::Store<unsigned>(rv::As<records::BattleTask>(sc)->height_whole, rv::As<records::BattleTask>(sc)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->step_y));
    Drop(SC()[0xA]);
    return SC()[0xA] == 0;
}

// One of BuffSpike_Draw's two rows: four flat triangles from +9 to +9 + 0x20
// by 8, each from the centre to two points of radius 0x30 at angles
// (a & 31) << 7 and ((a + 8) & 31) << 7, the centre `height` above (the
// first row +0x50, the second the scratch word +2 negated), coloured by
// `colors`[3 * (i + 4 * +4)]; each triangle's depth into `depths` and the
// packet cursor moved past it (0x34).
void SpikeRow(const unsigned char* colors, bool first, long (&depths)[4]) {
    unsigned char* sc = SC();
    int a = sc[9];
    if (a >= sc[9] + 0x20) return;
    if (first) {
        SetDs(2, 0x50);
        SetDs(0, 0x30);
    }
    do {
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
        if (k < 0 || k > 3) bof3::Fatal("BuffSpike_Draw: depth index %d, past the four-entry array", k);
        depths[k] = depth;
        for (unsigned q : {4u, 0x14u, 0x24u}) {
            p[q] = static_cast<unsigned char>(Ds(8));
            p[q + 1] = static_cast<unsigned char>(Ds(0xA));
            p[q + 2] = static_cast<unsigned char>(Ds(0xC));
        }
        Gfx_PacketNext = Gfx_PacketNext + 0x34;
        sc = SC();
    } while (a < sc[9] + 0x20);
}

}  // namespace

// original 0x4C01F0: the kind-2 task. Its phase +1 through a six-entry stack
// table: Buff_Start, BattleFx_TintActor, BattleFx_Brighten (round eight's,
// MAGIC062's), Buff_WaitChildren, Buff_Fade, 0x4E5200 (MAGIC131's: the done
// flag and free once +0xB is 0). Unchecked in the original; ours aborts.
MS18_EXPORT void __cdecl Buff_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {
        bof3::addr::Buff_Start,        bof3::addr::BattleFx_TintActor, bof3::addr::BattleFx_Brighten,
        bof3::addr::Buff_WaitChildren, bof3::addr::Buff_Fade,          bof3::addr::MagicFx_EndWhenChildrenDone};
    CallPhase(kPhases, 6, "Buff_Task");
}

// original 0x4C0240: the target's object's +8 and position to the task; +4
// the kind (Buff_Kind); +0xB 0, +9 0x10, the phase on. Then five children
// BattleTask_Create(1, 5), this task's +0xB counting them: the ring (+1 0,
// +4 the kind, +9 0, the task's position) and four spikes (+1 1, +4 the kind,
// +0xB 0 / 8 / 0x10 / 0x18, +9 that + 1, +0xA the task's +9). The CLUT strip
// 0x80E980 (16 + 16 words) to 0x812980, Gfx_ClutStripDirty 1,
// Sound_PlayById(0x100).
MS18_EXPORT void __cdecl Buff_Start(void) {
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    SC()[8] = src[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    const unsigned char kind = MH_CALL(Buff_Kind)();
    SC()[4] = kind;
    SC()[0xB] = 0;
    SC()[9] = 0x10;
    Bump(SC()[1]);
    {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 5));
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        unsigned char* const sc = SC();
        records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
        tt->owner = Ptr(sc);
        t[1] = 0;
        t[4] = sc[4];
        t[9] = 0;
        tt->x = tsc->x;
        tt->z = tsc->z;
        rv::Store(tt->height, tsc->height);
        Bump(sc[0xB]);
    }
    for (unsigned b = 0; b < 0x20; b += 8) {
        unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 5));
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        unsigned char* const sc = SC();
        tt->owner = Ptr(sc);
        t[1] = 1;
        t[4] = sc[4];
        t[0xB] = static_cast<unsigned char>(b);
        t[9] = static_cast<unsigned char>(b + 1);
        t[0xA] = sc[9];
        Bump(sc[0xB]);
    }
    for (unsigned i = 0; i < 0x20; i += 2) {
        SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + i), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400 + i)));
        SetWord(Mem(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400 + 0x20 + i), Word(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3400 + 0x20 + i)));
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C03B0: once +0xB (the children running) is 1 or 0, +9 8 and on.
MS18_EXPORT void __cdecl Buff_WaitChildren(void) {
    if (SC()[0xB] > 1) return;
    SC()[9] = 8;
    Bump(SC()[1]);
}

// original 0x4C03D0: the tint record MoveScript_TintRecords[+0xA]'s three
// colour bytes (+2, +3, +4) down one; +9 down. At 0: Sprite_ReleaseTint on
// the target's object, BattleActor_Flash(the target), and a last child
// BattleTask_Create(1, 0x48) with +4 the kind when 0x4FB6F0(the kind's byte
// of 0x65C39C, the target) answers non-zero, else 8; +9 1, +0xA 0; +0xB up;
// the phase on.
MS18_EXPORT void __cdecl Buff_Fade(void) {
    unsigned char* const sc = SC();
    unsigned char* const tints = MoveScript_TintRecords;
    Drop(tints[sc[0xA] * 12u + 2]);
    Drop(tints[sc[0xA] * 12u + 3]);
    Drop(tints[sc[0xA] * 12u + 4]);
    Drop(sc[9]);
    if (SC()[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    MH_CALL(BattleActor_Flash)(Mem(::bof3::Img(at::kTarget))[0]);
    const unsigned char target = Mem(::bof3::Img(at::kTarget))[0];
    const unsigned char byte = Mem(::bof3::Img(MagicFx_BuffStats_addr))[SC()[4]];
    const unsigned char suits = MH_CALL(MagicFx_ApplyBuff)(byte, target);
    unsigned char* const t = Slot(MH_CALL(BattleTask_Create)(1, 0x48));
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    unsigned char* const owner = SC();
    tt->owner = Ptr(owner);
    t[4] = suits != 0 ? owner[4] : 8;
    t[9] = 1;
    t[0xA] = 0;
    Bump(owner[0xB]);
    Bump(SC()[1]);
}

// original 0x4C04F0: the effect's kind by the action. The acting block's
// command kind 4 (an ability): by the id 0x904B80, 0x52 / 0xB8 answer 0,
// 0x54 / 0xBA 1, 0x22 / 0x23 / 0x55 / 0xBB 2, any other 3 (a jump table of
// four cases by a byte table over 0x22..0xBB). Any other kind: 0x10B becomes
// 0x52 and answers 0, 0x211 becomes 0x54 and 1, 0x14 and 0x117 become 0x55
// and 2, any other 3. Only the low word of 0x904B80 is read.
MS18_EXPORT unsigned char __cdecl Buff_Kind(void) {
    const unsigned id = Word(Mem(::bof3::Img(kActionId.v)));
    if (Mem(::bof3::Img(kCommandKind.v))[0] == 4) {
        switch (id) {
        case 0x52: case 0xB8: return 0;
        case 0x54: case 0xBA: return 1;
        case 0x22: case 0x23: case 0x55: case 0xBB: return 2;
        default: return 3;
        }
    }
    switch (id) {
    case 0x10B: SetWord(Mem(::bof3::Img(kActionId.v)), 0x52); return 0;
    case 0x211: SetWord(Mem(::bof3::Img(kActionId.v)), 0x54); return 1;
    case 0x14: case 0x117: SetWord(Mem(::bof3::Img(kActionId.v)), 0x55); return 2;
    default: return 3;
    }
}

// original 0x4C0620: the children (kind 1, parameter 5): jmp [BuffFx_Types +
// 4 * +1] - the ring, the spikes.
MS18_EXPORT void __cdecl BuffFx_Dispatch(void) { CallCell(BuffFx_Types, 2, SC()[1], "BuffFx_Dispatch"); }

// original 0x4C0640: the ring - its three phases (0x4C2D10 MAGIC086's,
// BuffRing_Wait, 0x4B1740 MAGIC060's); while live (+0), the band and the disc
// under the actor matrix.
MS18_EXPORT void __cdecl BuffRing_Task(void) {
    CallCell(BuffRing_Phases, 3, SC()[2], "BuffRing_Task");
    if (SC()[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(BuffRing_DrawBand)();
    MH_CALL(BuffRing_DrawDisc)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C0680: on once the owner's +0xB is 2 or less.
MS18_EXPORT void __cdecl BuffRing_Wait(void) {
    if (Owner()[0xB] <= 2) Bump(SC()[2]);
}

// original 0x4C06A0: a flat disc of sixteen semi-transparent Gouraud
// triangles (tpage 0x35), radius 0x80 (sin / cos << 7 >> 12); the centre
// +9 * 5 grey, the rim BuffRing_DiscColors[+4] times +9.
MS18_EXPORT void __cdecl BuffRing_DrawDisc(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const sc = SC();
        const unsigned char* const rgb = BuffRing_DiscColors + sc[4] * 3u;
        SetDs(6, sc[9] * 5u);
        SetDs(8, rgb[0] * static_cast<unsigned>(sc[9]));
        SetDs(0xA, rgb[1] * static_cast<unsigned>(sc[9]));
        SetDs(0xC, rgb[2] * static_cast<unsigned>(sc[9]));
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
        p[4] = p[5] = p[6] = static_cast<unsigned char>(Ds(6));
        for (unsigned k : {0x14u, 0x24u}) {
            p[k] = static_cast<unsigned char>(Ds(8));
            p[k + 1] = static_cast<unsigned char>(Ds(0xA));
            p[k + 2] = static_cast<unsigned char>(Ds(0xC));
        }
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4C08A0: a flat band of sixteen semi-transparent Gouraud quads
// (tpage 0x35) from radius 0x80 to 0x100; the inner edge
// BuffRing_BandColors[+4] times +9, the outer 1 1 1.
MS18_EXPORT void __cdecl BuffRing_DrawBand(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
    {
        const unsigned char* const sc = SC();
        const unsigned char* const rgb = BuffRing_BandColors + sc[4] * 3u;
        SetDs(8, rgb[0] * static_cast<unsigned>(sc[9]));
        SetDs(0xA, rgb[1] * static_cast<unsigned>(sc[9]));
        SetDs(0xC, rgb[2] * static_cast<unsigned>(sc[9]));
    }
    int r = Sin(0);
    SetVx(8, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Cos(0);
    SetVx(0xA, static_cast<unsigned>(ShlSar(r, 7, 12)));
    r = Sin(0);
    SetVx(0x18, static_cast<unsigned>(ShlSar(r, 8, 12)));
    r = Cos(0);
    SetVx(0x1A, static_cast<unsigned>(ShlSar(r, 8, 12)));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        SetVx(0, static_cast<unsigned>(Vx(8)));
        SetVx(2, static_cast<unsigned>(Vx(0xA)));
        SetVx(0x10, static_cast<unsigned>(Vx(0x18)));
        SetVx(0x12, static_cast<unsigned>(Vx(0x1A)));
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
            p[k] = static_cast<unsigned char>(Ds(8));
            p[k + 1] = static_cast<unsigned char>(Ds(0xA));
            p[k + 2] = static_cast<unsigned char>(Ds(0xC));
        }
        for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// original 0x4C0AF0: a spike - jmp [BuffSpike_Phases + 4 * +2] (six:
// 0x4BDD20 MAGIC078's, 0x4C1EB0 MAGIC083's, then the four below).
MS18_EXPORT void __cdecl BuffSpike_Dispatch(void) { CallCell(BuffSpike_Phases, 6, SC()[2], "BuffSpike_Dispatch"); }

// original 0x4C0B10: the spike's frame (disc 0x18); flies; at +0xA 0, the
// speed -16, the acceleration 4, +0xA 8, on.
MS18_EXPORT void __cdecl BuffSpike_Arc(void) {
    SpikeFrame(0x18);
    if (!SpikeFly()) return;
    rv::Store(rv::Current<records::BattleTask>()->step_y, -16);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 4);
    SC()[0xA] = 8;
    Bump(SC()[2]);
}

// original 0x4C0BB0: the same frame and flight; at +0xA 0, +0xA 0x1C - +0xB
// and on.
MS18_EXPORT void __cdecl BuffSpike_Arc2(void) {
    SpikeFrame(0x18);
    if (!SpikeFly()) return;
    unsigned char* const sc = SC();
    sc[0xA] = static_cast<unsigned char>(0x1C - sc[0xB]);
    Bump(SC()[2]);
}

// original 0x4C0C40: the frame (disc 0x40 once the spin +0xC is 4 and +0xA
// below 5, else 0x18); the spin up one every eighth frame of +0x10 until 4;
// +9 (the angle) plus the spin. At spin 4, +0xA down; at 0,
// Sound_PlayById(0x101) for the first spike (+0xB 0), +0x10 +0x14 +0x20 8,
// on.
MS18_EXPORT void __cdecl BuffSpike_Spin(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* sc = SC();
    const bool wide = rv::As<records::BattleTask>(sc)->step_x == 4 && sc[0xA] < 5;
    const int r = MH_CALL(Rand)();
    MH_CALL(MagicFx_DrawDiscRadius)(static_cast<unsigned>((r & 3) + (wide ? 0x40 : 0x18)));
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(BuffSpike_Draw)();
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

// original 0x4C0D20: the frame (disc 0x20); +0x10 up 4; +9 plus the spin; +0xB
// up 2; the spike at the owner's x / z plus sin / cos((+0xB & 31) << 7) times
// (+0xA * +0x10 + 0xB0) >> 3; +0xA up; at 0x10, the owner's +0xB down and
// freed.
MS18_EXPORT void __cdecl BuffSpike_Orbit(void) {
    SpikeFrame(0x20);
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

// original 0x4C0E30: a spike (tpage 0x15, linked at the task's x / z, +2,
// 0xC), two rows of four triangles - up from BuffSpike_ColorsUp, down from
// BuffSpike_ColorsDown - each row's four depths and first packet to
// 0x4FB880(x, z, depths, packets, 4, 0x34, 2), the effect library's.
MS18_EXPORT void __cdecl BuffSpike_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    LinkAt(2, 0xC);
    long depths[4] = {};
    unsigned char* const up = Gfx_PacketNext;
    SpikeRow(BuffSpike_ColorsUp, true, depths);
    {
        const unsigned char* const sc = SC();
        const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
        MH_AT(LinkDepthsFn, bof3::addr::MagicFx_LinkByDepth)(static_cast<unsigned long>(tsc->x),
                                                             static_cast<unsigned long>(tsc->z), depths, up, 4,
                                                             0x34, 2);
    }
    unsigned char* const down = Gfx_PacketNext;
    SpikeRow(BuffSpike_ColorsDown, false, depths);
    const unsigned char* const sc = SC();
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_AT(LinkDepthsFn, bof3::addr::MagicFx_LinkByDepth)(static_cast<unsigned long>(tsc->x),
                                                         static_cast<unsigned long>(tsc->z), depths, down, 4,
                                                         0x34, 2);
}

// original 0x4C12F0 (also reached from MAGIC083): a flat disc of eight
// semi-transparent Gouraud triangles on screen (tpage 0x35), at the task's
// screen point (+0x2E / +0x30), radius the argument's low word; the centre
// 0x80 grey, the rim 1 1 1; each linked at the task's x / z, +2 (0xC for the
// mode, 0x34 for the triangle). MagicFx_DrawDisc's shape with the radius
// passed in.
MS18_EXPORT void __cdecl MagicFx_DrawDiscRadius(unsigned radius) {
    SetDs(0, radius);
    int a = 0;
    do {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        LinkAt(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, static_cast<short>(static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)));
        PutFloat(p + 0xC, static_cast<short>(static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)));
        int r = Sin(a);
        PutFloat(p + 0x18, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(static_cast<short>(static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)))));
        r = Cos(a);
        PutFloat(p + 0x1C, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(static_cast<short>(static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)))));
        a += 0x200;
        r = Sin(a);
        PutFloat(p + 0x28, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(static_cast<short>(static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)))));
        r = Cos(a);
        PutFloat(p + 0x2C, static_cast<int>(static_cast<std::uint32_t>(MulSar(r, Ds(0), 12)) +
                                            static_cast<std::uint32_t>(static_cast<short>(static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)))));
        p[4] = p[5] = p[6] = 0x80;
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        LinkAt(2, 0x34);
    } while (a < 0x1000);
}

void BuffSpike_Inject() {
    BOF3_INJECT(Buff_Task);
    BOF3_INJECT(Buff_Start);
    BOF3_INJECT(Buff_WaitChildren);
    BOF3_INJECT(Buff_Fade);
    BOF3_INJECT(Buff_Kind);
    BOF3_INJECT(BuffFx_Dispatch);
    BOF3_INJECT(BuffRing_Task);
    BOF3_INJECT(BuffRing_Wait);
    BOF3_INJECT(BuffRing_DrawDisc);
    BOF3_INJECT(BuffRing_DrawBand);
    BOF3_INJECT(BuffSpike_Dispatch);
    BOF3_INJECT(BuffSpike_Arc);
    BOF3_INJECT(BuffSpike_Arc2);
    BOF3_INJECT(BuffSpike_Spin);
    BOF3_INJECT(BuffSpike_Orbit);
    BOF3_INJECT(BuffSpike_Draw);
    BOF3_INJECT(MagicFx_DrawDiscRadius);
}
