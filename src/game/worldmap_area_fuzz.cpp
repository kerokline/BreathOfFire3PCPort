// BOF3X_SHADOW=worldmap_area: a differential fuzz of group DA's thirty
// functions against byte-copies of Capcom's, once at start-up.
// docs/worldmap_area.md section 7.
//
// Thirty byte-copies, every call and tail jmp out re-aimed at a recording
// stand-in (bof3::CloneCall with the callee each site was read to reach -
// the calls among the thirty included, so each function is tested alone),
// and the seven .data tables the dispatches read in place swapped for
// recorders and put back: WorldMap33_PlateStates, WorldMapHud_States,
// WorldMapHud_BoxStates, the code slots +0 / +0xC / +0x10 of the twelve
// WorldMap_Records (the eleventh's "none" row included), EffectKind06_States,
// EffectKind06_Ticks and the first five EffectKind18_States. One round: one
// function, random bytes in every region any of them touches, the pointers
// put back inside them, then that function's boundaries seeded; theirs, then
// from the same state ours; the regions and the stand-ins' log compared.
//
// The stand-ins are louder than the real callees: any call may move
// Sprite_Current between the fuzz's two objects, a field of the object, the
// map's mode byte, Field_Request, the two flag words, the place word, the
// message state, Frame_Counter, Draw_PassFlags, Game_Mode, Cond_ByteFA or
// the leader's +0x134 - so a value ours keeps where the original reads memory
// again (or the other way round) shows. The ones that answer a byte answer
// garbage above it.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/worldmap_area_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace worldmap_area {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using U = std::uint32_t;
U Address(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

U g_rng = 0x44415F31u;
U Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

constexpr unsigned kLog = 2048;   // WorldMap33_DrawDrift: up to 17 x 16 cells, five calls each
struct Entry { U what, a, b, c, d, e; };
Entry g_log[kLog];
unsigned g_log_n;
U g_seed;            // the stand-ins' own stream: the same on both passes
int g_rand_first = -1, g_rand_pending = -1;   // the seeding's answer for the round's first Rand, or -1
int g_cell_first = -1, g_cell_pending = -1;   // the same for AreaMap_ByteAt's al

U Hash() {
    U h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(U what, U a = 0, U b = 0, U c = 0, U d = 0, U e = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e};
    ++g_log_n;
}

// --- the fuzz's own memory -------------------------------------------------

constexpr unsigned kObjectBytes = 0x80;       // an Effect_Objects record
constexpr unsigned kPacketBytes = 0x200;
constexpr unsigned kItemBytes = 0x48;         // a DrawItems half
alignas(16) unsigned char g_objects[2][kObjectBytes];
alignas(16) unsigned char g_packets[kPacketBytes];
alignas(16) unsigned char g_items[4][kItemBytes];

unsigned char* Cur() { return Sprite_Current; }
U Cur32() { return Address(Sprite_Current); }

// The packet cursor moved on by `size`, kept inside the fuzz's buffer (both
// passes wrap it the same way: it is a function of the log position).
void Advance(U size) {
    U next = Address(Gfx_PacketNext) + size;
    if (next + 0x50 > Address(g_packets) + kPacketBytes) next = Address(g_packets) + (Hash() % 4) * 4;
    Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}

// Every cell below is one some function reads again after a call.
void Disturb() {
    const U h = Hash();
    if (h % 3 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const auto b = static_cast<unsigned char>(h >> 20);
    switch ((h >> 4) % 16) {
    case 0: Sprite_Current = g_objects[v & 1]; break;
    case 1: case 2: case 3: {
        static const unsigned kFields[] = {1, 2, 3, 5, 6, 7, 9, 0xB, 0x18, 0x19, 0x30, 0x31, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x40, 0x42};
        // The dispatch bytes stay inside the smallest table that reads them
        // after a call: +1 below 2, +3 below 4, +6 below 6.
        const unsigned f = kFields[v % 20];
        Cur()[f] = f == 1 ? b % 2 : f == 3 ? b % 4 : f == 6 ? b % 6 : b;
        break;
    }
    case 4: At(at::kMapMode)[0] = static_cast<unsigned char>(v % 4 == 0 ? b : v % 3); break;
    case 5: Field_Request = static_cast<unsigned char>(v % 3 == 0 ? 2 : v % 3 == 1 ? 5 : b); break;
    case 6: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ 0x100); break;
    case 7: Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000); break;
    case 8: SetWord(At(at::kPlace), b); break;
    case 9: At(at::kMsgState)[0] = static_cast<unsigned char>(v % 3); break;
    case 10: Frame_Counter = h >> 6; break;
    case 11: Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags ^ 4); break;
    case 12: Game_Mode = static_cast<unsigned short>(Game_Mode ^ 1); break;
    case 13: Cond_ByteFA = static_cast<signed char>(v % 16); break;
    case 14: SetLong(At(at::kLeaderEdge), static_cast<std::int32_t>(h)); break;
    default: break;
    }
}

// --- the stand-ins ---------------------------------------------------------

