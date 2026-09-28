// BOF3X_SHADOW=scena_sc15: scenario chapters 15..19 through the scenario
// round's harness (scenario_harness.h), once at start-up. docs/scena_sc15.md
// section 4.
//
// Five Runs under the one shadow name, one per chapter byte (15, 16, 17, 18,
// 19): each chapter's clone table (tools/scenario_rows.py --unit SC15 / SC16 /
// SC17 --clones, held against the group's own reading: the same 89 starts,
// extents, calls and jump tables), every callee they call, their .data
// tables, the regions they write beyond the harness's standard ones, a seed
// per function that puts its switch's steps and each comparison's constants
// in, the arguments of those that take any, and a disturbance of the
// chapters' own cells.
//
// Three functions the harness cannot take as they are, handled here (the
// harness is not edited): Scena15_EnterArea (69 call sites) and Scena15_Run6
// (118) have more than the 64 a harness clone re-aims, so this file copies
// them itself, every site re-aimed at a trampoline that calls the recorder
// the harness stands in for the callee (scena_sc12_fuzz.cpp's way), and hands
// the harness a six-byte jmp to each copy; and Scena17_EndTask never returns
// (a task's loop), so both its sides are called through wrappers that
// __builtin_setjmp, and this file's Task_Sleep stand-in __builtin_longjmps
// back after one to three frames while it is fuzzed.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc15.h"
#include "game/scena_sc15_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc15 {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

template <typename T> std::uint32_t KeyOf(T p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
#define SC15_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define SC15_THEIRS(name) #name, KeyOf(name), KeyOf(name)
#define SC15_RAW(label, address) label, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])

unsigned char* Mem(std::uint32_t a) { return sh::Mem(a); }
unsigned char& B(std::uint32_t a) { return sh::Mem(a)[0]; }
void SetW(std::uint32_t a, unsigned v) { SetWord(Mem(a), v); }
void SetL(std::uint32_t a, std::uint32_t v) { SetLong(Mem(a), static_cast<std::int32_t>(v)); }
template <typename T, unsigned N> T PickOf(const T (&v)[N]) { return v[sh::Next() % N]; }

// The cells (scena_sc15.cpp has their meaning).
constexpr std::uint32_t kState = 0x8034E2, kPart = 0x8034E3, kRun = 0x8034E4, kStep = 0x8034E5, kTimer = 0x8034E6;
constexpr std::uint32_t kByteFD = 0x8034F1;
constexpr std::uint32_t kCounter0 = 0x903848, kCounter3 = 0x90384B;
constexpr std::uint32_t kWait = 0x66C810, kRequest = 0x66C7D8, kArea = 0x904EFC;
constexpr std::uint32_t kEffects = 0x7E11E0;
constexpr std::uint32_t kDistance = 0x903840;
constexpr std::uint32_t kPartyByte = 0x90412C;
constexpr std::uint32_t kShakeShift = 0x6BC740, kShakeOn = 0x6BC741, kKept = 0x6BC742, kSlot = 0x6BC743;
constexpr std::uint32_t kRollLine = 0x6BC744, kRollScroll = 0x6BC746, kRollSkip = 0x6BC747;
constexpr std::uint32_t kInputHeld = 0x7E1BE8, kInputPressed = 0x7E1BEC;
constexpr std::uint32_t kHold = 0x929F12, kElevation = 0x929F1C;
constexpr std::uint32_t kGameStep = 0x66C7EA;
constexpr std::uint32_t kPacketNext = 0x7E0670;
constexpr std::uint32_t kRecordTable = 0x7E0880;   // the pointer Scena15_RecordWord reads

unsigned char* Effect(unsigned slot) { return Mem(kEffects + (slot & 0xFFu) * 0x80u); }

// ================================================================================
// The copies of the functions the harness cannot clone as they are

// The recorder standing in for a callee, on either pass (SH_CALL answers the
// real callee outside ours' pass; the copies' sites must reach the recorder
// on theirs too, as a site the harness re-aims does).
template <typename F> F Stub(F f) {
    return reinterpret_cast<F>(const_cast<void*>(sh::StandIn(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(f)))));
}
template <typename F> F StubAt(std::uint32_t a) { return reinterpret_cast<F>(const_cast<void*>(sh::StandIn(a))); }

unsigned char __cdecl TFlagsTest(const unsigned char* b, unsigned i) { return Stub(Flags_Test)(b, i); }
void __cdecl TFlagsSet(unsigned char* b, unsigned i) { Stub(Flags_Set)(b, i); }
unsigned char __cdecl TPlace(unsigned k, int x, int y, int z, unsigned n, unsigned long t) {
    return Stub(Scena15_PlaceEffect)(k, x, y, z, n, t);
}
void __cdecl TBattle(unsigned n) { Stub(Scena15_BattleSetup)(n); }
unsigned __cdecl TDropIn(unsigned e) { return Stub(Party_DropIn)(e); }
unsigned char __cdecl TFindFree() { return Stub(Effect_FindFree)(); }
unsigned char __cdecl TSpawn(long x, long z, unsigned a, unsigned b, unsigned c) { return Stub(Scena15_SpawnMarker)(x, z, a, b, c); }
void __cdecl TCallA(unsigned n) { Stub(Scenario_CallA)(n); }
int __cdecl TLoadFile(unsigned t) { return Stub(Music_LoadFile)(t); }
void __cdecl TTransition(unsigned char k) { Stub(Transition_Start)(k); }
void __cdecl TLoadDat(int i) { Stub(LoadDatFile)(i); }
void __cdecl TShake(unsigned s) { Stub(Scena15_Shake)(s); }
int __cdecl TLoadDone() { return Stub(File_LoadDone)(); }
void __cdecl TMusicPlay(unsigned t, int f) { Stub(Music_Play)(t, f); }
void __cdecl TPartyPlace(long x, long z, unsigned k) { StubAt<void (__cdecl*)(long, long, unsigned)>(kPartyPlace)(x, z, k); }
void __cdecl TFadeOut(int f) { Stub(Music_FadeOut)(f); }
void __cdecl TFadeOutStop(int f) { Stub(Music_FadeOutStop)(f); }
void __cdecl TStartBattle(unsigned id) { Stub(Field_StartEventBattle)(id); }
void __cdecl TRandomPause() { Stub(Scena15_RandomPause)(); }
void __cdecl TSound(unsigned short id) { Stub(Sound_PlayEffect)(id); }
void __cdecl TChangeArea(unsigned a, int x, int z, unsigned f) { Stub(Field_ChangeArea)(a, x, z, f); }
void __cdecl TMsg(unsigned short id) { Stub(Msg_OpenScript)(id); }
void __cdecl TViewShift() { StubAt<void (__cdecl*)()>(kViewShift)(); }
void __cdecl TStatus80() { StubAt<void (__cdecl*)()>(kStatusBit80)(); }
void __cdecl TClearPrivate() { Stub(Task_ClearPrivate)(); }
void __cdecl TWindowReset() { Stub(Window_ResetAll)(); }
void __cdecl TSleep(int f) { Stub(Task_Sleep)(f); }

struct Tramp { std::uint32_t target; const void* to; };
#define SC15_T(address, fn) {address, reinterpret_cast<const void*>(&fn)}
const Tramp kTramps[] = {
    SC15_T(0x57C140, TFlagsTest), SC15_T(0x57C0F0, TFlagsSet),   SC15_T(0x5685D0, TPlace),       SC15_T(0x568510, TBattle),
    SC15_T(0x531F90, TDropIn),    SC15_T(0x589810, TFindFree),   SC15_T(0x56ADF0, TSpawn),       SC15_T(0x5341A0, TCallA),
    SC15_T(0x587A20, TLoadFile),  SC15_T(0x495040, TTransition), SC15_T(0x454590, TLoadDat),     SC15_T(0x56ADC0, TShake),
    SC15_T(0x454810, TLoadDone),  SC15_T(0x587AE0, TMusicPlay),  SC15_T(0x532ED0, TPartyPlace),  SC15_T(0x587BE0, TFadeOut),
    SC15_T(0x587B40, TFadeOutStop), SC15_T(0x4410B0, TStartBattle), SC15_T(0x56AD80, TRandomPause), SC15_T(0x587740, TSound),
    SC15_T(0x594E00, TChangeArea), SC15_T(0x4976D0, TMsg),       SC15_T(0x423380, TViewShift),   SC15_T(0x56D6F0, TStatus80),
    SC15_T(0x5A99F4, TClearPrivate), SC15_T(0x59E330, TWindowReset), SC15_T(0x5A9949, TSleep),
};
#undef SC15_T