int __cdecl StubRand() {
    Record(1);
    Disturb();
    const U h = Hash();
    if (g_rand_pending >= 0) {
        const int v = g_rand_pending;
        g_rand_pending = -1;
        return static_cast<int>((h >> 8) & 0x7F00u) | v;
    }
    return h % 4 == 0 ? static_cast<int>(h) : static_cast<int>(h >> 1) & 0x7FFF;
}
long __cdecl StubElevation(long x, long z) {
    Record(2, static_cast<U>(x), static_cast<U>(z));
    Disturb();
    return static_cast<long>(Hash());
}
void __cdecl StubSetCell(int x, int z, int value) {
    Record(3, static_cast<U>(x) & 0xFFFF, static_cast<U>(z) & 0xFFFF, static_cast<U>(value) & 0xFF);
    Disturb();
}
void __cdecl StubSet40() { Record(4); Disturb(); }
void __cdecl StubClear40() { Record(5); Disturb(); }
void __cdecl StubOpenScript(unsigned id) { Record(6, id & 0xFFFF); Disturb(); }
// AreaMap_ByteAt: the cell in al (the kinds the plate tests, mostly),
// garbage above; the words it is handed are its arguments' low halves.
unsigned __cdecl StubByteAt(unsigned x, unsigned z) {
    Record(7, x & 0xFFFF, z & 0xFFFF);
    Disturb();
    const U h = Hash();
    U al;
    if (g_cell_pending >= 0) {
        al = static_cast<U>(g_cell_pending);
        g_cell_pending = -1;
    } else {
        static const U kCells[] = {0xA1, 0xA0, 0xAE, 0xA2, 0x9F, 0xAF, 0, 0x21};
        al = h % 4 == 0 ? (h >> 8) & 0xFF : kCells[(h >> 4) % 8];
    }
    return (h & 0xFFFFFF00u) | al;
}
void __cdecl StubSetAnimation(unsigned animation) { Record(8, animation & 0xFF, Cur32()); Disturb(); }
template <unsigned N> void __cdecl StubVoid() { Record(N, Cur32(), Cur()[1]); Disturb(); }
void __cdecl StubDrawHud(int x, int y) { Record(14, static_cast<U>(x), static_cast<U>(y) & 0xFFFF); Disturb(); }
unsigned __cdecl StubRecordIndex() {
    Record(15);
    Disturb();
    const U h = Hash();
    return (h & 0xFFFFFF00u) | ((h >> 8) % 12);
}
unsigned __cdecl StubSetBank(unsigned bank) {
    Record(16, bank & 0xFFFF, Cur32());
    Disturb();
    return Hash();
}
unsigned __cdecl StubScriptTick() {
    Record(17, Cur32());
    Disturb();
    return Hash();
}
// The primitive setters: every byte of the primitive is written, so a store
// the caller makes before the call (where the original makes it after) shows.
void __cdecl StubSetPolyFT4(unsigned char* prim) {
    Record(19, Address(prim));
    for (unsigned i = 0; i < 0x48; i += 4) SetLong(prim + i, static_cast<std::int32_t>(Hash() + i));
    prim[7] = 0x2C;
    Disturb();
}
void __cdecl StubSetShadeTex(unsigned char* prim, unsigned tge) {
    Record(20, Address(prim), tge);
    prim[7] = static_cast<unsigned char>(tge ? prim[7] | 1 : prim[7] & 0xFE);
    Disturb();
}
void __cdecl StubSetSemiTrans(unsigned char* prim, unsigned abe) {
    Record(21, Address(prim), abe);
    prim[7] = static_cast<unsigned char>(abe ? prim[7] | 2 : prim[7] & 0xFD);
    Disturb();
}
// Gte_RotTransPers4 with the ten arguments the original pushes: records the
// four vectors' words and the corner pointers, writes the corners and the two
// locals.
long __cdecl StubRotTransPers4(const short* v0, const short* v1, const short* v2, const short* v3, float* s0, float* s1,
                               float* s2, float* s3, long* p, long* flag) {
    const auto w = [](const short* v, unsigned i) { return static_cast<U>(Word(reinterpret_cast<const unsigned char*>(v) + i * 2)); };
    Record(22, Address(v0), w(v0, 0) | w(v0, 1) << 16, w(v0, 2) | w(v0, 3) << 16, Address(v1), w(v1, 0) | w(v1, 1) << 16);
    Record(23, w(v1, 2) | w(v1, 3) << 16, Address(v2), w(v2, 0) | w(v2, 1) << 16, w(v2, 2) | w(v2, 3) << 16, Address(v3));
    Record(24, w(v3, 0) | w(v3, 1) << 16, w(v3, 2) | w(v3, 3) << 16, Address(s0), Address(s1), Address(s2));
    Record(25, Address(s3), p != nullptr, flag != nullptr, Address(p) != Address(flag));
    float* const corners[] = {s0, s1, s2, s3};
    unsigned k = 0;
    for (float* c : corners) {
        SetLong(reinterpret_cast<unsigned char*>(c), static_cast<std::int32_t>(Hash() + k++));
        SetLong(reinterpret_cast<unsigned char*>(c) + 4, static_cast<std::int32_t>(Hash() + k++));
    }
    *p = static_cast<long>(Hash() ^ 1);
    *flag = static_cast<long>(Hash() ^ 2);
    Disturb();
    return static_cast<long>(Hash());
}
void __cdecl StubPrimDepths(void* prim) {
    Record(26, Address(prim));
    auto* const q = static_cast<unsigned char*>(prim);
    for (unsigned z = 0x10; z <= 0x40; z += 0x10) SetLong(q + z, static_cast<std::int32_t>(Hash() + z));
    Disturb();
}
void __cdecl StubSetTexture(unsigned long texture, unsigned char* prim, int count) {
    Record(27, static_cast<U>(texture), Address(prim), static_cast<U>(count));
    prim[0x16] = static_cast<unsigned char>(texture);
    Disturb();
}
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(28, slot & 0xFF, size & 0xFF, Address(Gfx_PacketNext));
    Advance(size & 0xFF);
    Disturb();
}
// MapView_ItemHalfAt: a third of the time none, else one of the fuzz's four
// items (the caller copies its corners).
unsigned char* __cdecl StubItemHalfAt(long x, long y) {
    Record(29, static_cast<U>(x), static_cast<U>(y));
    Disturb();
    const U h = Hash();
    if (h % 3 == 0) return nullptr;
    return g_items[(h >> 4) % 4];
}
// MapView_LinkPrimAt: the primitive appended - the cursor moved on by the
// size, now and then not (a full row or pool).
void __cdecl StubLinkPrimAt(unsigned long x, unsigned long z, int dy, unsigned size) {
    Record(30, static_cast<U>(x), static_cast<U>(z), static_cast<U>(dy), size, Address(Gfx_PacketNext));
    if (Hash() % 5) Advance(size & 0xFF);
    Disturb();
}
// The .data tables' entries.
template <std::size_t N> void __cdecl StubDataEntry() {
    Record(100 + N, Cur32(), Cur()[1], Cur()[3], Cur()[6]);
    Disturb();
}

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