// Scena15_EnterArea: 0x73B bytes, 69 call sites.
constexpr sh::CallSite kCalls567DD0[] = {{0x13, 0x57C140}, {0x28, 0x57C140}, {0x4C, 0x57C140}, {0x60, 0x57C140}, {0x91, 0x57C140}, {0xF6, 0x5685D0}, {0x112, 0x57C140}, {0x126, 0x57C140}, {0x15F, 0x57C140}, {0x16D, 0x568510}, {0x187, 0x57C140}, {0x19B, 0x57C0F0}, {0x1C0, 0x57C140}, {0x1CE, 0x568510}, {0x1E8, 0x57C140}, {0x1FC, 0x57C0F0}, {0x221, 0x57C140}, {0x236, 0x57C0F0}, {0x24F, 0x57C140}, {0x25D, 0x568510}, {0x282, 0x57C140}, {0x297, 0x57C0F0}, {0x2B0, 0x57C140}, {0x2BE, 0x568510}, {0x2E3, 0x57C140}, {0x2F1, 0x568510}, {0x30B, 0x57C140}, {0x319, 0x568510}, {0x33D, 0x57C140}, {0x34B, 0x568510}, {0x365, 0x57C140}, {0x37A, 0x57C0F0}, {0x3A2, 0x57C140}, {0x3B0, 0x568510}, {0x3D5, 0x57C140}, {0x3EA, 0x57C0F0}, {0x403, 0x57C140}, {0x411, 0x568510}, {0x43A, 0x57C140}, {0x448, 0x568510}, {0x464, 0x57C140}, {0x49D, 0x57C140}, {0x4B2, 0x57C0F0}, {0x4B9, 0x531F90}, {0x4CA, 0x57C140}, {0x4DE, 0x57C140}, {0x4EA, 0x589810}, {0x521, 0x56ADF0}, {0x528, 0x5341A0}, {0x52F, 0x531F90}, {0x555, 0x57C140}, {0x56A, 0x57C140}, {0x586, 0x56ADF0}, {0x596, 0x57C140}, {0x5AB, 0x57C140}, {0x5BC, 0x587A20}, {0x5D4, 0x57C140}, {0x5E8, 0x57C140}, {0x5FC, 0x5341A0}, {0x603, 0x531F90}, {0x616, 0x57C140}, {0x63A, 0x57C140}, {0x665, 0x57C140}, {0x671, 0x589810}, {0x698, 0x589810}, {0x6D4, 0x57C140}, {0x6E9, 0x57C140}, {0x704, 0x57C140}, {0x718, 0x57C140}};
// Scena15_Run6: 0xE54 bytes, 118 call sites, the 55-entry jump table at +0xD78.
constexpr sh::CallSite kCalls569F20[] = {{0x34, 0x495040}, {0x52, 0x454590}, {0x60, 0x56ADC0}, {0x6B, 0x454810}, {0x87, 0x587A20}, {0x9C, 0x56ADC0}, {0xB4, 0x589810}, {0xE8, 0x56ADC0}, {0x116, 0x56ADC0}, {0x147, 0x56ADC0}, {0x15F, 0x454810}, {0x18C, 0x5685D0}, {0x1A0, 0x587AE0}, {0x1B5, 0x56ADC0}, {0x1ED, 0x56ADC0}, {0x205, 0x589810}, {0x23F, 0x532ED0}, {0x254, 0x56ADC0}, {0x278, 0x56ADC0}, {0x29D, 0x56ADC0}, {0x2B7, 0x587BE0}, {0x2CC, 0x56ADC0}, {0x2E4, 0x589810}, {0x30D, 0x587B40}, {0x32B, 0x56ADC0}, {0x360, 0x5685D0}, {0x37D, 0x56ADC0}, {0x3A2, 0x4410B0}, {0x3B7, 0x56ADC0}, {0x3C2, 0x454810}, {0x3DB, 0x454590}, {0x3E2, 0x531F90}, {0x3F7, 0x56ADC0}, {0x402, 0x454810}, {0x421, 0x589810}, {0x45D, 0x56ADC0}, {0x47C, 0x56AD80}, {0x486, 0x587740}, {0x4A4, 0x56ADC0}, {0x4C6, 0x56AD80}, {0x4D1, 0x56ADC0}, {0x4F5, 0x56AD80}, {0x500, 0x56ADC0}, {0x516, 0x495040}, {0x525, 0x56AD80}, {0x530, 0x56ADC0}, {0x555, 0x587740}, {0x573, 0x56ADC0}, {0x594, 0x57C0F0}, {0x5AA, 0x594E00}, {0x5CD, 0x56ADC0}, {0x5E7, 0x587A20}, {0x5FF, 0x587740}, {0x61D, 0x56ADC0}, {0x655, 0x56ADC0}, {0x671, 0x587AE0}, {0x694, 0x56ADC0}, {0x6AC, 0x589810}, {0x6E0, 0x56ADC0}, {0x70C, 0x594E00}, {0x728, 0x56ADC0}, {0x745, 0x589810}, {0x781, 0x56ADC0}, {0x7B2, 0x56ADC0}, {0x7CD, 0x495040}, {0x7E2, 0x56ADC0}, {0x804, 0x4976D0}, {0x820, 0x56ADC0}, {0x845, 0x56ADC0}, {0x858, 0x57C0F0}, {0x871, 0x594E00}, {0x894, 0x56ADC0}, {0x8AE, 0x495040}, {0x8C3, 0x56ADC0}, {0x8E5, 0x4976D0}, {0x901, 0x56ADC0}, {0x926, 0x56ADC0}, {0x945, 0x594E00}, {0x968, 0x56ADC0}, {0x989, 0x495040}, {0x99E, 0x56ADC0}, {0x9C0, 0x4976D0}, {0x9DC, 0x56ADC0}, {0xA01, 0x56ADC0}, {0xA15, 0x57C0F0}, {0xA21, 0x57C0F0}, {0xA37, 0x594E00}, {0xA5A, 0x56ADC0}, {0xA86, 0x56ADC0}, {0xAA7, 0x495040}, {0xABC, 0x56ADC0}, {0xADE, 0x4976D0}, {0xAFA, 0x56ADC0}, {0xB1F, 0x56ADC0}, {0xB3E, 0x594E00}, {0xB61, 0x56ADC0}, {0xB8D, 0x56ADC0}, {0xBC8, 0x5685D0}, {0xBE5, 0x56ADC0}, {0xC08, 0x57C0F0}, {0xC21, 0x594E00}, {0xC4B, 0x56ADC0}, {0xC71, 0x56ADC0}, {0xC87, 0x587BE0}, {0xC8E, 0x587740}, {0xC95, 0x495040}, {0xCA4, 0x423380}, {0xCAF, 0x56ADC0}, {0xCCB, 0x587B40}, {0xCD8, 0x495040}, {0xCED, 0x56ADC0}, {0xD18, 0x56ADC0}, {0xD3D, 0x594E00}, {0xD62, 0x56D6F0}, {0xD6D, 0x56ADC0}};
constexpr sh::JumpTable kTables569F20[] = {{0x15, 0xD78, 55}};
// Scena17_EndTask: 0x2E bytes; +0x1B call through .data 0x6620CC (Scena17_EndSteps, swapped by the harness).
constexpr sh::CallSite kCalls56D3B0[] = {{0x9, 0x5A99F4}, {0xE, 0x59E330}, {0x24, 0x5A9949}};

void* CopyWithTramps(const char* name, std::uint32_t base, std::uint32_t size, const sh::CallSite* sites, int n,
                     const sh::JumpTable* tables, int n_tables) {
    static bof3::CloneCall calls[128];
    if (n > 128) bof3::Fatal("scena_sc15: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) {
        const void* to = nullptr;
        for (const Tramp& t : kTramps)
            if (t.target == sites[i].target) to = t.to;
        if (!to) bof3::Fatal("scena_sc15: %s: no trampoline for 0x%X", name, (unsigned)sites[i].target);
        calls[i] = {sites[i].offset, to, sites[i].target};
    }
    void* copy = bof3::CloneOriginal(name, base, size, calls, n);
    for (int i = 0; i < n_tables; ++i)
        move_script::Relocate(copy, base, size, {tables[i].jmp_disp, tables[i].table, tables[i].entries});
    return copy;
}

}  // namespace
}  // namespace scena_sc15

extern "C" {
void* g_sc15_enter_copy = nullptr;
void* g_sc15_run6_copy = nullptr;
void* g_sc15_endtask_theirs = nullptr;
__attribute__((naked)) void Sc15EnterTheirs() { asm("jmp *_g_sc15_enter_copy"); }
__attribute__((naked)) void Sc15Run6Theirs() { asm("jmp *_g_sc15_run6_copy"); }
__attribute__((naked)) void Sc15EndTaskTheirs() { asm("jmp *_g_sc15_endtask_theirs"); }
}

namespace scena_sc15 {
namespace {

constexpr std::uint32_t kJmpWrapper = 6;   // FF 25 disp32
std::uint32_t Wrapper(void (*f)()) {
    const auto* p = reinterpret_cast<const unsigned char*>(f);
    if (p[0] != 0xFF || p[1] != 0x25) bof3::Fatal("scena_sc15: the jmp wrapper at %p is not FF 25", static_cast<const void*>(p));
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

// --- Scena17_EndTask's escape ---------------------------------------------------
//
// The task loops for ever (a Task_Sleep a frame): while it is fuzzed, this
// file's Task_Sleep stand-in jumps back to the wrapper after g_sleep_limit
// sleeps (1..3, the seed's), on both passes alike.
void* g_jb[5];
bool g_escape = false;
unsigned g_sleeps = 0, g_sleep_limit = 1;
void* g_endtask_copy = nullptr;

void __cdecl SleepStandIn(int frames) {
    sh::Record(::bof3::addr::Task_Sleep, static_cast<std::uint32_t>(frames) & kU16);
    sh::Stir();
    if (g_escape && ++g_sleeps >= g_sleep_limit) __builtin_longjmp(g_jb, 1);
}

__attribute__((noinline)) void RunEscaping(void (*fn)()) {
    g_sleeps = 0;
    g_escape = true;
    if (__builtin_setjmp(g_jb) == 0) {
        fn();
        bof3::Fatal("scena_sc15: Scena17_EndTask returned");
    }
    g_escape = false;
}
void TheirsEndTaskCopy() { reinterpret_cast<void (*)()>(g_endtask_copy)(); }
void OursEndTaskCall() { ::Scena17_EndTask(); }
void __cdecl TheirsEndTask() { RunEscaping(&TheirsEndTaskCopy); }
void __cdecl OursEndTask() { RunEscaping(&OursEndTaskCall); }

// ================================================================================
// The callees (registered before the standard set: these listings stand)

// A commit logs the primitive it links (the draws write every primitive into
// the same bytes of the fuzz's buffer: the regions at the end hold only the last).
std::uint32_t LogPrimitive(const std::uint32_t* a, std::uint32_t answer) {
    const unsigned size = a[1] & 0xFFu;
    sh::NoteBytes(Gfx_PacketNext, size < 0x40 ? size : 0x40);
    return answer;
}
const unsigned char* P(std::uint32_t v) { return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(v)); }
unsigned char* WP(std::uint32_t v) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(v)); }
// The GTE's recorders: the vectors and matrices they read by what they hold
// (the pads left out: stack bytes neither side writes), the outputs filled
// from the recorders' stream so that what reads them next sees the same.
std::uint32_t RotTransEffect(const std::uint32_t* a, std::uint32_t r) {
    sh::NoteBytes(P(a[0]), 6);
    sh::FillBytes(WP(a[1]), 12);
    return r;
}
std::uint32_t RotMatrixEffect(const std::uint32_t* a, std::uint32_t r) {
    sh::NoteBytes(P(a[0]), 6);
    sh::FillBytes(WP(a[1]), 18);
    return r;
}
std::uint32_t MulMatrixEffect(const std::uint32_t* a, std::uint32_t r) {
    sh::NoteBytes(P(a[0]), 18);
    sh::NoteBytes(P(a[1]), 18);
    sh::FillBytes(WP(a[2]), 18);
    return r;
}
std::uint32_t RotMatrixSetEffect(const std::uint32_t* a, std::uint32_t r) {
    sh::NoteBytes(P(a[0]), 18);
    return r;
}
std::uint32_t TransMatrixSetEffect(const std::uint32_t* a, std::uint32_t r) {
    sh::NoteBytes(P(a[0]) + 0x14, 12);
    return r;
}
std::uint32_t Pers3Effect(const std::uint32_t* a, std::uint32_t r) {
    sh::NoteBytes(P(a[0]), 6);
    sh::NoteBytes(P(a[1]), 6);
    sh::NoteBytes(P(a[2]), 6);
    return r;
}
// Scena15_RecordWord's answer is compared with 0x308 (Scena15_Run5): a
// third of the time it is.
std::uint32_t RecordWordEffect(const std::uint32_t*, std::uint32_t r) { return r % 3 == 0 ? 0x308u : r; }
// PartySet_LoadSecond / LoadFirst: Scena15_Run4 reads the party byte 0x90412C
// and its kept copy 0x6BC742 again after each; half the time one moves.
std::uint32_t PartySetEffect(const std::uint32_t*, std::uint32_t r) {
    const std::uint32_t n = sh::Noise();
    if (n & 1) B((n & 2) ? kPartyByte : kKept) = static_cast<unsigned char>((n & 4) ? 0xFF : n >> 8);
    return r;
}

const sh::Callee kCallees[] = {
    // the group's own, called directly: logged with their arguments
    {SC15_OURS(Scena15_PlaceEffect), 6, {kU8, kU16, kU16, kU16, kU8, kAll}, sh::Answer::kFlag, 0, 0},
    {SC15_OURS(Scena15_BattleSetup), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Scena15_SpawnMarker), 5, {kAll, kAll, kU8, kU8, kU8}, sh::Answer::kFlag, 0, 0},
    {SC15_OURS(Scena15_EventObjects), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC15_OURS(Scena15_ClutToGrey), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Scena15_RandomPause), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC15_OURS(Scena15_Shake), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Scena15_RecordWord), 1, {kU16}, sh::Answer::kGarbage, 0, 0, {}, &RecordWordEffect},
    {SC15_OURS(Scena17_DrawFade), 2, {kU8, kU8}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Scena17_DrawRays), 2, {kU16, kU8}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Scena17_DrawLetterbox), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC15_OURS(Scena17_DrawPanel), 4, {kU16, kU16, kU8, kU8}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Scena17_ScrollRoll), 0, {}, sh::Answer::kFlag, 0, 0},
    {SC15_OURS(Scena17_DrawLine), 3, {kU16, kU16, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Scena17_DrawGlyph), 4, {kU16, kU16, kU8, kU8}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Scena17_DrawLogo), 2, {kU16, kU16}, sh::Answer::kGarbage, 0, 0},
    // engine functions the standard set lacks or has otherwise
    {SC15_OURS(Party_DropIn), 1, {kU8}, sh::Answer::kFlag, 0, 0},   // Scena15_Run3 loops while it answers
    {SC15_OURS(Field_StartEventBattle), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(PartySet_LoadFirst), 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, &PartySetEffect},
    {SC15_OURS(PartySet_LoadSecond), 4, {kAll, kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, &PartySetEffect},
    {SC15_OURS(Menu_DrawHand), 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(EventOp_0x), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Task_Sleep), 1, {kU16}, sh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&SleepStandIn)},
    {SC15_OURS(Task_ClearPrivate), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Window_ResetAll), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Field_RunTaskRecords), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(ShopMode_Dispatch), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gfx_ClutStripRestore), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gfx_ClutStripCopyRow), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(AreaMap_Frame), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Field_ObjectsScreen), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Field_DrawFrame), 0, {}, sh::Answer::kGarbage, 0, 0},
    // the draws
    {SC15_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, &LogPrimitive},
    {SC15_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gpu_SetDrawMove), 4, {kAll, 0, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {0, 8}},
    {SC15_OURS(Gpu_SetTile), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gpu_SetSprt), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gpu_SetPolyG3), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gpu_SetPolyF4), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_RAW("0x5A7730", kPrimSprt16), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Math_Sin), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Math_Cos), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gte_PushMatrix), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gte_PopMatrix), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gte_RotTrans), 3, {0, 0, 0}, sh::Answer::kGarbage, 0, 0, {}, &RotTransEffect},
    {SC15_OURS(Gte_RotMatrix), 2, {0, 0}, sh::Answer::kGarbage, 0, 0, {}, &RotMatrixEffect},
    {SC15_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, sh::Answer::kGarbage, 0, 0, {}, &MulMatrixEffect},
    {SC15_OURS(Gte_SetRotMatrix), 1, {0}, sh::Answer::kGarbage, 0, 0, {}, &RotMatrixSetEffect},
    {SC15_OURS(Gte_SetTransMatrix), 1, {0}, sh::Answer::kGarbage, 0, 0, {}, &TransMatrixSetEffect},
    {SC15_OURS(Gte_RotTransPers3), 8, {0, 0, 0, kAll, kAll, kAll, 0, 0}, sh::Answer::kGarbage, 0, 0, {}, &Pers3Effect},
    {SC15_OURS(Gte_PrimDepths3_10B), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC15_OURS(Gte_StoreDepthF3), 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    // nobody's this wave, by address
    {SC15_RAW("0x57CD90", kEventSlot), 0, {}, sh::Answer::kByte, 0xFF, 0x1D},
    {SC15_RAW("0x533E50", kPartyPass), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_RAW("0x587860", kSoundStop), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_RAW("0x423380", kViewShift), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC15_RAW("0x42C0A0", kTalkIdC0), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC15_RAW("0x42BA90", kTalkId), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
};
constexpr unsigned kCalleesN = sizeof kCallees / sizeof kCallees[0];

// ================================================================================
// The object tables' typed stand-ins
//
// The handler recorder logs no arguments, and each chapter's object hook
// passes two (the object, the flag row): while an object hook is fuzzed,
// every entry of its table is this stand-in of the entry's own type, one per
// entry so the log says which (scena_sc0_fuzz.cpp's ObjectEntry, SC11's).
template <unsigned I> void __cdecl ObjectEntry(unsigned char* object, unsigned char* row) {
    sh::Record(0x56AE80, I, KeyOf(object), KeyOf(row));
    sh::Stir();
}
using ObjectFn = void (__cdecl*)(unsigned char*, unsigned char*);
const ObjectFn kObjectEntries[] = {
    &ObjectEntry<0>, &ObjectEntry<1>, &ObjectEntry<2>, &ObjectEntry<3>, &ObjectEntry<4>,  &ObjectEntry<5>,
    &ObjectEntry<6>, &ObjectEntry<7>, &ObjectEntry<8>, &ObjectEntry<9>, &ObjectEntry<10>, &ObjectEntry<11>,
};
const sh::Callee kObjectCallee = {"object table entries (keyed on 0x56AE80)", 0x56AE80, 0x56AE80, 2, {kAll, kAll},
                                  sh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&ObjectEntry<0>)};

// A table's entries: the typed stand-ins while its object hook is fuzzed, the
// harness's recorders (kept from the first round) otherwise.
struct ObjectTable {
    std::uint32_t at;
    unsigned n;
    std::uint32_t kept[12];
    bool have;
};
void SwapObjects(ObjectTable& t, bool typed) {
    if (!t.have) {
        for (unsigned i = 0; i < t.n; ++i) t.kept[i] = static_cast<std::uint32_t>(Long(Mem(t.at + 4 * i)));
        t.have = true;
    }
    for (unsigned i = 0; i < t.n; ++i)
        SetLong(Mem(t.at + 4 * i), static_cast<std::int32_t>(typed ? KeyOf(kObjectEntries[i]) : t.kept[i]));
}

// ================================================================================
// The state

constexpr unsigned kPrimBytes = 0x100;
alignas(16) unsigned char g_prims[kPrimBytes];   // what Gfx_PacketNext points into
alignas(16) unsigned char g_text[0x40];         // Scena17_DrawLine's text
// Scena15_RecordWord's table: 0x10000 records of 8 bytes, a word at each
// record's start - 0x308 at every seventh, 0x261 at every eleventh.
std::uint16_t g_records[0x10000 * 4];

void FillRecords() {
    for (unsigned i = 0; i < 0x10000; ++i)
        g_records[i * 4] = static_cast<std::uint16_t>(i % 7 == 0 ? 0x308 : i % 11 == 0 ? 0x261 : (i * 0x9E37u) >> 3);
}