const Callees kStubs = {
    StubRand,
    StubElevation,
    StubSetCell,
    StubSet40,
    StubClear40,
    As<void (__cdecl*)(unsigned short)>(&StubOpenScript),
    As<unsigned char (__cdecl*)(short, short)>(&StubByteAt),
    As<void (__cdecl*)(unsigned char)>(&StubSetAnimation),
    &StubVoid<9>,    // pin_sprite
    &StubVoid<10>,   // queue_overlay
    &StubVoid<11>,   // effect_release
    &StubVoid<12>,   // frame_step
    &StubVoid<13>,   // box_step
    StubDrawHud,
    StubRecordIndex,
    As<unsigned char (__cdecl*)(unsigned short)>(&StubSetBank),
    As<unsigned char (__cdecl*)()>(&StubScriptTick),
    &StubVoid<18>,   // update_screen
    StubSetPolyFT4,
    StubSetShadeTex,
    StubSetSemiTrans,
    StubRotTransPers4,
    StubPrimDepths,
    StubSetTexture,
    StubCommit,
    StubItemHalfAt,
    StubLinkPrimAt,
};

const void* StubFor(U target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5B93D2: return f(kStubs.rand);
    case 0x5720C0: return f(kStubs.elevation);
    case 0x579F00: return f(kStubs.set_cell);
    case 0x57C7C0: return f(kStubs.flags_set40);
    case 0x57C7A0: return f(kStubs.flags_clear40);
    case 0x4976D0: return f(kStubs.open_script);
    case 0x536700: return f(kStubs.byte_at);
    case 0x5891F0: return f(kStubs.set_animation);
    case 0x4112A0: return f(kStubs.pin_sprite);
    case 0x5890E0: return f(kStubs.queue_overlay);
    case 0x589840: return f(kStubs.effect_release);
    case 0x404160: return f(kStubs.frame_step);
    case 0x404230: return f(kStubs.box_step);
    case 0x404620: return f(kStubs.draw_hud);
    case 0x462A90: return f(kStubs.record_index);
    case 0x589590: return f(kStubs.set_bank);
    case 0x5893A0: return f(kStubs.script_tick);
    case 0x588F20: return f(kStubs.update_screen);
    case 0x5A75D0: return f(kStubs.set_poly_ft4);
    case 0x5A77A0: return f(kStubs.set_shade_tex);
    case 0x5A7780: return f(kStubs.set_semi_trans);
    case 0x5A85F0: return f(kStubs.rot_trans_pers4);
    case 0x5A9290: return f(kStubs.prim_depths);
    case 0x572A00: return f(kStubs.set_texture);
    case 0x461E50: return f(kStubs.commit_prim);
    case 0x572F70: return f(kStubs.item_half_at);
    case 0x572FA0: return f(kStubs.link_prim_at);
    default: bof3::Fatal("worldmap_area: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the thirty copies (capstone, 2026-09-25: each decodes exactly to its
// extent, every jump internal; the calls and tail jmps below are every
// transfer that leaves; the indirect ones read the .data tables swapped
// below) ----------------------------------------------------------------

struct Call { U offset, target; };
struct Clone {
    const char* name;
    U base, size;
    const Call* calls;
    int n_calls;
    const void* ours;
};

// Area29_PickFieldObject's entry is itself a five-byte `jmp 0x4037C0`, which
// CloneOriginal takes for a detour: its copy is of the body the jmp reaches,
// 0x4037C0..0x40387A, the calls 0x10 nearer the start.
constexpr Call kArea29Calls[] = {{0x2, 0x5B93D2}, {0x3D, 0x5B93D2}, {0x88, 0x5720C0}};
constexpr Call kCellsACalls[] = {{0x6, 0x579F00},  {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00},
                                 {0x32, 0x579F00}, {0x3D, 0x579F00}, {0x4B, 0x579F00}, {0x56, 0x579F00}};
constexpr Call kCellsBCalls[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}};
constexpr Call kMessageCalls[] = {{0x18, 0x57C7A0}, {0x33, 0x57C7C0}, {0x6B, 0x4976D0}};
constexpr Call kPlateRunCalls[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr Call kPlateShowCalls[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr Call kGrowCalls[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr Call kHoldCalls[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr Call kShrinkCalls[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr Call kHudFrameCalls[] = {{0x0, 0x404160}, {0x5, 0x404230}};
constexpr Call kSlideInCalls[] = {{0x59, 0x404620}};
constexpr Call kBoxHoldCalls[] = {{0x65, 0x404620}};
constexpr Call kSlideOutCalls[] = {{0x4E, 0x404620}};
constexpr Call kDriftCalls[] = {{0xB7, 0x5A75D0},  {0xBF, 0x5A77A0},  {0x199, 0x5A85F0}, {0x19F, 0x5A9290},
                                {0x1AF, 0x572A00}, {0x1BB, 0x461E50}, {0x21D, 0x572F70}, {0x236, 0x5A75D0},
                                {0x23E, 0x5A77A0}, {0x246, 0x5A7780}, {0x430, 0x572FA0}};
constexpr Call kRecordCalls[] = {{0x0, 0x462A90}};
constexpr Call kStartCalls[] = {{0x2, 0x589590}, {0xC4, 0x5891F0}};
constexpr Call kBlinkCalls[] = {{0x1D, 0x5893A0}, {0x33, 0x5890E0}, {0x38, 0x588F20}};
constexpr Call kEndCalls[] = {{0x1D, 0x589840}};

#define DA_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define DA_C(name, base, size, calls) {#name, base, size, calls, DA_N(calls), reinterpret_cast<const void*>(&::name)}
#define DA_P(name, base, size) {#name, base, size, nullptr, 0, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    DA_C(Area29_PickFieldObject, 0x4037C0, 0xBB, kArea29Calls),
    DA_C(Area33_ClearCellsA, 0x403CE0, 0x5F, kCellsACalls),
    DA_C(Area33_ClearCellsB, 0x403D40, 0x25, kCellsBCalls),
    DA_C(WorldMap33_PlaceMessage, 0x403D70, 0x87, kMessageCalls),
    DA_C(WorldMap33_PlateRun, 0x403E00, 0xD6, kPlateRunCalls),
    DA_C(WorldMap33_PlateShow, 0x403EE0, 0x142, kPlateShowCalls),
    DA_C(WorldMap33_PlateGrow, 0x404030, 0x41, kGrowCalls),
    DA_C(WorldMap33_PlateHold, 0x404080, 0x58, kHoldCalls),
    DA_C(WorldMap33_PlateShrink, 0x4040E0, 0x50, kShrinkCalls),
    DA_P(WorldMapHud_Run, 0x404130, 0x12),
    DA_C(WorldMapHud_Frame, 0x404150, 0xA, kHudFrameCalls),
    DA_P(WorldMapHud_BoxStep, 0x404230, 0x12),
    DA_C(WorldMapHud_BoxSlideIn, 0x404250, 0x62, kSlideInCalls),
    DA_C(WorldMapHud_BoxHold, 0x4042C0, 0x6E, kBoxHoldCalls),
    DA_C(WorldMapHud_BoxSlideOut, 0x404330, 0x57, kSlideOutCalls),
    DA_C(WorldMap33_DrawDrift, 0x4048E0, 0x462, kDriftCalls),
    DA_P(WorldMap_FrameWait, 0x411310, 0x27),
    DA_P(WorldMapHud_BoxWait, 0x414BB0, 0x38),
    DA_P(WorldMapHud_Start, 0x419110, 0x1D),
    DA_P(WorldMap_RecordIndex, 0x462A90, 0x26),
    DA_C(EffectKind00_WorldMap, 0x462AE0, 0x1A, kRecordCalls),
    DA_C(EffectKind58_WorldMap, 0x462B40, 0x1A, kRecordCalls),
    DA_C(WorldMap_RecordHook10, 0x462B80, 0x1A, kRecordCalls),
    DA_P(EffectKind06_Run, 0x469BB0, 0x12),
    DA_C(EffectKind06_Start, 0x469BD0, 0xD7, kStartCalls),
    DA_P(EffectKind06_Tick, 0x469CB0, 0x12),
    DA_C(EffectKind06_Blink, 0x469CD0, 0x3D, kBlinkCalls),
    DA_C(EffectKind06_End, 0x469DB0, 0x22, kEndCalls),
    DA_P(EffectKind18_Run, 0x46D830, 0x12),
    DA_P(EffectKind18_Start, 0x46D850, 0x32),
};
#undef DA_C
#undef DA_P
#undef DA_N
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kArea29, kCellsA, kCellsB, kMessage, kPlateRun, kPlateShow, kGrow, kHold, kShrink, kHudRun, kHudFrame, kBoxStep,
    kSlideIn, kBoxHold, kSlideOut, kDrift, kFrameWait, kBoxWait, kHudStart, kRecordIndex, kKind00, kKind58, kHook10,
    kKind06Run, kKind06Start, kKind06Tick, kBlink, kKind06End, kKind18Run, kKind18Start,
};
static_assert(kKind18Start + 1 == kCount, "the seeding's indices");

// The .data tables swapped for recorders while the fuzz runs, each entry its
// own recorder (100 + its index in g_slot_at): WorldMap33_PlateStates 5,
// WorldMapHud_States 2, WorldMapHud_BoxStates 4, the twelve records' three
// code slots, EffectKind06_States 3, EffectKind06_Ticks 6, EffectKind18_States 5.
constexpr unsigned kSlots = 5 + 2 + 4 + 36 + 3 + 6 + 5;
template <std::size_t... N> constexpr std::array<Handler, sizeof...(N)> MakeSlotStubs(std::index_sequence<N...>) {
    return {{&StubDataEntry<N>...}};
}
constexpr std::array<Handler, kSlots> kSlotStubs = MakeSlotStubs(std::make_index_sequence<kSlots>{});
U g_slot_at[kSlots];
U g_slot_kept[kSlots];
void FillSlots() {
    unsigned n = 0;
    for (U k = 0; k < 5; ++k) g_slot_at[n++] = at::kPlateStates + 4 * k;
    for (U k = 0; k < 2; ++k) g_slot_at[n++] = at::kHudStates + 4 * k;
    for (U k = 0; k < 4; ++k) g_slot_at[n++] = at::kBoxStates + 4 * k;
    for (U r = 0; r < 12; ++r) {
        g_slot_at[n++] = at::kRecords + r * at::kRecordSize;
        g_slot_at[n++] = at::kRecords + r * at::kRecordSize + 0xC;
        g_slot_at[n++] = at::kRecords + r * at::kRecordSize + 0x10;
    }
    for (U k = 0; k < 3; ++k) g_slot_at[n++] = at::kKind06States + 4 * k;
    for (U k = 0; k < 6; ++k) g_slot_at[n++] = at::kKind06Ticks + 4 * k;
    for (U k = 0; k < 5; ++k) g_slot_at[n++] = at::kKind18States + 4 * k;
    if (n != kSlots) bof3::Fatal("worldmap_area: %u table slots, not %u", n, kSlots);
}

// --- the state both passes start from --------------------------------------

struct Region { U at, size; };
Region g_regions[] = {
    {0x937F80, 0x18},                                         // the place word 0x937F82, Sprite_Current, Frame_Counter
    {0, 2 * kObjectBytes},                                    // g_objects (filled in at start-up)
    {at::kFieldObjects, at::kFieldObjectsEnd - at::kFieldObjects},   // the first eight Sprite_Objects
    {at::kLeader, 0x138},                                     // the leader's record to +0x137
    {0x905B80, 4},                                            // Field_EdgeBits
    {at::kArea29Cells, 0x18},                                 // Area29_Cells, Area29_Weights
    {0x9039A0, 4},                                            // Field_ScriptFlags
    {0x9039F0, 8},                                            // the place message's bytes 0x9039F3..0x9039F5
    {0x8034E0, 4},                                            // Cond_ByteFA, Field_StatusBits
    {0x66C7D8, 4},                                            // Field_Request
    {0x66C7E8, 2},                                            // Game_Mode
    {0x905BA4, 2},                                            // Field_ScriptFlags2
    {at::kMapMode, 1},
    {0x904EFC, 2},                                            // Game_AreaNumber
    {0x7E0918, 1},                                            // Draw_PassFlags
    {0x7E0670, 4},                                            // Gfx_PacketNext
    {0, kPacketBytes},                                        // g_packets
    {0x9037A0, 0x20},                                         // Prim_VertexScratch
    {0x8CB580, 4},                                            // AreaMap_Header
    {0x905E60, 8},                                            // Field_Kind2Z, Field_Kind2X
    {0x5EF6B0, 0x10},                                         // WorldMap33_DriftUV and the dwords either side
    {at::kPlateAnims, 0x18},                                  // WorldMap33_PlateAnims
    {0, 4 * kItemBytes},                                      // g_items
};
constexpr unsigned kMaxRegionBytes = 0x1000;

struct State {
    unsigned char memory[kMaxRegionBytes];
    Entry log[kLog];
    unsigned log_n;
    U ret;
};
unsigned g_region_bytes;
void Capture(State& s) {
    unsigned n = 0;
    for (const Region& r : g_regions) { std::memcpy(s.memory + n, At(r.at), r.size); n += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned n = 0;
    for (const Region& r : g_regions) { std::memcpy(At(r.at), s.memory + n, r.size); n += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
    g_rand_pending = g_rand_first;
    g_cell_pending = g_cell_first;
}

unsigned char g_area29_exe[0x18];     // Area29_Cells and _Weights as the exe has them
unsigned char g_plates_exe[0x18];     // WorldMap33_PlateAnims as the exe has it
unsigned char g_drift_exe[0x10];      // 0x5EF6B0..0x5EF6BF as the exe has it

U Pick(const U* v, unsigned n) { return v[Next() % n]; }
#define DA_PICK(...) [] { static const U kV[] = {__VA_ARGS__}; return Pick(kV, sizeof kV / sizeof kV[0]); }()

// Random bytes put back inside what the functions dereference: the current
// object one of the fuzz's two, whose dispatch bytes stay inside the
// smallest table (+1 < 2, +3 < 4, +6 < 6); the packet cursor in the fuzz's
// buffer.
void Fix() {
    Sprite_Current = g_objects[Next() & 1];
    for (auto& o : g_objects) {
        o[1] = static_cast<unsigned char>(Next() % 2);
        o[3] = static_cast<unsigned char>(Next() % 4);
        o[6] = static_cast<unsigned char>(Next() % 6);
    }
    Gfx_PacketNext = g_packets + (Next() % 4) * 4;
    if (Often()) std::memcpy(At(at::kArea29Cells), g_area29_exe, sizeof g_area29_exe);
    if (Often()) std::memcpy(At(at::kPlateAnims), g_plates_exe, sizeof g_plates_exe);
    if (Often()) std::memcpy(At(0x5EF6B0), g_drift_exe, sizeof g_drift_exe);
    if (Often()) SetWord(At(at::kPlace), DA_PICK(8, 0xD, 0x60, 0x17, 0xE, 0x13, 7, 0));
}

// The box's leave test's inputs, at their edges.
void SeedLeave(unsigned char* o) {
    At(at::kMapMode)[0] = static_cast<unsigned char>(Often() ? DA_PICK(0, 0, 1, 2) : Next());
    if (Often()) o[0xB] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
    if (Often()) Field_Request = static_cast<unsigned char>(DA_PICK(0, 2, 5, 1, 3));
    Field_ScriptFlags = static_cast<unsigned short>(Often() ? Field_ScriptFlags & ~0x100u : Field_ScriptFlags | 0x100u);
}

void Seed(unsigned k) {
    unsigned char* const o = Sprite_Current;
    g_rand_first = -1;
    g_cell_first = -1;
    switch (k) {
    case kArea29: {
        // The first roll at a chance's edge (the running sum, or one below),
        // with bits above the 0x3F mask set now and then.
        if (Half()) {
            const unsigned char* const w = At(at::kArea29Weights);
            const unsigned upto = Next() % 9;
            unsigned sum = 0;
            for (unsigned i = 0; i < upto; ++i) sum += w[i];
            const unsigned roll = (sum - (Half() ? 1u : 0u)) & 0x3F;
            g_rand_first = static_cast<int>(roll | (Next() & 0xC0));
        }
        if (Half()) std::memset(At(at::kArea29Weights), Half() ? 0 : 7, 8);   // "none kept" (8) reached
        break;
    }
    case kMessage:
        At(at::kMsgState)[0] = static_cast<unsigned char>(Often() ? DA_PICK(0, 0, 1, 1, 2, 0xFF, 0x80) : Next());
        Cond_ByteFA = static_cast<signed char>(Often() ? DA_PICK(1, 2, 15, 0, 0xFF, 0x80, 0x7F, 8) : Next());
        if (Often()) Field_Request = static_cast<unsigned char>(DA_PICK(2, 2, 0, 5, 3));
        break;
    case kPlateRun:
        o[1] = static_cast<unsigned char>(Next() % 5);
        if (Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (Half()) At(at::kLeaderSteps)[0] = static_cast<unsigned char>(DA_PICK(0, 1, 0xFF, 0x80));
        if (Half()) g_cell_first = static_cast<int>(DA_PICK(0xA1, 0xA0, 0xAE));
        break;
    case kPlateShow: {
        o[0xB] = static_cast<unsigned char>(Often() ? DA_PICK(1, 1, 1, 2, 3, 4, 0, 5) : Next());
        // The place the search looks for, planted in one of the six entries:
        // the search has no bound, so it must find it there.
        const unsigned entry = Next() % 6;
        const unsigned place = Word(At(at::kPlace));
        SetWord(At(at::kPlateAnims + entry * 4), place);
        break;
    }
    case kGrow:
    case kShrink:
        if (Often()) o[9] = static_cast<unsigned char>(DA_PICK(1, 1, 2, 0, 0xFF, 0x80));
        if (Half()) SetLong(o + 0x40, static_cast<std::int32_t>(DA_PICK(0, 0xE000, 0x10000, 0xFFFFE000u, 0x7FFFF000)));
        if (Often()) Field_Request = static_cast<unsigned char>(DA_PICK(5, 5, 2, 0));
        break;
    case kHold:
        Game_Mode = static_cast<unsigned short>(Often() ? DA_PICK(0, 2, 1, 0x101) : Next());
        if (Often()) o[7] = static_cast<unsigned char>(Half() ? 1 : DA_PICK(0, 2, 3, 4));
        if (Often()) o[0xB] = o[7];
        if (Often()) SetLong(o + 0x18, static_cast<std::int32_t>(Word(At(at::kPlace)) | (Half() ? 0 : 0x10000u)));
        if (Often()) Field_Request = static_cast<unsigned char>(DA_PICK(0, 5, 2));
        break;
    case kHudRun:
        o[1] = static_cast<unsigned char>(Next() % 2);
        break;
    case kBoxStep:
        o[3] = static_cast<unsigned char>(Next() % 4);
        break;
    case kSlideIn:
        if (Often()) SetWord(o + 0x30, DA_PICK(0xD1, 0xD2, 0xD3, 0xF0, 0x800A, 0x8009, 0));
        SeedLeave(o);
        break;
    case kBoxHold:
        if (Often()) o[0xB] = 0;
        if (Often()) o[9] = static_cast<unsigned char>(DA_PICK(0x58, 0x59, 0x5A, 0xFF, 0, 0x7F));
        SeedLeave(o);
        break;
    case kSlideOut:
        if (Often()) SetWord(o + 0x30, DA_PICK(0xE5, 0xE6, 0xE7, 0xC8, 0x7FF6, 0x7FF5));
        SeedLeave(o);
        break;
    case kFrameWait:
        At(at::kMapMode)[0] = static_cast<unsigned char>(Often() ? DA_PICK(2, 0, 1, 3, 0x82) : Next());
        if (Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ 0x100);
        break;
    case kBoxWait:
        SeedLeave(o);
        break;
    case kDrift: {
        if (Half()) o[2] = 0;
        if (Often()) Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags | 4);
        o[0xB] = static_cast<unsigned char>(Often() ? DA_PICK(2, 3, 2, 3, 0, 1, 4, 5) : Next());
        const unsigned height = At(at::kMapHeight)[0];
        if (Half()) SetWord(o + 0x3A, height + DA_PICK(8, 9, 7, 0, 0x7FFF));
        // The leader within 25 cells, or just out, in x and in z.
        const auto near = [](unsigned char* cell, unsigned centre) {
            const U d = DA_PICK(0, 25, 26, static_cast<U>(-25), static_cast<U>(-26), 1, 100);
            SetWord(cell, centre + d);
        };
        if (Often()) near(At(at::kLeaderCellX), Word(o + 0x36));
        if (Often()) near(At(at::kLeaderCellZ), Word(o + 0x3A));
        if (Half()) Frame_Counter = DA_PICK(0, 15, 16, 31, 0x2F, 0xFFFFFFFFu);
        break;
    }
    case kRecordIndex:
        Game_AreaNumber = static_cast<unsigned short>(Often() ? DA_PICK(16, 33, 45, 65, 87, 88, 104, 115, 121, 151, 152, 0x121,
                                                                        0x110, 0, 0xFFFF, 29, 0x98)
                                                             : Next());
        break;
    case kKind06Run:
        o[1] = static_cast<unsigned char>(Next() % 3);
        break;
    case kKind06Start:
        if (Often()) o[6] = static_cast<unsigned char>(DA_PICK(3, 3, 0, 1, 2, 4, 5, 0x83, 7));
        break;
    case kKind06Tick:
        o[6] = static_cast<unsigned char>(Next() % 6);
        break;
    case kBlink:
        if (Often()) o[9] = static_cast<unsigned char>(DA_PICK(1, 1, 2, 4, 5, 0, 0x84, 0xFC));
        if (Half()) o[5] = 6;
        break;
    case kKind18Run:
        o[1] = static_cast<unsigned char>(Next() % 5);
        break;
    default:
        break;
    }
}
#undef DA_PICK

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[200];
    unsigned kept_none, plate_kinds[6], drift_drawn, drift_items, index_none, message_opened;
} g_cover;

void Cover(unsigned k, const State& in, const State& out) {
    unsigned n_item = 0;
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
        if (out.log[i].what < 200) ++g_cover.logged[out.log[i].what];
        if (out.log[i].what == 30) ++n_item;
    }
    switch (k) {
    case kArea29: {
        bool elevated = false;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) elevated |= out.log[i].what == 2;
        if (!elevated) ++g_cover.kept_none;
        break;
    }
    case kPlateShow: {
        // The kind the function read: +0xB of the object Sprite_Current held.
        unsigned obj = 0;
        std::memcpy(&obj, in.memory + 8, 4);
        const unsigned char* base = in.memory + 0x18;
        const unsigned kind = obj == Address(g_objects[1]) ? base[kObjectBytes + 0xB] : base[0xB];
        ++g_cover.plate_kinds[kind < 5 ? kind : 5];
        break;
    }
    case kDrift:
        if (out.log_n) ++g_cover.drift_drawn;
        g_cover.drift_items += n_item;
        break;
    case kMessage:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) g_cover.message_opened += out.log[i].what == 6;
        break;
    case kRecordIndex:
        if ((out.ret & 0xFF) == 11) ++g_cover.index_none;
        break;
    default:
        break;
    }
}

using Fn0 = U (__cdecl*)();

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    g_regions[1].at = Address(g_objects);
    g_regions[16].at = Address(g_packets);
    g_regions[22].at = Address(g_items);
    g_region_bytes = 0;
    for (const Region& r : g_regions) g_region_bytes += r.size;
    if (g_region_bytes > kMaxRegionBytes)
        bof3::Fatal("worldmap_area: the regions are %u bytes, the state holds %u", g_region_bytes, kMaxRegionBytes);
    std::memcpy(g_area29_exe, At(at::kArea29Cells), sizeof g_area29_exe);
    std::memcpy(g_plates_exe, At(at::kPlateAnims), sizeof g_plates_exe);
    std::memcpy(g_drift_exe, At(0x5EF6B0), sizeof g_drift_exe);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[12];
        if (c.n_calls > 12) bof3::Fatal("worldmap_area: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (!clones[k]) bof3::Fatal("worldmap_area: CloneOriginal(%s) returned null", c.name);
    }

    // The .data tables: their entries swapped for recorders, put back after.
    FillSlots();
    for (unsigned i = 0; i < kSlots; ++i) {
        g_slot_kept[i] = static_cast<U>(Long(At(g_slot_at[i])));
        SetLong(At(g_slot_at[i]), static_cast<std::int32_t>(Address(reinterpret_cast<const void*>(kSlotStubs[i]))));
    }

    static State saved, input, their_out, our_out;
    Capture(saved);
    g = kStubs;

    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        for (unsigned i = 0; i < g_region_bytes; i += 4) {
            const U v = Next();
            std::memcpy(input.memory + i, &v, g_region_bytes - i < 4 ? g_region_bytes - i : 4);
        }
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        g_seed = Next();
        Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            const U ret = reinterpret_cast<Fn0>(const_cast<void*>(fn))();
            Capture(out);
            out.ret = k == kRecordIndex ? ret : 0;
            if (pass == 0) Cover(k, input, their_out);
        }
        calls += their_out.log_n;
        if (their_out.log_n > kLog)
            bof3::Fatal("worldmap_area: %s made %u calls, the log holds %u", kClones[k].name, their_out.log_n, kLog);
        const bool same = std::memcmp(their_out.memory, our_out.memory, g_region_bytes) == 0 &&
                          their_out.log_n == our_out.log_n &&
                          std::memcmp(their_out.log, our_out.log, sizeof their_out.log) == 0 && their_out.ret == our_out.ret;
        if (!same) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < g_region_bytes && their_out.memory[first] == our_out.memory[first]) ++first;
                unsigned first_call = 0;
                while (first_call < kLog && std::memcmp(&their_out.log[first_call], &our_out.log[first_call], sizeof(Entry)) == 0)
                    ++first_call;
                bof3::Log("shadow      worldmap_area self-test MISMATCH: round %u, %s, log %u / %u (first differing call %u: "
                          "%u / %u), first differing state byte %u of %u, result %08X / %08X",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first_call,
                          first_call < kLog ? their_out.log[first_call].what : 0u,
                          first_call < kLog ? our_out.log[first_call].what : 0u, first, g_region_bytes,
                          static_cast<unsigned>(their_out.ret), static_cast<unsigned>(our_out.ret));
            }
        }
    }
    g = kOriginals;
    Apply(saved);
    for (unsigned i = 0; i < kSlots; ++i) SetLong(At(g_slot_at[i]), static_cast<std::int32_t>(g_slot_kept[i]));

    bof3::Log("shadow      worldmap_area self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the fuzz's two objects, packets and items, the field objects, the leader, the place and "
              "message bytes, the flag words, the mode byte, the vertex scratch, the plate and drift tables and the "
              "stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      worldmap_area: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      worldmap_area coverage: plate states %u / %u / %u / %u / %u, hud states %u / %u, box states "
              "%u / %u / %u / %u, records +0 %u +0xC %u +0x10 %u, kind 6 states %u / %u / %u, ticks %u / %u / %u / %u / "
              "%u / %u, kind 0x18 states %u / %u / %u / %u / %u",
              c.logged[100], c.logged[101], c.logged[102], c.logged[103], c.logged[104], c.logged[105], c.logged[106],
              c.logged[107], c.logged[108], c.logged[109], c.logged[110],
              [&] { unsigned s = 0; for (unsigned r = 0; r < 12; ++r) s += c.logged[111 + 3 * r]; return s; }(),
              [&] { unsigned s = 0; for (unsigned r = 0; r < 12; ++r) s += c.logged[112 + 3 * r]; return s; }(),
              [&] { unsigned s = 0; for (unsigned r = 0; r < 12; ++r) s += c.logged[113 + 3 * r]; return s; }(),
              c.logged[147], c.logged[148], c.logged[149], c.logged[150], c.logged[151], c.logged[152], c.logged[153],
              c.logged[154], c.logged[155], c.logged[156], c.logged[157], c.logged[158], c.logged[159], c.logged[160]);
    bof3::Log("shadow      worldmap_area coverage: area 29 none kept %u, cells set %u, elevations %u; messages opened %u, "
              "flags set %u / cleared %u; cells asked %u; plate kinds shown 0:%u 1:%u 2:%u 3:%u 4:%u other:%u, animations "
              "%u, pins %u, overlays %u, releases %u; hud draws %u, frame steps %u, box steps %u; drift drawn %u, items "
              "%u, commits %u, textures %u; record index none %u; banks %u, ticks %u, screen updates %u",
              c.kept_none, c.logged[3], c.logged[2], c.message_opened, c.logged[4], c.logged[5], c.logged[7],
              c.plate_kinds[0], c.plate_kinds[1], c.plate_kinds[2], c.plate_kinds[3], c.plate_kinds[4], c.plate_kinds[5],
              c.logged[8], c.logged[9], c.logged[10], c.logged[11], c.logged[14], c.logged[12], c.logged[13],
              c.drift_drawn, c.drift_items, c.logged[28], c.logged[27], c.index_none, c.logged[16], c.logged[17],
              c.logged[18]);
    if (bad) bof3::Fatal("the world map's area code differs from the original in %u self-test rounds", bad);
}

}  // namespace worldmap_area