// Chapter 15's, beyond the harness's 22 standard regions.
const sh::Region kRegions15[] = {
    {0x6BC740, 8},        // the chapters' own bytes
    {0x80F580, 0x4000},   // Gfx_ClutStrip, 32 rows (Scena15_ClutToGrey)
    {0x903800, 4},        // Camera_ShiftY
    {0x904EF0, 1},        // cleared by Scena15_BattleSetup
    {0x905BA4, 2},        // Field_ScriptFlags2
    {0x92BF19, 1},        // Draw_OtSlot
    {0x904131, 1},        // Music_Track
    {0x904152, 2},        // run 1's bytes
    {0x904CD0, 1},        // the area-change track byte
    {0x904EE0, 1},
    {0x90412C, 1},        // the party byte
    {0x929F10, 1},
    {0x905E20, 1},        // Cond_ByteFE
};
// Chapter 17's.
sh::Region g_regions17[] = {
    {0x6BC740, 8},
    {kPacketNext, 4},     // Gfx_PacketNext
    {0, kPrimBytes},      // g_prims (filled in at start-up)
    {0, sizeof g_text},   // g_text (likewise)
    {0x9037A0, 0x18},     // Prim_VertexScratch
    {0x905B89, 1},        // Gfx_BufferIndex
    {0x905E40, 0x20},     // Camera_Matrix
    {0x905E20, 1},        // Cond_ByteFE
    {0x904131, 1},        // Music_Track
    {0x929F00, 1},
    {0x929F0C, 1},
    {0x929F10, 4},        // Field_Kind2Hold at +2
    {kGameStep, 2},       // Game_Step
    {0x66C7DA, 1},
};
// Chapters 16's, 18's and 19's: the standard ones and the chapters' bytes.
const sh::Region kRegionsSmall[] = {{0x6BC740, 8}};

// ================================================================================
// Chapter 15

#define SC15_PLAIN(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, sh::Shape::shape}
#define SC15_CALLS(name, base, size, calls, shape) \
    {#name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, sh::Shape::shape}
#define SC15_TABLE(name, base, size, calls, tables) \
    {#name, base, size, calls, SH_N(calls), nullptr, 0, tables, SH_N(tables), reinterpret_cast<const void*>(&::name), 0, false, sh::Shape::kState}

// tools/scenario_rows.py --unit SC15 --clones (this tip), held against the
// group's reading: every jump internal, nothing REFUSED.
constexpr sh::CallSite kCalls568510[] = {{0x0, 0x57C7C0}, {0xC, 0x589810}, {0x66, 0x56F670}, {0x74, 0x531F90}, {0x9F, 0x532ED0}};
constexpr sh::CallSite kCalls5685D0[] = {{0x0, 0x589810}};
constexpr sh::CallSite kCalls568650[] = {{0x3C, 0x531F90}, {0x64, 0x57C0F0}, {0x7A, 0x594E00}, {0xC1, 0x4976D0}, {0xFD, 0x5685D0}, {0x115, 0x5905D0}, {0x138, 0x4976D0}, {0x174, 0x5685D0}, {0x18C, 0x5905D0}, {0x1AF, 0x4976D0}, {0x1D0, 0x57C7A0}, {0x1DC, 0x57C110}, {0x1F5, 0x594E00}, {0x222, 0x5905D0}, {0x246, 0x57C110}, {0x265, 0x594E00}, {0x276, 0x57C0F0}, {0x282, 0x57C0F0}, {0x29B, 0x594E00}, {0x2B4, 0x57C0F0}};
constexpr sh::JumpTable kTables568650[] = {{0x1E, 0x2E4, 10}};
constexpr sh::CallSite kCalls568980[] = {{0x79, 0x4410B0}, {0x92, 0x57C7A0}, {0xBC, 0x57C0F0}};
constexpr sh::JumpTable kTables568980[] = {{0x13, 0xD8, 6}};
constexpr sh::CallSite kCalls568A70[] = {{0x26, 0x454590}, {0x2D, 0x531F90}, {0x64, 0x5685D0}, {0x7E, 0x454810}, {0x98, 0x589810}, {0xFC, 0x57C140}, {0x124, 0x5685D0}, {0x160, 0x531F90}, {0x19E, 0x56ADF0}, {0x226, 0x57C7A0}, {0x23B, 0x57C0F0}, {0x242, 0x531F90}, {0x26E, 0x56ADF0}, {0x27C, 0x531F90}, {0x29D, 0x495040}, {0x2C6, 0x4976D0}, {0x2FE, 0x57C0F0}, {0x314, 0x594E00}, {0x350, 0x495040}, {0x393, 0x594E00}, {0x3D4, 0x56ADF0}, {0x3DB, 0x531F90}, {0x403, 0x57C0F0}, {0x42B, 0x531F90}, {0x452, 0x57C0F0}, {0x468, 0x594E00}, {0x4A7, 0x594E00}, {0x4C8, 0x57C7A0}, {0x4D6, 0x57C0F0}, {0x4F5, 0x57C0F0}, {0x50B, 0x594E00}, {0x53D, 0x495040}, {0x558, 0x57C7A0}, {0x566, 0x57C0F0}, {0x57F, 0x594E00}};
constexpr sh::JumpTable kTables568A70[] = {{0x1D, 0x598, 29}};
constexpr sh::CallSite kCalls5690C0[] = {{0x1D, 0x454590}, {0x24, 0x531F90}, {0x42, 0x589810}, {0xD0, 0x531F90}, {0xE1, 0x454810}, {0xFB, 0x589810}, {0x122, 0x587BE0}, {0x140, 0x589810}, {0x16E, 0x587B40}, {0x1A9, 0x587A20}, {0x1D3, 0x454810}, {0x1E2, 0x5734F0}, {0x228, 0x536890}, {0x274, 0x454810}, {0x291, 0x5341A0}, {0x2A3, 0x536850}, {0x2BB, 0x5341A0}, {0x2C5, 0x531F90}, {0x2CA, 0x569730}, {0x2E4, 0x587AE0}, {0x326, 0x454810}, {0x33B, 0x57C0F0}, {0x354, 0x594E00}, {0x38A, 0x594E00}, {0x3CA, 0x594E00}, {0x3FF, 0x57C0F0}, {0x418, 0x594E00}, {0x44D, 0x57C0F0}, {0x466, 0x594E00}, {0x49C, 0x4976D0}, {0x4C1, 0x454810}, {0x4D5, 0x587AE0}, {0x4E3, 0x495040}, {0x550, 0x5685D0}, {0x586, 0x5685D0}, {0x5A2, 0x587A20}, {0x5BC, 0x454810}, {0x5E3, 0x57C7A0}, {0x5F1, 0x57C0F0}};
constexpr sh::JumpTable kTables5690C0[] = {{0x14, 0x60C, 25}};
constexpr sh::CallSite kCalls569730[] = {{0xC, 0x57CD90}, {0x22, 0x57A010}, {0x3A, 0x57CD90}, {0x50, 0x57AD10}};
constexpr sh::CallSite kCalls5697A0[] = {{0x1A, 0x57C7A0}, {0x21, 0x531F90}, {0x3D, 0x531F90}, {0x5F, 0x589810}, {0xBD, 0x587A20}, {0xC4, 0x531F90}, {0xE4, 0x454810}, {0xF1, 0x589810}, {0x13B, 0x5720C0}, {0x156, 0x587AE0}, {0x1CD, 0x57C0F0}, {0x1E6, 0x594E00}, {0x210, 0x4976D0}, {0x240, 0x495040}, {0x278, 0x569DA0}, {0x28D, 0x569DA0}, {0x294, 0x587BE0}, {0x2BF, 0x587B40}, {0x2C6, 0x495040}, {0x311, 0x4976D0}, {0x359, 0x589810}, {0x399, 0x495040}, {0x3BA, 0x587860}, {0x3C4, 0x5A9976}, {0x3D2, 0x531F90}, {0x3DC, 0x587A20}, {0x41C, 0x537580}, {0x45E, 0x57C0F0}, {0x480, 0x495040}, {0x4AA, 0x4976D0}, {0x4D3, 0x5341A0}, {0x52C, 0x594E00}};
constexpr sh::JumpTable kTables5697A0[] = {{0x16, 0x564, 37}};
constexpr sh::CallSite kCalls56AD80[] = {{0x10, 0x5B93D2}};
constexpr sh::CallSite kCalls56ADF0[] = {{0x1, 0x589810}, {0x46, 0x5720C0}};
constexpr sh::CallSite kCalls56AEA0[] = {{0x26, 0x42C0A0}, {0x2C, 0x4976D0}, {0x41, 0x42BA90}, {0x47, 0x4976D0}};
constexpr sh::CallSite kCalls56AF00[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCallsFlagDrop[] = {{0x0, 0x57C7C0}, {0xD, 0x57C0F0}, {0x14, 0x531F90}};   // 0x56AF20, 0x56AF50, 0x56AFB0, 0x56AFE0
constexpr sh::CallSite kCalls56AF80[] = {{0x0, 0x57C7C0}, {0xD, 0x57C0F0}};
constexpr sh::CallSite kCalls56B010[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls56B030[] = {{0x21, 0x57C140}, {0x37, 0x57C7C0}, {0x5D, 0x57C140}, {0x7A, 0x57C7C0}, {0xB4, 0x57C7C0}};
constexpr sh::CallSite kCalls56B100[] = {{0x17, 0x57C140}, {0x42, 0x57C7C0}, {0x61, 0x57C140}, {0x8C, 0x57C7C0}, {0xAB, 0x57C140}, {0xD6, 0x57C7C0}, {0x114, 0x57C140}, {0x129, 0x57C140}, {0x13D, 0x57C140}, {0x152, 0x57C140}, {0x167, 0x57C140}, {0x173, 0x57C7C0}, {0x189, 0x57C7C0}};

sh::Clone g_clones15[] = {
    {"Scena15_RecordWord", 0x537580, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena15_RecordWord), 0xFFFF, false, sh::Shape::kEntry},
    SC15_PLAIN(Scena15_Frame, 0x567DC0, 0xE, kSlot),
    {"Scena15_EnterArea", 0, kJmpWrapper, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena15_EnterArea), 0, false, sh::Shape::kState},   // this file's copy
    SC15_CALLS(Scena15_BattleSetup, 0x568510, 0xBE, kCalls568510, kEntry),
    {"Scena15_PlaceEffect", 0x5685D0, 0x63, kCalls5685D0, SH_N(kCalls5685D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena15_PlaceEffect), 0xFF, false, sh::Shape::kEntry},
    SC15_PLAIN(Scena15_Run, 0x568640, 0xE, kState),
    SC15_TABLE(Scena15_Run1, 0x568650, 0x326, kCalls568650, kTables568650),
    SC15_TABLE(Scena15_Run2, 0x568980, 0xF0, kCalls568980, kTables568980),
    SC15_TABLE(Scena15_Run3, 0x568A70, 0x641, kCalls568A70, kTables568A70),
    SC15_TABLE(Scena15_Run4, 0x5690C0, 0x670, kCalls5690C0, kTables5690C0),
    SC15_CALLS(Scena15_EventObjects, 0x569730, 0x61, kCalls569730, kState),
    SC15_TABLE(Scena15_Run5, 0x5697A0, 0x5F8, kCalls5697A0, kTables5697A0),
    SC15_PLAIN(Scena15_ClutToGrey, 0x569DA0, 0x172, kEntry),
    {"Scena15_Run6", 0, kJmpWrapper, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena15_Run6), 0, false, sh::Shape::kState},   // this file's copy
    SC15_CALLS(Scena15_RandomPause, 0x56AD80, 0x34, kCalls56AD80, kState),
    SC15_PLAIN(Scena15_Shake, 0x56ADC0, 0x2E, kEntry),
    {"Scena15_SpawnMarker", 0x56ADF0, 0x87, kCalls56ADF0, SH_N(kCalls56ADF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena15_SpawnMarker), 0xFF, false, sh::Shape::kEntry},
    SC15_PLAIN(Scena15_ObjectTrigger, 0x56AE80, 0x1F, kObject),
    SC15_CALLS(Scena15_ObjectTalk, 0x56AEA0, 0x57, kCalls56AEA0, kEntry),
    SC15_CALLS(Scena15_Object05, 0x56AF00, 0x14, kCalls56AF00, kEntry),
    SC15_CALLS(Scena15_Object06, 0x56AF20, 0x28, kCallsFlagDrop, kEntry),
    SC15_CALLS(Scena15_Object07, 0x56AF50, 0x28, kCallsFlagDrop, kEntry),
    SC15_CALLS(Scena15_Object08, 0x56AF80, 0x21, kCalls56AF80, kEntry),
    SC15_CALLS(Scena15_Object09, 0x56AFB0, 0x28, kCallsFlagDrop, kEntry),
    SC15_CALLS(Scena15_Object10, 0x56AFE0, 0x28, kCallsFlagDrop, kEntry),
    SC15_CALLS(Scena15_Object11, 0x56B010, 0x11, kCalls56B010, kEntry),
    {"Scena15_StepHook", 0x56B030, 0xCF, kCalls56B030, SH_N(kCalls56B030), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena15_StepHook), 0xFF, false, sh::Shape::kHook},
    {"Scena15_ArriveHook", 0x56B100, 0x19F, kCalls56B100, SH_N(kCalls56B100), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena15_ArriveHook), 0xFF, false, sh::Shape::kHook},
};
enum : unsigned {
    k15RecordWord, k15Frame, k15EnterArea, k15Battle, k15Place, k15Run, k15Run1, k15Run2, k15Run3, k15Run4,
    k15EventObjects, k15Run5, k15Grey, k15Run6, k15Pause, k15Shake, k15Spawn, k15Object, k15Talk, k15Object05,
    k15Object11 = k15Object05 + 6, k15StepHook, k15ArriveHook, k15Count
};
static_assert(k15Count == sizeof g_clones15 / sizeof g_clones15[0], "one role per clone");

const sh::DataTable kTables15[] = {{0x661924, 3}, {0x661930, 7}, {0x6619AC, 12}};
ObjectTable g_objects15 = {0x6619AC, 12, {}, false};

// The steps each run's switch holds (and a few beside them, which do nothing).
constexpr std::uint8_t kSteps1[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0x13, 0x14, 0x15, 0x19, 0x1A};
constexpr std::uint8_t kSteps3[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
                                    0x1E, 0x1F, 0x20, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x32, 0x33, 0x34, 0x35};
constexpr std::uint8_t kCounters3[] = {0xB, 0xC, 0xE, 0x13, 3, 5, 6, 2, 0, 1};
constexpr std::uint8_t kCounters4[] = {0xF, 0x11, 0x12, 0x14, 0x15, 0x16, 0x2B, 1, 2, 0xD, 0, 0x26, 0x3C, 0x3F, 0x10, 0x18};
constexpr std::uint8_t kSteps5[] = {0, 1, 5, 6, 7, 8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
                                    0x17, 0x18, 0x19, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25};
constexpr std::uint8_t kCounters5[] = {1, 0x15, 0x1F, 3, 4, 0x2A, 0x31, 0};
constexpr std::uint8_t kCounters6[] = {5, 8, 9, 0xB, 0xD, 0xE, 0xF, 1, 0, 3, 7, 2, 0x15};
constexpr std::uint16_t kAreas15[] = {2, 0x2D, 0x95, 0x98, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xAC, 0xAD, 0xAE, 0xBD, 0xC6, 0xC0, 0};
constexpr std::uint16_t kTimers[] = {1, 1, 1, 0, 2, 0x1F, 0x20, 0x21};

unsigned g_k;   // the clone being seeded, for Disturb and Settle
int g_chapter;

// A timer seeded for a count-down (1 half the time) or a threshold.
void SeedTimer() { SetW(kTimer, sh::Half() ? 1u : PickOf(kTimers)); }

void Seed15(unsigned k) {
    g_k = k;
    g_chapter = 15;
    SwapObjects(g_objects15, k == k15Object);
    SetL(kRecordTable, KeyOf(g_records));
    B(kState) = static_cast<unsigned char>(sh::Next() % 3);
    B(kRun) = static_cast<unsigned char>(sh::Next() % 7);
    B(kSlot) = static_cast<unsigned char>(sh::Next() % 20);
    Effect(B(kSlot))[0] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 0 : sh::Next());
    Effect(B(kSlot))[0xB] = static_cast<unsigned char>(sh::Next() % 12);
    if (sh::Often()) SetW(kArea, PickOf(kAreas15));
    B(kByteFD) = static_cast<unsigned char>(sh::Next() % 4);
    if (sh::Half()) B(kRequest) = PickOf<std::uint8_t, 4>({0, 2, 6, 1});
    if (sh::Half()) SetW(kWait, 0);
    if (sh::Half()) B(kShakeOn) = 0;
    B(kShakeShift) = static_cast<unsigned char>(sh::Next() % 8);
    if (sh::Half()) B(kKept) = sh::Half() ? 0 : 0xFF;
    if (sh::Half()) B(kPartyByte) = sh::Half() ? B(kKept) : 0xFF;
    SeedTimer();
    sh::SetRandHint(sh::Next() & 0xFF);
    switch (k) {
    case k15Run1:
        B(kStep) = PickOf(kSteps1);
        if (sh::Half()) B(kCounter0) = 1;
        break;
    case k15Run2:
        B(kStep) = static_cast<unsigned char>(sh::Next() % 7);
        if (sh::Half()) B(kCounter0) = 3;
        if (sh::Half()) SetW(kInputHeld, 0);
        break;
    case k15Run3:
        B(kStep) = PickOf(kSteps3);
        B(kCounter0) = PickOf(kCounters3);
        break;
    case k15Run4:
        B(kStep) = static_cast<unsigned char>(sh::Next() % 0x1A);
        B(kCounter0) = PickOf(kCounters4);
        B(kCounter3) = PickOf<std::uint8_t, 3>({0x10, 0, 5});
        break;
    case k15Run5:
        B(kStep) = PickOf(kSteps5);
        B(kCounter0) = PickOf(kCounters5);
        if (sh::Half()) B(kRequest) = PickOf<std::uint8_t, 3>({0, 2, 6});
        break;
    case k15Run6:
        B(kStep) = static_cast<unsigned char>(sh::Next() % 0x38);
        B(kCounter0) = PickOf(kCounters6);
        if (sh::Half()) SetW(kDistance, 0x780 + (sh::Next() % 3) - 1);
        break;
    case k15EnterArea:
        SetW(kArea, PickOf(kAreas15));
        B(kByteFD) = static_cast<unsigned char>(sh::Next() % 3);
        break;
    case k15Object:
        sh::SpriteRecord(0)[0x86] = static_cast<unsigned char>(sh::Next() % 12);
        break;
    case k15Talk:
        sh::SpriteRecord(0)[0x86] = static_cast<unsigned char>(sh::Next() % 5);
        if (sh::Half()) SetW(kArea, 0xC0);
        break;
    case k15StepHook:
        SetW(kArea, sh::Half() ? 0xAD : PickOf<std::uint16_t, 3>({0xAE, 0xAE, 0x9E}));
        B(kByteFD) = static_cast<unsigned char>(sh::Next() % 3);
        break;
    case k15ArriveHook:
        if (sh::Often()) SetW(kArea, 0xAE);
        break;
    default: break;
    }
    if (sh::Next() % 10 == 0) B(kStep) = static_cast<unsigned char>(sh::Next());   // any step, now and then
}

// The hooks' coordinates: the values their tests name, either side of each,
// in the cells each tests; else the harness's cell and fraction.
std::uint32_t Coord(std::uint32_t exact, std::uint32_t cell_lo, unsigned cells) {
    switch (sh::Next() % 5) {
    case 0: return exact;
    case 1: return exact + (sh::Half() ? 1u : 0xFFFFFFFFu);
    case 2: return (cell_lo + sh::Next() % (cells + 2) - 1) << 16 | (sh::Next() & 0xFFFF);
    case 3: return (cell_lo + sh::Next() % cells) << 16;
    default: return sh::Next() & 0x7FFFFF;
    }
}

void Args15(unsigned k, std::uint32_t* a) {
    switch (k) {
    case k15Object:
    case k15Talk:
    case k15Object05:
    case k15Object05 + 1:
    case k15Object05 + 2:
    case k15Object05 + 3:
    case k15Object05 + 4:
    case k15Object05 + 5:
    case k15Object11:
        a[0] = KeyOf(sh::SpriteRecord(0));
        a[1] = KeyOf(sh::FlagRow());
        break;
    case k15Battle: a[0] = (sh::Next() & ~0xFFu) | (sh::Next() % 11); break;
    case k15StepHook:
        switch (sh::Next() % 3) {
        case 0:   // area 0xAD, Cond_ByteFD 1: z against 0x470000
            a[1] = Coord(0x470000, 0x46, 3);
            break;
        case 1:   // Cond_ByteFD 2: x's cell above 0x2C, z's 0x78..0x7F
            a[0] = Coord(0x2C0000, 0x2B, 4);
            a[1] = Coord(0x780000, 0x77, 10);
            break;
        default:   // area 0xAE: x 0x418000, z's cell 0x22..0x24
            a[0] = Coord(0x418000, 0x41, 2);
            a[1] = Coord(0x220000, 0x21, 5);
            break;
        }
        break;
    case k15ArriveHook:
        switch (sh::Next() % 4) {
        case 0:
            a[0] = Coord(0x440000, 0x43, 3);
            a[1] = Coord(0x30000, 2, 4);
            break;
        case 1:
            a[0] = Coord(0x530000, 0x52, 3);
            a[1] = Coord(0x2E0000, 0x2D, 5);
            break;
        case 2:
            a[0] = Coord(0x230000, 0x22, 6);
            a[1] = Coord(0x3A0000, 0x39, 3);
            break;
        default:
            a[0] = Coord(0x310000, 0x30, 3);
            a[1] = Coord(0x270000, 0x26, 4);
            break;
        }
        break;
    default: break;
    }
}

// After a call, two in three (the harness's disturbance, then this through
// Settle for the functions that read these cells again): drawn from the hash
// or the recorders' stream only, never the harness's Next.
void Move15(std::uint32_t h) {
    switch ((h >> 8) % 9) {
    case 0: SetW(kArea, kAreas15[(h >> 12) % (sizeof kAreas15 / sizeof kAreas15[0])]); break;
    case 1: B(kByteFD) = static_cast<unsigned char>((h >> 12) % 3); break;
    case 2: B(kSlot) = static_cast<unsigned char>((h >> 12) % 20); break;
    case 3: Effect(B(kSlot))[0] = static_cast<unsigned char>(Effect(B(kSlot))[0] ^ 1); break;
    case 4: B(kPartyByte) = static_cast<unsigned char>(h >> 16); break;
    case 5: B(kKept) = static_cast<unsigned char>((h >> 12) & 1 ? 0 : h >> 16); break;
    case 6: B(kShakeShift) = static_cast<unsigned char>((h >> 12) % 8); break;
    case 7: B(kShakeOn) = static_cast<unsigned char>((h >> 12) & 1); break;
    default: B(kCounter0) = kCounters6[(h >> 12) % (sizeof kCounters6)]; break;
    }
}
void Disturb15(std::uint32_t h) { Move15(h); }
// Half the time a second cell moved, from the recorders' stream (the
// harness's disturbance reaches the group's cells one call in 24); the effect
// slot always put back inside the 20 records (the runs write through it).
void Settle15() {
    const std::uint32_t n = sh::Noise();
    if (n & 1) Move15(n >> 1);
    B(kSlot) = static_cast<unsigned char>(B(kSlot) % 20);
}

// ================================================================================
// Chapter 16's object hook

const sh::Clone kClones16[] = {SC15_PLAIN(Scena16_ObjectTrigger, 0x56C080, 0x1F, kObject)};
const sh::DataTable kTables16[] = {{0x661A1C, 1}};
ObjectTable g_objects16 = {0x661A1C, 1, {}, false};
void Seed16(unsigned) {
    g_chapter = 16;
    SwapObjects(g_objects16, true);
    sh::SpriteRecord(0)[0x86] = 0;
}
void ArgsObject(unsigned, std::uint32_t* a) { a[0] = KeyOf(sh::SpriteRecord(0)); }

// ================================================================================
// Chapter 17

constexpr sh::CallSite kCalls56C140[] = {{0x0, 0x4549B0}, {0x24, 0x594E00}};
constexpr sh::CallSite kCalls56C180[] = {{0x12, 0x57C7C0}, {0x17, 0x57C810}, {0x33, 0x5725F0}, {0x38, 0x56E6C0}, {0x3D, 0x5173E0}, {0x42, 0x592F00}, {0x85, 0x5A7810}, {0x8E, 0x461E50}, {0x9F, 0x4DF820}};
constexpr sh::CallSite kCalls56C250[] = {{0xE, 0x56CFF0}};
constexpr sh::CallSite kCalls56C270[] = {{0x1E, 0x56F670}, {0x2F, 0x587A20}, {0x36, 0x495040}, {0x42, 0x56CBA0}};
constexpr sh::CallSite kCalls56C2D0[] = {{0xA, 0x454810}, {0x2D, 0x56CBA0}};
constexpr sh::CallSite kCalls56C310[] = {{0x15, 0x587AE0}, {0x37, 0x56CBA0}};
constexpr sh::CallSite kCalls56C350[] = {{0x54, 0x56CBA0}};
constexpr sh::CallSite kCalls56C3B0[] = {{0xF, 0x56CBA0}};
constexpr sh::CallSite kCalls56C470[] = {{0x16, 0x5720C0}, {0x1C, 0x5725F0}};
constexpr sh::CallSite kCalls56C4C0[] = {{0x16, 0x5720C0}, {0x1C, 0x5725F0}, {0x33, 0x5720C0}};
constexpr sh::CallSite kCalls56C550[] = {{0x16, 0x5720C0}, {0x1C, 0x5725F0}};
constexpr sh::CallSite kCalls56C640[] = {{0x10, 0x495040}};
constexpr sh::CallSite kCalls56C670[] = {{0xC, 0x587B40}};
constexpr sh::CallSite kCalls56C6B0[] = {{0xE, 0x56CFF0}};
constexpr sh::CallSite kCalls56C6D0[] = {{0x2, 0x587910}};
constexpr sh::CallSite kCalls56C6F0[] = {{0x1, 0x56D110}, {0x8, 0x587A00}, {0x17, 0x495040}};
constexpr sh::CallSite kCalls56C720[] = {{0x1E, 0x56D110}};
constexpr sh::CallSite kCalls56C760[] = {{0x14, 0x56F670}, {0x1B, 0x495040}, {0x22, 0x5A9949}, {0x35, 0x587AE0}};
constexpr sh::CallSite kCalls56C7B0[] = {{0x24, 0x56CCF0}};
constexpr sh::CallSite kCalls56C7E0[] = {{0x2D, 0x56CCF0}, {0x45, 0x56CCF0}};
constexpr sh::CallSite kCalls56C830[] = {{0x32, 0x56CCF0}};
constexpr sh::CallSite kCalls56C870[] = {{0x1E, 0x56CCF0}};
constexpr sh::CallSite kCalls56C8A0[] = {{0x2D, 0x56CCF0}, {0x48, 0x56CCF0}};
constexpr sh::CallSite kCalls56C900[] = {{0x2D, 0x56CCF0}, {0x41, 0x56CCF0}};
constexpr sh::CallSite kCalls56C950[] = {{0x38, 0x454590}, {0x4E, 0x56D070}, {0x60, 0x56CCF0}, {0x76, 0x56D070}, {0x88, 0x56CCF0}};
constexpr sh::CallSite kCalls56C9F0[] = {{0x10, 0x454810}, {0x4E, 0x56CCF0}, {0x64, 0x56D070}};
constexpr sh::CallSite kCalls56CA60[] = {{0x14, 0x587BE0}, {0x1B, 0x495040}, {0x39, 0x56CCF0}, {0x4F, 0x56D070}};
constexpr sh::CallSite kCalls56CAC0[] = {{0xC, 0x587B40}, {0x1A, 0x4DF820}, {0x28, 0x4976D0}, {0x4D, 0x56CCF0}, {0x63, 0x56D070}};
constexpr sh::CallSite kCalls56CB30[] = {{0x19, 0x587860}, {0x23, 0x5A9976}};
constexpr sh::CallSite kCalls56CB60[] = {{0x11, 0x5A9976}};
constexpr sh::CallSite kCalls56CBA0[] = {{0x20, 0x5A77C0}, {0x29, 0x461E50}, {0x35, 0x5A7740}, {0x3D, 0x5A7780}, {0x67, 0x461E50}, {0x82, 0x5A77C0}, {0x8B, 0x461E50}, {0x97, 0x5A7710}, {0xA8, 0x5A7780}, {0xD4, 0x461E50}, {0xE8, 0x5A77C0}, {0xF4, 0x461E50}, {0x100, 0x5A7710}, {0x107, 0x5A7780}, {0x137, 0x461E50}};
constexpr sh::CallSite kCalls56CCF0[] = {{0x7, 0x5A7B90}, {0x4A, 0x5720C0}, {0x6D, 0x5A8200}, {0x7C, 0x5A8060}, {0x90, 0x5A7D70}, {0x9A, 0x5A8DE0}, {0xA4, 0x5A8E00}, {0xB5, 0x5A77C0}, {0xC1, 0x461E50}, {0xFB, 0x5A7A50}, {0x10D, 0x5A7A00}, {0x12C, 0x5A7A50}, {0x13E, 0x5A7A00}, {0x15F, 0x5A75F0}, {0x167, 0x5A7780}, {0x191, 0x5A84A0}, {0x197, 0x5A9310}, {0x1C1, 0x461E50}, {0x1DB, 0x5A7A50}, {0x1F4, 0x5A7A00}, {0x216, 0x5A7A50}, {0x22E, 0x5A7A00}, {0x254, 0x5A75B0}, {0x25C, 0x5A7780}, {0x286, 0x5A84A0}, {0x29A, 0x5A9130}, {0x2D0, 0x461E50}, {0x2E6, 0x5A7BC0}};
constexpr sh::CallSite kCalls56CFF0[] = {{0x9, 0x5A7740}, {0x31, 0x461E50}, {0x3D, 0x5A7740}, {0x67, 0x461E50}};
constexpr sh::CallSite kCalls56D070[] = {{0x1F, 0x5A77C0}, {0x28, 0x461E50}, {0x34, 0x5A7710}, {0x45, 0x5A7780}, {0x95, 0x461E50}};
constexpr sh::CallSite kCalls56D110[] = {{0x38, 0x56D1A0}};
constexpr sh::CallSite kCalls56D1A0[] = {{0x15, 0x5A77C0}, {0x1E, 0x461E50}, {0x81, 0x56D2D0}, {0xA2, 0x56D350}, {0xCD, 0x56D2D0}};
constexpr sh::JumpTable kTables56D1A0[] = {{0x5A, 0xF8, 5}};
constexpr sh::CallSite kCalls56D2D0[] = {{0x8, 0x5A7730}, {0x6A, 0x461E50}};
constexpr sh::CallSite kCalls56D350[] = {{0x8, 0x5A7710}, {0x56, 0x461E50}};
constexpr sh::CallSite kCalls56D3E0[] = {{0xB, 0x57C0F0}, {0x1E, 0x57C110}, {0x2C, 0x533E50}, {0x3A, 0x57C7A0}, {0x9D, 0x454590}, {0xA5, 0x454810}, {0xB1, 0x5A9949}, {0xB9, 0x454810}, {0xC4, 0x4549F0}, {0xCB, 0x4549F0}};
constexpr sh::CallSite kCalls56D4D0[] = {{0x0, 0x59E230}, {0x5, 0x57F500}};
constexpr sh::CallSite kCalls56D4E0[] = {{0x16, 0x587860}, {0x20, 0x5A9976}};

sh::Clone g_clones17[] = {
    SC15_PLAIN(Scena17_Frame, 0x56C130, 0xE, kSlot),
    SC15_CALLS(Scena17_Start, 0x56C140, 0x3E, kCalls56C140, kState),
    SC15_CALLS(Scena17_EnterArea, 0x56C180, 0xB6, kCalls56C180, kState),
    SC15_PLAIN(Scena17_Run, 0x56C240, 0xE, kState),
    SC15_CALLS(Scena17_Intro, 0x56C250, 0x13, kCalls56C250, kState),
    SC15_CALLS(Scena17_Intro00, 0x56C270, 0x57, kCalls56C270, kState),
    SC15_CALLS(Scena17_Intro01, 0x56C2D0, 0x36, kCalls56C2D0, kState),
    SC15_CALLS(Scena17_Intro02, 0x56C310, 0x40, kCalls56C310, kState),
    SC15_CALLS(Scena17_Intro03, 0x56C350, 0x5D, kCalls56C350, kState),
    SC15_CALLS(Scena17_Intro04, 0x56C3B0, 0x27, kCalls56C3B0, kState),
    SC15_PLAIN(Scena17_Intro05, 0x56C3E0, 0x20, kState),
    SC15_PLAIN(Scena17_Intro06, 0x56C400, 0x2F, kState),
    SC15_PLAIN(Scena17_Intro07, 0x56C430, 0x32, kState),
    SC15_CALLS(Scena17_Intro08, 0x56C470, 0x4B, kCalls56C470, kState),
    SC15_CALLS(Scena17_Intro09, 0x56C4C0, 0x8A, kCalls56C4C0, kState),
    SC15_CALLS(Scena17_Intro10, 0x56C550, 0x5D, kCalls56C550, kState),
    SC15_PLAIN(Scena17_Intro11, 0x56C5B0, 0x3C, kState),
    SC15_PLAIN(Scena17_Intro12, 0x56C5F0, 0x1D, kState),
    SC15_PLAIN(Scena17_Intro13, 0x56C610, 0x2F, kState),
    SC15_CALLS(Scena17_Intro14, 0x56C640, 0x25, kCalls56C640, kState),
    SC15_CALLS(Scena17_Intro15, 0x56C670, 0x3D, kCalls56C670, kState),
    SC15_CALLS(Scena17_Roll, 0x56C6B0, 0x13, kCalls56C6B0, kState),
    SC15_CALLS(Scena17_Roll0, 0x56C6D0, 0x1E, kCalls56C6D0, kState),
    SC15_CALLS(Scena17_Roll1, 0x56C6F0, 0x2D, kCalls56C6F0, kState),
    SC15_CALLS(Scena17_Roll2, 0x56C720, 0x23, kCalls56C720, kState),
    SC15_PLAIN(Scena17_Outro, 0x56C750, 0xE, kState),
    SC15_CALLS(Scena17_Outro00, 0x56C760, 0x4A, kCalls56C760, kState),
    SC15_CALLS(Scena17_Outro01, 0x56C7B0, 0x2D, kCalls56C7B0, kState),
    SC15_CALLS(Scena17_Outro02, 0x56C7E0, 0x4E, kCalls56C7E0, kState),
    SC15_CALLS(Scena17_Outro03, 0x56C830, 0x3B, kCalls56C830, kState),
    SC15_CALLS(Scena17_Outro04, 0x56C870, 0x27, kCalls56C870, kState),
    SC15_CALLS(Scena17_Outro05, 0x56C8A0, 0x51, kCalls56C8A0, kState),
    SC15_CALLS(Scena17_Outro06, 0x56C900, 0x4A, kCalls56C900, kState),
    SC15_CALLS(Scena17_Outro07, 0x56C950, 0x91, kCalls56C950, kState),
    SC15_CALLS(Scena17_Outro08, 0x56C9F0, 0x6D, kCalls56C9F0, kState),
    SC15_CALLS(Scena17_Outro09, 0x56CA60, 0x58, kCalls56CA60, kState),
    SC15_CALLS(Scena17_Outro10, 0x56CAC0, 0x6C, kCalls56CAC0, kState),
    SC15_CALLS(Scena17_Outro11, 0x56CB30, 0x2A, kCalls56CB30, kState),
    SC15_CALLS(Scena17_Outro12, 0x56CB60, 0x18, kCalls56CB60, kState),
    SC15_PLAIN(Scena17_ObjectTrigger, 0x56CB80, 0x1F, kObject),
    SC15_CALLS(Scena17_DrawFade, 0x56CBA0, 0x144, kCalls56CBA0, kEntry),
    SC15_CALLS(Scena17_DrawRays, 0x56CCF0, 0x2F3, kCalls56CCF0, kEntry),
    SC15_CALLS(Scena17_DrawLetterbox, 0x56CFF0, 0x72, kCalls56CFF0, kState),
    SC15_CALLS(Scena17_DrawPanel, 0x56D070, 0x9F, kCalls56D070, kEntry),
    {"Scena17_ScrollRoll", 0x56D110, 0x81, kCalls56D110, SH_N(kCalls56D110), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena17_ScrollRoll), 0xFF, false, sh::Shape::kState},
    {"Scena17_DrawLine", 0x56D1A0, 0x12D, kCalls56D1A0, SH_N(kCalls56D1A0), nullptr, 0, kTables56D1A0, SH_N(kTables56D1A0), reinterpret_cast<const void*>(&::Scena17_DrawLine), 0, false, sh::Shape::kEntry},
    SC15_CALLS(Scena17_DrawGlyph, 0x56D2D0, 0x74, kCalls56D2D0, kEntry),
    SC15_CALLS(Scena17_DrawLogo, 0x56D350, 0x60, kCalls56D350, kEntry),
    {"Scena17_EndTask", 0, kJmpWrapper, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&OursEndTask), 0, false, sh::Shape::kState},   // both sides wrapped
    SC15_CALLS(Scena17_EndReturn, 0x56D3E0, 0xE2, kCalls56D3E0, kState),
    SC15_CALLS(Scena17_EndField, 0x56D4D0, 0xA, kCalls56D4D0, kState),
    SC15_CALLS(Scena17_EndRestart, 0x56D4E0, 0x27, kCalls56D4E0, kState),
};
enum : unsigned {
    k17Frame, k17Start, k17EnterArea, k17Run, k17Intro, k17Intro00, k17Intro15 = k17Intro00 + 15, k17Roll, k17Roll0,
    k17Roll2 = k17Roll0 + 2, k17Outro, k17Outro00, k17Outro12 = k17Outro00 + 12, k17Object, k17Fade, k17Rays,
    k17Letterbox, k17Panel, k17Scroll, k17Line, k17Glyph, k17Logo, k17EndTask, k17EndReturn, k17EndField,
    k17EndRestart, k17Count
};
static_assert(k17Count == sizeof g_clones17 / sizeof g_clones17[0], "one role per clone");

const sh::DataTable kTables17[] = {{0x661FEC, 3}, {0x661FF8, 4}, {0x662008, 16}, {0x662048, 3}, {0x662054, 13},
                                   {0x662088, 1}, {0x6620CC, 3}};
ObjectTable g_objects17 = {0x662088, 1, {}, false};

// What 0x8034E5 counts to in the steps that count it (and one either side).
constexpr std::uint8_t kCounts[] = {1, 1, 2, 0, 0x31, 0x32, 0x30, 0x44, 0x45, 0x46, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81};
constexpr std::uint32_t kElevations[] = {0x53E, 0x53F, 0x540, 0x541, 0xCAF, 0xCB0, 0xCB1, 0xCC0, 0};

void SeedText() {
    static const char kAlphabet[] = "  !0!5!9##@ABMZaz09.-,'&";
    const unsigned n = 1 + sh::Next() % 20;
    for (unsigned i = 0; i < n; ++i) g_text[i] = static_cast<unsigned char>(kAlphabet[sh::Next() % (sizeof kAlphabet - 1)]);
    if (sh::Half()) g_text[sh::Next() % n] = static_cast<unsigned char>(sh::Next());
    g_text[n] = 0;
    if (sh::Next() % 8 == 0) g_text[0] = 0;
}

void Seed17(unsigned k) {
    g_k = k;
    g_chapter = 17;
    SwapObjects(g_objects17, k == k17Object);
    Gfx_PacketNext = g_prims + 4 * (sh::Next() % 16);
    B(kState) = static_cast<unsigned char>(sh::Next() % 3);
    B(kPart) = static_cast<unsigned char>(sh::Next() % 4);
    B(kStep) = PickOf(kCounts);
    if (sh::Half()) SetW(kWait, 0);
    if (sh::Half()) B(kRequest) = sh::Half() ? 2 : 0;
    if (sh::Half()) B(kHold) = 0;
    B(kCounter3) = PickOf<std::uint8_t, 5>({0x14, 0x64, 1, 2, 0});
    SetL(kElevation, PickOf(kElevations));
    if (sh::Half()) B(kInputPressed) = static_cast<unsigned char>(sh::Next() & 0x28);
    if (sh::Half()) B(kRollSkip) = 0;
    SetW(kGameStep, sh::Next() % 3);
    g_sleep_limit = 1 + sh::Next() % 3;
    if (sh::Half()) SetW(kArea, 0xC7);
    // the roll: a line near the end half the time, the scroll at its turn
    SetW(kRollLine, sh::Half() ? 336 + sh::Next() % 16 : sh::Next() % 352);
    B(kRollScroll) = static_cast<unsigned char>(sh::Half() ? 0x15 : sh::Next() % 0x16);
    SeedText();
    switch (k) {
    case k17Intro: B(kRun) = static_cast<unsigned char>(sh::Next() % 16); break;
    case k17Roll: B(kRun) = static_cast<unsigned char>(sh::Next() % 3); break;
    case k17Outro: B(kRun) = static_cast<unsigned char>(sh::Next() % 13); break;
    case k17Object: sh::SpriteRecord(0)[0x86] = 0; break;
    default: B(kRun) = static_cast<unsigned char>(sh::Next()); break;
    }
}

void Args17(unsigned k, std::uint32_t* a) {
    switch (k) {
    case k17Object: a[0] = KeyOf(sh::SpriteRecord(0)); break;
    case k17Line: a[2] = KeyOf(g_text); break;
    case k17Fade:
        if (sh::Half()) a[0] &= 0xFFFFFF00u;
        break;
    default: break;
    }
}

void Move17(std::uint32_t h) {
    switch ((h >> 8) % 8) {
    case 0: B(kHold) = static_cast<unsigned char>((h >> 12) & 1); break;
    case 1: SetL(kElevation, kElevations[(h >> 12) % (sizeof kElevations / sizeof kElevations[0])]); break;
    case 2: B(kPart) = static_cast<unsigned char>((h >> 12) % 4); break;
    case 3: SetW(kRollLine, (h >> 12) % 352); break;
    case 4: B(kRollScroll) = static_cast<unsigned char>((h >> 12) % 0x17); break;
    case 5: B(kCounter3) = static_cast<unsigned char>((h >> 12) & 3); break;
    case 6: SetL(0x905E60, h); break;   // Field_Kind2Z
    default: B(kRollSkip) = static_cast<unsigned char>((h >> 12) & 1); break;
    }
}
void Disturb17(std::uint32_t h) { Move17(h); }
void Settle17() {
    const std::uint32_t n = sh::Noise();
    if (n & 1) Move17(n >> 1);
    SetW(kGameStep, move_script::Word(Mem(kGameStep)) % 3);   // Scena17_EndTask indexes its table by it again each frame
}

// ================================================================================
// Chapters 18 and 19

const sh::Clone kClones18[] = {
    SC15_PLAIN(Scena18_Frame, 0x56D510, 0xE, kSlot),
    SC15_PLAIN(Scena18_Run, 0x56D520, 0xE, kState),
    SC15_PLAIN(Scena18_ObjectTrigger, 0x56D530, 0x1F, kObject),
    SC15_PLAIN(Scena18_EnterArea, 0x56D560, 0x8, kState),
};
const sh::DataTable kTables18[] = {{0x662C44, 3}, {0x662C50, 1}, {0x662C54, 1}};
ObjectTable g_objects18 = {0x662C54, 1, {}, false};
constexpr sh::CallSite kCalls56D580[] = {{0xA, 0x589810}};
const sh::Clone kClones19[] = {
    SC15_PLAIN(Scena19_Frame, 0x56D550, 0xE, kSlot),
    SC15_PLAIN(Scena19_Run, 0x56D570, 0xE, kState),
    SC15_CALLS(Scena19_Run0, 0x56D580, 0x31, kCalls56D580, kState),
    SC15_PLAIN(Scena19_ObjectTrigger, 0x56D5C0, 0x1F, kObject),
};
const sh::DataTable kTables19[] = {{0x662C6C, 3}, {0x662C78, 1}, {0x662C7C, 1}};
ObjectTable g_objects19 = {0x662C7C, 1, {}, false};

void SeedSmall(unsigned k, ObjectTable& t, unsigned object_k) {
    g_k = k;
    SwapObjects(t, k == object_k);
    B(kState) = static_cast<unsigned char>(sh::Next() % 3);
    B(kRun) = 0;   // one entry
    sh::SpriteRecord(0)[0x86] = 0;
    if (sh::Half()) B(kInputPressed) = static_cast<unsigned char>(sh::Next() & 8);
}
void Seed18(unsigned k) {
    g_chapter = 18;
    SeedSmall(k, g_objects18, 2);
}
void Seed19(unsigned k) {
    g_chapter = 19;
    SeedSmall(k, g_objects19, 3);
}

// The callees list with the object tables' stand-in appended: one array.
sh::Callee g_callees[kCalleesN + 1];

}  // namespace

void SelfTest() {
    for (unsigned i = 0; i < kCalleesN; ++i) g_callees[i] = kCallees[i];
    g_callees[kCalleesN] = kObjectCallee;
    FillRecords();
    const std::uint32_t kept_records = static_cast<std::uint32_t>(Long(Mem(kRecordTable)));

    g_sc15_enter_copy = CopyWithTramps("Scena15_EnterArea", 0x567DD0, 0x73B, kCalls567DD0, SH_N(kCalls567DD0), nullptr, 0);
    g_sc15_run6_copy = CopyWithTramps("Scena15_Run6", 0x569F20, 0xE54, kCalls569F20, SH_N(kCalls569F20), kTables569F20,
                                      SH_N(kTables569F20));
    g_endtask_copy = CopyWithTramps("Scena17_EndTask", 0x56D3B0, 0x2E, kCalls56D3B0, SH_N(kCalls56D3B0), nullptr, 0);
    g_sc15_endtask_theirs = reinterpret_cast<void*>(&TheirsEndTask);
    g_clones15[k15EnterArea].base = Wrapper(&Sc15EnterTheirs);
    g_clones15[k15Run6].base = Wrapper(&Sc15Run6Theirs);
    g_clones17[k17EndTask].base = Wrapper(&Sc15EndTaskTheirs);
    g_regions17[2].at = KeyOf(g_prims);
    g_regions17[3].at = KeyOf(g_text);

    sh::Group g15 = {"scena_sc15", g_clones15, k15Count, g_callees, kCalleesN + 1, kTables15, SH_N(kTables15),
                     kRegions15, SH_N(kRegions15), &Seed15, &Disturb15, 6000};
    g15.settle = &Settle15;
    g15.args = &Args15;
    g15.chapter = 15;
    sh::Run(g15);

    sh::Group g16 = {"scena_sc15", kClones16, SH_N(kClones16), g_callees, kCalleesN + 1, kTables16, SH_N(kTables16),
                     kRegionsSmall, SH_N(kRegionsSmall), &Seed16, nullptr, 4000};
    g16.args = &ArgsObject;
    g16.chapter = 16;
    sh::Run(g16);

    sh::Group g17 = {"scena_sc15", g_clones17, k17Count, g_callees, kCalleesN + 1, kTables17, SH_N(kTables17),
                     g_regions17, SH_N(g_regions17), &Seed17, &Disturb17, 6000};
    g17.settle = &Settle17;
    g17.args = &Args17;
    g17.chapter = 17;
    sh::Run(g17);

    sh::Group g18 = {"scena_sc15", kClones18, SH_N(kClones18), g_callees, kCalleesN + 1, kTables18, SH_N(kTables18),
                     kRegionsSmall, SH_N(kRegionsSmall), &Seed18, nullptr, 4000};
    g18.args = &ArgsObject;
    g18.chapter = 18;
    sh::Run(g18);

    sh::Group g19 = {"scena_sc15", kClones19, SH_N(kClones19), g_callees, kCalleesN + 1, kTables19, SH_N(kTables19),
                     kRegionsSmall, SH_N(kRegionsSmall), &Seed19, nullptr, 4000};
    g19.args = &ArgsObject;
    g19.chapter = 19;
    sh::Run(g19);

    SetLong(Mem(kRecordTable), static_cast<std::int32_t>(kept_records));
}

}  // namespace scena_sc15
#undef SH_N
