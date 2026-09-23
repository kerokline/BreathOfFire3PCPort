// BOF3X_SHADOW=mode_flow: a differential fuzz of the top-level task flow,
// once at start-up. docs/mode-flow.md section 6.
//
// Forty-two byte-copies, every call out re-aimed at a recording stand-in (the
// tail jumps included), Transition_Task's stack-built table re-aimed inside
// its copy, GameMode_Handlers' twelve entries swapped for recorders and four
// entries of Area_Descriptors pointed at descriptors of our own (whose +0x40
// is a recorder or null). One round: one function, random bytes in every
// region any of them touches, then each branch's boundaries seeded; theirs,
// then from the same state ours; the regions, the primitive pool, the packet
// cursor, the level word, the result and the stand-ins' log compared.
//
// The task bodies never return and several loops wait on a stand-in: the
// Task_Sleep recorder ends any round by a long jump after one to eight
// frames, so a loop that would run for ever is cut at the same frame on both
// sides. Gfx_ClutStripRestore, whose region is 32 KB, is checked in its own
// shorter loop.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/mode_flow_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace mode_flow {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the long jump out of a task body --------------------------------------
// Our own setjmp - ebx, esi, edi, ebp, esp and the return address - as
// mode_tasks.cpp has it (clang's __builtin_setjmp brought registers back
// wrong there).
std::uint32_t g_jump[6];
extern "C" __attribute__((naked, returns_twice)) int __cdecl ModeFlowJumpSave(std::uint32_t* buffer) {
    asm("movl 4(%esp), %eax\n\t"
        "movl %ebx, 0(%eax)\n\t"
        "movl %esi, 4(%eax)\n\t"
        "movl %edi, 8(%eax)\n\t"
        "movl %ebp, 12(%eax)\n\t"
        "leal 4(%esp), %ecx\n\t"
        "movl %ecx, 16(%eax)\n\t"
        "movl (%esp), %ecx\n\t"
        "movl %ecx, 20(%eax)\n\t"
        "xorl %eax, %eax\n\t"
        "ret");
}
extern "C" __attribute__((naked, noreturn)) void __cdecl ModeFlowJumpBack(std::uint32_t* buffer) {
    asm("movl 4(%esp), %eax\n\t"
        "movl 0(%eax), %ebx\n\t"
        "movl 4(%eax), %esi\n\t"
        "movl 8(%eax), %edi\n\t"
        "movl 12(%eax), %ebp\n\t"
        "movl 16(%eax), %esp\n\t"
        "movl 20(%eax), %ecx\n\t"
        "movl $1, %eax\n\t"
        "jmp *%ecx");
}

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed, g_sleeps, g_sleep_limit, g_load_waits;
bool g_area_wild;   // Disturb may put Game_AreaNumber at 0xBD (not while Area_Enter indexes the descriptors)

constexpr unsigned kPool = 0x200;
unsigned char g_prim[kPool];
std::int32_t g_level;                     // Transition_DrawTile's level, when the fuzz calls it directly
constexpr unsigned kAreas = 4;            // Game_AreaNumber 0..3 index descriptors of our own
unsigned char g_desc[kAreas][0x48];
unsigned char g_colour[kAreas][0x28];
unsigned char g_script[kAreas][8];

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p);
    const std::uint32_t pool = Address(g_prim), desc = Address(g_desc), colour = Address(g_colour),
                        script = Address(g_script);
    if (at >= pool && at < pool + sizeof g_prim) return 0x10000u + (at - pool);
    if (at >= desc && at < desc + sizeof g_desc) return 0x20000u + (at - desc);
    if (at >= colour && at < colour + sizeof g_colour) return 0x30000u + (at - colour);
    if (at >= script && at < script + sizeof g_script) return 0x40000u + (at - script);
    return at;
}

// Every byte below is one some function reads again, or stores, after a
// call - so a read moved before a call, or a store moved across one, shows.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    switch ((h >> 4) % 22) {
    case 0: Game_Mode = static_cast<std::uint16_t>(v % 12); break;
    // Inside Area_Enter only areas 0..2, whose descriptors all have a colour
    // matrix: the original re-reads +0x38 after testing it, and a call that
    // moved the area to one without would fault it (no callee does that).
    case 1: Game_AreaNumber = static_cast<std::uint16_t>(g_area_wild ? (v % 3 == 0 ? 0xBD : v % kAreas) : v % 3); break;
    case 2: Field_Request = static_cast<unsigned char>(v % 3 == 0 ? 5 : v % 3 == 1 ? 9 : v); break;
    case 3: At(0x905BA4)[0] = static_cast<unsigned char>(v); break;
    case 4: At(0x905BA5)[0] = static_cast<unsigned char>(v); break;
    case 5: At(0x9039A3)[0] = static_cast<unsigned char>(v); break;   // Field_ScriptFlags' bit 11
    case 6: Field_InputFlags = static_cast<unsigned char>(v); break;
    case 7: Field_MemberCount = static_cast<unsigned char>(v % 5); break;
    case 8: MoveScript_WaitWordDA = static_cast<std::uint16_t>(v % 2 ? 0 : v); break;
    case 9: Camera_Distance = static_cast<short>(v % 2 ? 0x5DC - 0x32 : static_cast<int>(h >> 16)); break;
    case 10: SetWord(At(0x929ECC), h >> 16); break;
    case 11: Cond_ByteFA = static_cast<signed char>(v % 2 ? 0xE + v % 4 : v); break;
    case 12: Field_StatusBits = static_cast<unsigned char>(v); break;
    case 13: At(at::kKind2XHigh)[0] = static_cast<unsigned char>(v); break;
    case 14: At(at::kStartArea + (v % 12))[0] = static_cast<unsigned char>(h >> 20); break;
    case 15: At(at::kAreaTrack)[0] = static_cast<unsigned char>(v % 3 == 0 ? 0xFF : h >> 20); break;
    case 16: At(at::kMusicTrack)[0] = static_cast<unsigned char>(v % 3 == 0 ? 0xFF : h >> 20); break;
    case 17: At(at::kPendingKind)[0] = static_cast<unsigned char>(v % 3 == 0 ? 0xFE : v % 3 == 1 ? 0xFF : h >> 20); break;
    case 18: At(at::kClock + v % 4)[0] = static_cast<unsigned char>(v % 2 ? 0 : h >> 20); break;
    case 19: Gfx_ClutStripDirty = static_cast<unsigned char>(v); break;
    case 20: Camera_Angles[0] = static_cast<short>(h >> 16); break;
    default: At(at::kKind2ZHigh)[0] = static_cast<unsigned char>(v); break;
    }
}

// --- the stand-ins ---------------------------------------------------------

void __cdecl StubSleep(int frames) {
    Record(1, static_cast<std::uint32_t>(frames));
    if (++g_sleeps >= g_sleep_limit) ModeFlowJumpBack(g_jump);
    Disturb();
}
void __cdecl StubTaskExit() { Record(2); Disturb(); }
void __cdecl StubTaskCreate(int slot, void* entry) { Record(3, static_cast<std::uint32_t>(slot), Address(entry)); Disturb(); }
void __cdecl StubClearPrivate() { Record(4); Disturb(); }
// The camera bytes kind 12 stores before its fade: the fade's frames run the
// field, which moves them, so a store moved after the call shows.
void DisturbCamera() {
    if (Hash() % 2 == 0) return;
    Camera_Distance = static_cast<short>(Hash() >> 16);
    MapView_Redraw = static_cast<unsigned char>(Hash() >> 5);
}
void __cdecl StubFadeSub(int step, unsigned semi, unsigned slot) {
    Record(5, static_cast<std::uint32_t>(step), semi, slot);
    DisturbCamera();
    Disturb();
}
void __cdecl StubFadeAdd(int step, unsigned semi, unsigned slot) {
    Record(6, static_cast<std::uint32_t>(step), semi, slot);
    DisturbCamera();
    Disturb();
}
// The real one reads and adds to the level's low word, reads the step's low
// word, the semi's low byte and the abr's low byte; the slot goes on whole to
// Gfx_CommitPrim, which masks it. It moves the level as the real one does, so
// that a caller that reset its level between calls would show.
unsigned char __cdecl StubDrawTile(short* level, int step, unsigned semi, unsigned slot, unsigned abr) {
    const auto before = static_cast<std::uint16_t>(*level);
    Record(7, before | static_cast<std::uint32_t>(step) << 16, semi & 0xFF, slot, abr & 0xFF);
    *level = static_cast<short>(before + static_cast<std::uint16_t>(step));
    Disturb();
    return static_cast<unsigned char>(Hash() % 3 == 0 ? 1 : 0);
}
void __cdecl StubWindowReset() { Record(8); Disturb(); }
void __cdecl StubClockTick() { Record(9); Disturb(); }
void __cdecl StubTransition(unsigned char kind) {
    Record(10, *reinterpret_cast<const volatile unsigned char*>(&kind));
    Disturb();
}
// The field frames it runs may change the input flags GameMode_Enter reads
// after it (bit 0, the entry list).
void __cdecl StubWaitTransition(unsigned char run) {
    Record(11, *reinterpret_cast<const volatile unsigned char*>(&run));
    if (Hash() % 2) Field_InputFlags = static_cast<unsigned char>(Field_InputFlags ^ 1);
    Disturb();
}
// Area_Enter keeps the area's low word; the flags' low byte is what GameMode_Enter
// has (docs/mode-flow.md section 3: its eax above the byte is Field_Task's 1).
// The real one sets bit 3 of Field_ScriptFlags2 (through Area_EntryWalk),
// which GameMode_Enter clears BEFORE calling it.
void __cdecl StubAreaEnter(unsigned area, int x, int z, unsigned flags) {
    Record(12, area & 0xFFFF, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), flags & 0xFF);
    if (Hash() % 2) At(0x905BA4)[0] = static_cast<unsigned char>(At(0x905BA4)[0] | 8);
    Disturb();
}
// What Area_Enter tests again after the direction-0 walk: bit 11 of
// Field_ScriptFlags and bit 15 of Field_ScriptFlags2.
void __cdecl StubEntryWalk(unsigned dir, int x, int z) {
    Record(13, dir, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    const std::uint32_t h = Hash();
    if (h % 3 == 0) At(0x905BA5)[0] = static_cast<unsigned char>(At(0x905BA5)[0] ^ 0x80);
    else if (h % 3 == 1) At(0x9039A3)[0] = static_cast<unsigned char>(At(0x9039A3)[0] ^ 8);
    Disturb();
}
void __cdecl StubTintReset() { Record(14); Disturb(); }
void __cdecl StubSlotRelease(unsigned slot) { Record(15, slot); Disturb(); }
void __cdecl StubSlotsRelease() { Record(16); Disturb(); }
void __cdecl StubClutRestore() { Record(17); Disturb(); }
// Not done for the round's first 0..3 questions, then done.
int __cdecl StubLoadDone() {
    Record(18);
    if (g_load_waits) {
        --g_load_waits;
        return 0;
    }
    return 1;
}
void __cdecl StubClearRect(int x, int y, int w, int h) {
    Record(19, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(w),
           static_cast<std::uint32_t>(h));
    Disturb();
}
unsigned __cdecl StubGetTPage(unsigned tp, unsigned abr, int x, int y) {
    Record(20, tp, abr, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    return Hash();   // the whole dword: the caller keeps 16 bits
}
constexpr std::uint32_t kPointZeroOne = 0x3C23D70Au;
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(21, Id(prim), static_cast<std::uint32_t>(dfe) | static_cast<std::uint32_t>(dtd) << 16, tpage, static_cast<std::uint32_t>(tw));
    SetLong(prim + 4, static_cast<std::int32_t>(0xE8000000u | (tpage & 0xFFFFu)));
    SetLong(prim + 8, static_cast<std::int32_t>(tw));
    Disturb();
}
// The commit moves the packet cursor, as the real one does - wrapping inside
// our pool - so that a cursor read too early shows.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(22, slot, size, Id(Gfx_PacketNext));
    Gfx_PacketNext = g_prim + (static_cast<unsigned>(Gfx_PacketNext - g_prim) + (size & 0xFF)) % 0x100u;
    Disturb();
}
// Scribbles over every byte the caller stores after it, as the real one's
// code byte and 0.01 floats do over some of them.
void __cdecl StubSetTile(unsigned char* prim) {
    Record(23, Id(prim));
    for (unsigned i = 4; i < 0x1C; ++i) prim[i] = static_cast<unsigned char>(Hash() >> (i % 24));
    prim[7] = 0x60;
    SetLong(prim + 0x10, static_cast<std::int32_t>(kPointZeroOne));
}
void __cdecl StubSetSemi(unsigned char* prim, unsigned abe) { Record(24, Id(prim), abe); prim[7] ^= 2; }
// GameMode_Start reads the saved position after it.
void __cdecl StubPartyLoad(unsigned slot) {
    Record(25, slot);
    if (Hash() % 2) At(at::kStartArea + (Hash() >> 8) % 12)[0] = static_cast<unsigned char>(Hash() >> 16);
    Disturb();
}
void __cdecl StubChangeArea(unsigned area, int x, int z, unsigned flags) {
    Record(26, area & 0xFFFF, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), flags & 0xFF);
    Disturb();
}
void __cdecl StubEntryPoint(unsigned x, unsigned z) { Record(27, x & 0xFF, z & 0xFF); Disturb(); }
void __cdecl StubZoneRoll(unsigned keep) { Record(28, keep); Disturb(); }
void __cdecl StubFieldFrame() { Record(29); Disturb(); }
void __cdecl StubEffectClear() { Record(30); Disturb(); }
void __cdecl StubMusicPlay(unsigned track, int unknown) { Record(31, track, static_cast<std::uint32_t>(unknown)); Disturb(); }
void __cdecl StubLoadDat(int index) { Record(32, static_cast<std::uint32_t>(index)); Disturb(); }
void __cdecl StubPartySetUp(long x, long z, unsigned flags) {
    Record(33, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), flags);
    Disturb();
}
unsigned char __cdecl StubZoneId(unsigned x, unsigned z) {
    Record(34, x & 0xFF, z & 0xFF);
    Disturb();
    return static_cast<unsigned char>(Hash() >> 8);
}
void __cdecl StubViewReset() { Record(35); Disturb(); }
// Area_Enter re-reads the descriptor after it: move the area, among those
// with a colour matrix of their own (see Disturb), half the time.
void __cdecl StubColourMatrix(const unsigned long* m) {
    Record(36, Id(m));
    if (!g_area_wild && Hash() % 2) Game_AreaNumber = static_cast<std::uint16_t>((Hash() >> 8) % 3);
    Disturb();
}
void __cdecl StubScenarioStart(int chapter) { Record(37, static_cast<std::uint32_t>(chapter) & 0xFF); Disturb(); }
void __cdecl StubRunPlacement(const unsigned char* script) { Record(38, Id(script)); Disturb(); }
void __cdecl StubFlagsClear(unsigned char* bits, unsigned index) { Record(39, Id(bits), index & 0xFF); Disturb(); }
void __cdecl StubModeDispatch() { Record(40); Disturb(); }
void __cdecl StubFirstFrame() { Record(41); Disturb(); }
unsigned char __cdecl StubTestFB(short x, short z) {
    Record(42, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z));
    Disturb();
    return static_cast<unsigned char>(Hash());
}
unsigned char __cdecl StubPlaceParty(unsigned flags) {
    Record(43, flags & 0xFF);
    Disturb();
    return static_cast<unsigned char>(Hash());
}
void __cdecl StubMembersFrame() { Record(44); Disturb(); }
void __cdecl StubDropped(unsigned char b) { Record(45, *reinterpret_cast<const volatile unsigned char*>(&b)); }
void __cdecl StubTitleTask() { Record(46); Disturb(); }
// Gfx_ClearImage reads the rect's four words and the low byte of each colour.
void __cdecl StubClearImage(const short* rect, unsigned char red, unsigned char green) {
    Record(47, static_cast<std::uint16_t>(rect[0]) | static_cast<std::uint32_t>(static_cast<std::uint16_t>(rect[1])) << 16,
           static_cast<std::uint16_t>(rect[2]) | static_cast<std::uint32_t>(static_cast<std::uint16_t>(rect[3])) << 16,
           *reinterpret_cast<const volatile unsigned char*>(&red), *reinterpret_cast<const volatile unsigned char*>(&green));
    Disturb();
}
// The 21 transition kinds, the 12 game modes and an area's +0x40 hook.
template <unsigned N> void __cdecl StubHandler() { Record(N); Disturb(); }

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5A9949: return f(&StubSleep);
    case 0x5A99AD: return f(&StubTaskExit);
    case 0x5A9914: return f(&StubTaskCreate);
    case 0x5A99F4: return f(&StubClearPrivate);
    case 0x495620: return f(&StubFadeSub);
    case 0x4956A0: return f(&StubFadeAdd);
    case 0x495750: return f(&StubDrawTile);
    case 0x59E330: return f(&StubWindowReset);
    case 0x496870: return f(&StubClockTick);
    case 0x495040: return f(&StubTransition);
    case 0x4967F0: return f(&StubWaitTransition);
    case 0x594E60: return f(&StubAreaEnter);
    case 0x595160: return f(&StubEntryWalk);
    case 0x454A20: return f(&StubTintReset);
    case 0x454A50: return f(&StubSlotRelease);
    case 0x454AB0: return f(&StubSlotsRelease);
    case 0x4549B0: return f(&StubClutRestore);
    case 0x454810: return f(&StubLoadDone);
    case 0x461E10: return f(&StubClearRect);
    case 0x5A79A0: return f(&StubGetTPage);
    case 0x5A77C0: return f(&StubDrawMode);
    case 0x461E50: return f(&StubCommit);
    case 0x5A7740: return f(&StubSetTile);
    case 0x5A7780: return f(&StubSetSemi);
    case 0x533CE0: return f(&StubPartyLoad);
    case 0x594E00: return f(&StubChangeArea);
    case 0x5951D0: return f(&StubEntryPoint);
    case 0x52FEB0: return f(&StubZoneRoll);
    case 0x517200: return f(&StubFieldFrame);
    case 0x5898A0: return f(&StubEffectClear);
    case 0x587AE0: return f(&StubMusicPlay);
    case 0x454590: return f(&StubLoadDat);
    case 0x533110: return f(&StubPartySetUp);
    case 0x595390: return f(&StubZoneId);
    case 0x56F670: return f(&StubViewReset);
    case 0x5A8DC0: return f(&StubColourMatrix);
    case 0x56D5E0: return f(&StubScenarioStart);
    case 0x579740: return f(&StubRunPlacement);
    case 0x57C110: return f(&StubFlagsClear);
    case 0x56D690: return f(&StubModeDispatch);
    case 0x5334A0: return f(&StubFirstFrame);
    case 0x572650: return f(&StubTestFB);
    case 0x531F90: return f(&StubPlaceParty);
    case 0x517350: return f(&StubMembersFrame);
    case 0x4DF820: return f(&StubDropped);
    case 0x4621C0: return f(&StubTitleTask);
    case 0x59E650: return f(&StubClearImage);
    default: bof3::Fatal("mode_flow: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

const Callees kStubs = {
    StubSleep, StubTaskExit, StubTaskCreate, StubClearPrivate,
    StubFadeSub, StubFadeAdd, StubDrawTile, StubWindowReset, StubClockTick,
    StubTransition, StubWaitTransition, StubAreaEnter, StubEntryWalk, StubTintReset,
    StubSlotRelease, StubSlotsRelease, StubClutRestore, StubLoadDone, StubClearRect,
    StubGetTPage, StubDrawMode, StubCommit, StubSetTile, StubSetSemi,
    StubPartyLoad, StubChangeArea, StubEntryPoint, StubZoneRoll,
    StubFieldFrame, StubEffectClear, StubMusicPlay, StubLoadDat, StubPartySetUp, StubZoneId, StubViewReset,
    StubColourMatrix, StubScenarioStart, StubRunPlacement, StubFlagsClear, StubModeDispatch, StubFirstFrame,
    StubTestFB, StubPlaceParty, StubMembersFrame, StubDropped,
    StubTitleTask, StubClearImage,
    {&StubHandler<100>, &StubHandler<101>, &StubHandler<102>, &StubHandler<103>, &StubHandler<104>,
     &StubHandler<105>, &StubHandler<106>, &StubHandler<107>, &StubHandler<108>, &StubHandler<109>,
     &StubHandler<110>, &StubHandler<111>, &StubHandler<112>, &StubHandler<113>, &StubHandler<114>,
     &StubHandler<115>, &StubHandler<116>, &StubHandler<117>, &StubHandler<118>, &StubHandler<119>,
     &StubHandler<120>},
};
const Handler kModeStubs[12] = {&StubHandler<130>, &StubHandler<131>, &StubHandler<132>, &StubHandler<133>,
                                &StubHandler<134>, &StubHandler<135>, &StubHandler<136>, &StubHandler<137>,
                                &StubHandler<138>, &StubHandler<139>, &StubHandler<140>, &StubHandler<141>};
const Handler kHook = &StubHandler<150>;

// Transition_Task's `mov [esp + k], imm32`: the offset of each imm32 in the
// copy (capstone, 2026-09-22) and the handler it names.
constexpr std::uint32_t kTableImm[21] = {0x0C, 0x19, 0x21, 0x29, 0x31, 0x39, 0x41, 0x49, 0x51, 0x59, 0x61,
                                         0x69, 0x71, 0x79, 0x81, 0x89, 0x91, 0x99, 0xA1, 0xA9, 0xB1};
constexpr std::uint32_t kKinds[21] = {0x495130, 0x495150, 0x495170, 0x495190, 0x4951B0, 0x4951D0, 0x4951F0,
                                      0x495210, 0x495230, 0x495250, 0x495270, 0x4952D0, 0x4953A0, 0x4953D0,
                                      0x4953F0, 0x495410, 0x495430, 0x495450, 0x4954B0, 0x495580, 0x4955A0};

struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; unsigned ret; };
// ret: 0 nothing to compare, 1 the low byte, 2 the whole dword

constexpr Call kStartCalls[] = {{0x1D, 0x5A9914}};
constexpr Call kSubWrap[] = {{0x9, 0x495620}};
constexpr Call kAddWrap[] = {{0x9, 0x4956A0}};
constexpr Call kKind12Calls[] = {{0x19, 0x495620}};
constexpr Call kSwingOutCalls[] = {{0x1F, 0x495750}, {0x4A, 0x5A9949}, {0x54, 0x5A99AD}};
constexpr Call kSwingInCalls[] = {{0x39, 0x495750}, {0x40, 0x5A9949}, {0x66, 0x495750}, {0x9D, 0x5A9949}, {0xC0, 0x5A99AD}};
constexpr Call kKind20Calls[] = {{0x17, 0x495750}, {0x3B, 0x5A9949}, {0x69, 0x5A99AD}};
constexpr Call kFadeSubCalls[] = {{0x33, 0x495750}, {0x41, 0x5A9949}, {0x74, 0x5A99AD}};
constexpr Call kFadeAddCalls[] = {{0x5C, 0x495750}, {0x6A, 0x5A9949}, {0x9D, 0x5A99AD}};
constexpr Call kTileCalls[] = {{0x18, 0x5A79A0}, {0x31, 0x5A77C0}, {0x3D, 0x461E50}, {0x49, 0x5A7740}, {0x8A, 0x5A7780}, {0x92, 0x461E50}};
constexpr Call kFieldTaskCalls[] = {{0xE, 0x5A99F4}, {0x13, 0x59E330}, {0x29, 0x496870}, {0x30, 0x5A9949}};
constexpr Call kModeStartCalls[] = {{0x12, 0x533CE0}, {0x74, 0x594E00}};
constexpr Call kModeEnterCalls[] = {{0x24, 0x594E60}, {0x50, 0x495040}, {0x57, 0x4967F0}, {0x78, 0x5951D0}, {0xBF, 0x52FEB0}};
constexpr Call kWaitCalls[] = {{0x7, 0x5A9949}, {0x1D, 0x517200}, {0x30, 0x5A9949}};
constexpr Call kAreaEnterCalls[] = {
    {0x23, 0x5898A0},  {0x28, 0x454A20},  {0x6B, 0x587AE0},  {0xA1, 0x454590},  {0xA9, 0x454810},
    {0xB4, 0x5A9949},  {0xBC, 0x454810},  {0xDE, 0x4549B0},  {0xF5, 0x533110},  {0x109, 0x595390},
    {0x116, 0x56F670}, {0x133, 0x5A8DC0}, {0x165, 0x5A8DC0}, {0x1A4, 0x56D5E0}, {0x1BF, 0x579740},
    {0x1E7, 0x57C110}, {0x1F6, 0x56D690}, {0x1FB, 0x5334A0}, {0x22C, 0x595160}, {0x23F, 0x595160},
    {0x252, 0x595160}, {0x26D, 0x595160}, {0x297, 0x572650}, {0x2DA, 0x531F90}, {0x2EB, 0x517350}};
constexpr Call kBootCalls[] = {{0x10, 0x461E10}, {0x16, 0x4DF820}, {0x20, 0x454590}, {0x28, 0x454810},
                               {0x33, 0x5A9949}, {0x3B, 0x454810}, {0xCC, 0x59E330}, {0xD1, 0x454A20},
                               {0xD6, 0x454AB0}, {0xFD, 0x4549B0}, {0x117, 0x5A9914}, {0x11F, 0x5A99AD}};
constexpr Call kTitleLoadCalls[] = {{0x5, 0x454590}, {0xD, 0x454810}, {0x18, 0x5A9949}, {0x20, 0x454810}, {0x29, 0x4621C0}};
constexpr Call kSlotsCalls[] = {{0x4, 0x454A50}};
constexpr Call kClearRectCalls[] = {{0x36, 0x59E650}};

enum : unsigned {
    kStart, kTask, kKind0,
    kFadeSub = kKind0 + 21, kFadeAdd, kTile, kFieldTask, kModeStart, kModeEnter, kWait, kClock, kAreaEnter, kWalk,
    kWindowReset, kBoot, kTitleLoad, kLoadDone, kClutRestore, kTintReset, kSlotRelease, kSlotsRelease, kClearRect,
    kCount
};

#define MF_C(name, base, size, calls, ret) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), ret}
#define MF_P(name, base, size, ret) {name, base, size, nullptr, 0, ret}
const Clone kClones[kCount] = {
    MF_C("Transition_Start", 0x495040, 0x26, kStartCalls, 0),
    MF_P("Transition_Task", 0x495070, 0xBD, 0),
    MF_C("Transition_Kind00", 0x495130, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind01", 0x495150, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind02", 0x495170, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind03", 0x495190, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind04", 0x4951B0, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind05", 0x4951D0, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind06", 0x4951F0, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind07", 0x495210, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind08", 0x495230, 0x12, kAddWrap, 0),
    MF_C("Transition_Kind09", 0x495250, 0x12, kAddWrap, 0),
    MF_C("Transition_Kind10", 0x495270, 0x5C, kSwingOutCalls, 0),
    MF_C("Transition_Kind11", 0x4952D0, 0xC9, kSwingInCalls, 0),
    MF_C("Transition_Kind12", 0x4953A0, 0x22, kKind12Calls, 0),
    MF_C("Transition_Kind13", 0x4953D0, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind14", 0x4953F0, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind15", 0x495410, 0x12, kAddWrap, 0),
    MF_C("Transition_Kind16", 0x495430, 0x12, kAddWrap, 0),
    MF_C("Transition_Kind17", 0x495450, 0x5C, kSwingOutCalls, 0),
    MF_C("Transition_Kind18", 0x4954B0, 0xC9, kSwingInCalls, 0),
    MF_C("Transition_Kind19", 0x495580, 0x12, kSubWrap, 0),
    MF_C("Transition_Kind20", 0x4955A0, 0x71, kKind20Calls, 0),
    MF_C("Transition_FadeSub", 0x495620, 0x7E, kFadeSubCalls, 0),
    MF_C("Transition_FadeAdd", 0x4956A0, 0xA2, kFadeAddCalls, 0),
    MF_C("Transition_DrawTile", 0x495750, 0xAD, kTileCalls, 1),
    MF_C("Field_Task", 0x495800, 0x3A, kFieldTaskCalls, 0),
    MF_C("GameMode_Start", 0x495840, 0xBB, kModeStartCalls, 0),
    MF_C("GameMode_Enter", 0x495900, 0xE1, kModeEnterCalls, 0),
    MF_C("Field_WaitTransition", 0x4967F0, 0x3A, kWaitCalls, 0),
    MF_P("Game_ClockTick", 0x496870, 0x18F, 0),
    MF_C("Area_Enter", 0x594E60, 0x2FD, kAreaEnterCalls, 0),
    MF_P("Area_EntryWalk", 0x595160, 0x67, 0),
    MF_P("Window_ResetAll", 0x59E330, 0x26, 0),
    MF_C("Boot_Task", 0x496B60, 0x127, kBootCalls, 0),
    MF_C("Title_LoadTask", 0x496C90, 0x2E, kTitleLoadCalls, 0),
    MF_P("File_LoadDone", 0x454810, 0x6, 2),
    MF_P("Gfx_ClutStripRestore", 0x4549B0, 0x34, 0),
    MF_P("MoveScript_TintReset", 0x454A20, 0x25, 0),
    MF_P("Field_SlotRelease", 0x454A50, 0x21, 0),
    MF_C("Field_SlotsReleaseAll", 0x454AB0, 0x14, kSlotsCalls, 0),
    MF_C("Gfx_ClearRect", 0x461E10, 0x3F, kClearRectCalls, 0),
};
#undef MF_C
#undef MF_P

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x66C7D0, 0x60},          // Task_Records: Field_Request, Game_Mode / Step, the title's words, the wait word, the kind
    {0x9039A0, 0x40},          // Field_ScriptFlags, the pool pointer 0x9039D8
    {at::kBootClear, at::kBootClearBytes},   // the game state Boot_Task zeroes: the start position, the config bytes,
                                             // Cond_Flags, the clock, the party byte, the track, the countdowns
    {0x904A90, 0x70},          // 0x904AAA, 0x904AE5
    {at::kAreaTrack, 1},
    {at::kAreaTransition, 0x1E},   // the kind, the entry walk, Game_AreaNumber
    {0x903580, 0xC0},          // the buttons, Light_Angles, Field_Slots
    {0x9036D0, 1},
    {0x903840, 0x100},         // Camera_Distance, the pending x and z, the six hold bytes
    {0x905B80, 0x28},          // Field_EdgeBits, the pending flags, Field_InputFlags, Field_ScriptFlags2
    {0x905E60, 0x10},          // Field_Kind2Z / X, the last zone, MapView_Redraw
    {0x929EC0, 0x10},          // Field_MemberCount and the byte after it, Camera_Angles, the angle word
    {0x92BF19, 1},             // Draw_OtSlot
    {0x937F80, 0x1C},          // the pending area, Gfx_ClutStripDirty, the pending kind
    {at::kTintMarks, 0x1E4},   // the -1 marks, MoveScript_TintRecords, the pool pointer 0x7E0880
    {0x7E0918, 0xC},           // Draw_PassFlags, the last Kind2 X / Z
    {0x7E0940, 4},             // Sprite_Kind2 + 1..3
    {at::kLastArea, 2},
    {at::kWindowPass, 0x760},  // the pass byte, the members' walk bytes, the 22 window records
    {0x8034E0, 0x12},          // Cond_ByteFA, Field_StatusBits and the byte after, Cond_ByteFD
    {at::kWalkPoses, 4},       // constant data, read by Area_EntryWalk: random here, put back after
    {at::kButtonDefaults, 0x12},
    {at::kDefaultLight, 8},
};
constexpr unsigned kRegionBytes = 0x60 + 0x40 + at::kBootClearBytes + 0x70 + 1 + 0x1E + 0xC0 + 1 + 0x100 + 0x28 + 0x10 +
                                  0x10 + 1 + 0x1C + 0x1E4 + 0xC + 4 + 2 + 0x760 + 0x12 + 4 + 0x12 + 8;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char prim[kPool];
    unsigned char desc[sizeof g_desc];
    unsigned char colour[sizeof g_colour];
    std::int32_t level;
    std::uint32_t packet;   // Gfx_PacketNext, as an offset into the pool
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.prim, g_prim, sizeof g_prim);
    std::memcpy(s.desc, g_desc, sizeof g_desc);
    std::memcpy(s.colour, g_colour, sizeof g_colour);
    s.level = g_level;
    s.packet = static_cast<std::uint32_t>(Gfx_PacketNext - g_prim);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_prim, s.prim, sizeof g_prim);
    std::memcpy(g_desc, s.desc, sizeof g_desc);
    std::memcpy(g_colour, s.colour, sizeof g_colour);
    g_level = s.level;
    Gfx_PacketNext = g_prim + s.packet;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
    g_sleeps = 0;
}
unsigned Byte(const State& s, std::uint32_t address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + (address - r.at)];
        at += r.size;
    }
    return 0x100;
}

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Garbage(std::uint32_t low_bits, std::uint32_t value) { return (Next() & ~low_bits) | value; }

// Random bytes put back inside what the original's tables and our buffers
// hold: the mode index, the area index (our four descriptors), the member
// count (the entry walk writes a record per member), the transition kind.
void Fix() {
    Game_Mode = static_cast<std::uint16_t>(Next() % 12);
    Game_AreaNumber = static_cast<std::uint16_t>(Next() % kAreas);
    Field_MemberCount = static_cast<unsigned char>(Next() % 5);
    SetWord(At(at::kTransitionKind), Next() % 21);
    if (Often()) MoveScript_WaitWordDA = 0;
    for (unsigned a = 0; a < kAreas; ++a) {
        unsigned char* const d = g_desc[a];
        SetLong(d, static_cast<std::int32_t>(Address(g_script[a])));
        // area 3 alone may have no colour matrix of its own (see Disturb)
        SetLong(d + 0x38, a < 3 || Half() ? static_cast<std::int32_t>(Address(g_colour[a])) : 0);
        SetLong(d + 0x40, Half() ? static_cast<std::int32_t>(Address(reinterpret_cast<const void*>(kHook))) : 0);
    }
}

struct Args { std::uint32_t a[5]; };

// Each branch's boundaries, on top of the random bytes.
Args Seed(unsigned k) {
    Args args;
    for (std::uint32_t& v : args.a) v = Next();
    static const std::uint32_t kSteps[] = {0x800, 0xFFFFF800u, 0x2000, 0x400, 0x200, 0x100, 0, 1, 0xFFFFFFFFu,
                                           0x7FFF, 0x8000, 0x10000, 0x1FFFF, 0xFFFF0001u};
    static const std::uint16_t kLevels[] = {0, 1, 0x7F, 0x80, 0x7FFF, 0x8000, 0xFFFF, 0x7800, 0x7F80};
    g_area_wild = true;
    switch (k) {
    case kStart:
        args.a[0] = Garbage(0xFF, Next() % 22);
        if (Often()) MoveScript_WaitWordDA = 0;
        else if (Half()) MoveScript_WaitWordDA = 0x100;   // the high byte alone set
        break;
    case kFadeSub:
    case kFadeAdd:
        args.a[0] = kSteps[Next() % 14];
        args.a[1] = Half() ? 1 : Next();
        args.a[2] = Half() ? Next() % 3 : Next();
        break;
    case kTile:
        g_level = static_cast<std::int32_t>(Garbage(0xFFFF, kLevels[Next() % 9]));
        args.a[1] = kSteps[Next() % 14];
        args.a[2] = Half() ? 1 : Next();
        args.a[3] = Half() ? Next() % 3 : Next();
        args.a[4] = Half() ? 1 + Next() % 2 : Next();
        // a level the step brings to exactly 0: not below zero
        if (Next() % 4 == 0) g_level = static_cast<std::int32_t>(Garbage(0xFFFF, (0x10000u - (args.a[1] & 0xFFFFu)) & 0xFFFFu));
        break;
    case kKind0 + 11:
    case kKind0 + 18:
        // the distance a whole number of 50s below the stop, or not
        Camera_Distance = static_cast<short>(Often() ? 0x5DC - 0x32 * static_cast<int>(Next() % 4) : static_cast<int>(Next()));
        break;
    case kKind0 + 20:
        SetLong(At(0x929ECC), static_cast<std::int32_t>(Next()));
        break;
    case kModeStart:
        for (unsigned i = 0; i < 4; ++i) if (Half()) At(at::kClock + i)[0] = 0;
        if (Half()) At(at::kMusicTrack)[0] = 0xFF;
        break;
    case kModeEnter: {
        static const unsigned char kKinds_[] = {0xFE, 0xFF, 0, 1, 0xA, 0x14};
        At(at::kPendingKind)[0] = Often() ? kKinds_[Next() % 6] : static_cast<unsigned char>(Next());
        Field_InputFlags = static_cast<unsigned char>(Often() ? Next() % 16 : Next());
        if (Half()) Game_AreaNumber = 0xBD;
        break;
    }
    case kWait:
        args.a[0] = Garbage(0xFF, Half() ? 0 : Next() % 3);
        if (Half()) MoveScript_WaitWordDA = 0;
        break;
    case kClock: {
        unsigned char* const c = At(at::kClock);
        Field_Request = static_cast<unsigned char>(Half() ? 9 : Next() % 11);
        static const unsigned char kHours[] = {0, 0x62, 0x63, 0x64, 0xFF};
        static const unsigned char kSixty[] = {0, 0x3A, 0x3B, 0x3C, 0x7F, 0x80, 0xFF};
        static const unsigned char kFrames[] = {0, 0x1C, 0x1D, 0x1E, 0x7F, 0x80, 0xFF};
        if (Often()) c[0] = kHours[Next() % 5];
        if (Often()) c[1] = kSixty[Next() % 7];
        if (Often()) c[2] = Half() ? c[1] : kSixty[Next() % 7];
        if (Often()) c[3] = kFrames[Next() % 7];
        // a quarter of the rounds at the stop: 99:59, the seconds and frames
        // either side of their rollovers
        if (Next() % 4 == 0) {
            c[0] = 0x63;
            c[1] = 0x3B;
            c[2] = static_cast<unsigned char>(0x3A + Next() % 3);
            c[3] = static_cast<unsigned char>(0x1C + Next() % 2);
        }
        for (std::uint32_t t : {at::kCountdownA, at::kCountdownB}) {
            unsigned char* const d = At(t);
            if (Half()) { d[0] = 0; d[1] = 0; d[2] = 0; d[3] = 0; }
            if (Often()) d[0] = static_cast<unsigned char>(Half() ? 0 : Next() % 3);
            if (Often()) d[1] = kSixty[Next() % 7];
            if (Often()) d[2] = kSixty[Next() % 7];
            if (Often()) d[3] = kFrames[Next() % 7];
        }
        break;
    }
    case kAreaEnter:
        g_area_wild = false;
        args.a[0] = Garbage(0xFFFF, Next() % kAreas);
        args.a[3] = Half() ? (Next() | 0x80) : (Next() & ~0x80u);
        if (Half()) Field_Request = 5;
        if (Half()) At(at::kAreaTrack)[0] = 0xFF;
        if (Half()) At(0x905BA5)[0] = static_cast<unsigned char>(Half() ? 0x80 : 0);
        if (Half()) At(0x9039A3)[0] = static_cast<unsigned char>(Half() ? 8 : 0);
        if (Half()) Cond_ByteFA = static_cast<signed char>(0xE + Next() % 4);
        break;
    case kWalk:
        args.a[0] = Half() ? Next() % 4 : Garbage(0xFF, Next() % 4);
        break;
    case kBoot:
    case kTitleLoad:
        break;
    case kSlotRelease:
        args.a[0] = Garbage(0xFF, Next() % 8);
        break;
    default:
        break;
    }
    if (k == kAreaEnter || k == kModeEnter) {
        // the flags words' bits the ends of Area_Enter and GameMode_Enter test
        if (Half()) At(0x905BA4)[0] = static_cast<unsigned char>(Next() & 0x80);
        if (Half()) At(0x905BA5)[0] = static_cast<unsigned char>(Next() & 0x83);
        if (Half()) Field_MemberCount = static_cast<unsigned char>(Next() % 4);
    }
    g_load_waits = Next() % 4;
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[160];
    unsigned clock_frames, clock_minutes, clock_hours, clock_capped, count_hour_down, count_zero;
    unsigned area_loaded, area_own_colour, area_scenario, area_skip_10, area_drop_in, area_members_tail;
    unsigned enter_mode8, enter_fe, enter_transition, enter_bd;
    unsigned swing_in_done, wait_returned, cut_short;
} g_cover;
void Cover(unsigned k, const State& in, const State& out, bool cut) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 160) ++g_cover.logged[out.log[i].what];
    if (cut) ++g_cover.cut_short;
    switch (k) {
    case kClock:
        if (Byte(in, at::kClock + 3) != Byte(out, at::kClock + 3) && Byte(out, at::kClock + 3) == 0) ++g_cover.clock_frames;
        if (Byte(in, at::kClock + 1) != Byte(out, at::kClock + 1)) ++g_cover.clock_minutes;
        if (Byte(in, at::kClock) != Byte(out, at::kClock)) ++g_cover.clock_hours;
        if (Byte(in, at::kClock) == 0x63 && Byte(in, at::kClock + 1) == 0x3B && Byte(in, at::kClock + 2) == 0x3B) ++g_cover.clock_capped;
        for (std::uint32_t t : {at::kCountdownA, at::kCountdownB}) {
            if (Byte(out, t) != Byte(in, t)) ++g_cover.count_hour_down;
            if (Byte(in, t) == 0 && Byte(in, t + 1) == 0 && Byte(out, t + 3) == 0x1D && Byte(in, t + 3) == 0) ++g_cover.count_zero;
        }
        break;
    case kAreaEnter: {
        bool loaded = false, colour = false;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
            const Entry& e = out.log[i];
            if (e.what == 32) loaded = true;
            if (e.what == 36 && e.a >= 0x30000 && e.a < 0x40000) colour = true;
            if (e.what == 37) { ++g_cover.area_scenario; if (e.a == 0x11) ++g_cover.area_skip_10; }
            if (e.what == 43) ++g_cover.area_drop_in;
        }
        if (loaded) ++g_cover.area_loaded;
        if (colour) ++g_cover.area_own_colour;
        if ((Byte(out, 0x905BA4) & 0x12) == 0x12) ++g_cover.area_members_tail;
        break;
    }
    case kModeEnter:
        if ((Byte(out, 0x66C7E8) | Byte(out, 0x66C7E9) << 8) == 8) ++g_cover.enter_mode8;
        if (Byte(in, at::kPendingKind) == 0xFE) ++g_cover.enter_fe;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 10) ++g_cover.enter_transition;
        if (Byte(out, at::kAreaTransition) == 0x14) ++g_cover.enter_bd;
        break;
    case kKind0 + 11:
    case kKind0 + 18:
        if (!cut) ++g_cover.swing_in_done;
        break;
    case kWait:
        if (!cut) ++g_cover.wait_returned;
        break;
    default:
        break;
    }
}

using Fn5 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

// One call, cut short by the Task_Sleep recorder's long jump. Returns false
// when it was cut.
std::uint32_t g_result;
__attribute__((noinline)) bool Run(const void* fn, const Args& args) {
    g_result = 0;
    if (ModeFlowJumpSave(g_jump) != 0) return false;
    g_result = reinterpret_cast<Fn5>(const_cast<void*>(fn))(args.a[0], args.a[1], args.a[2], args.a[3], args.a[4]);
    return true;
}

void PatchTable(void* copy) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (unsigned i = 0; i < 21; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + kTableImm[i], sizeof had);
        if (had != kKinds[i])
            bof3::Fatal("mode_flow: Transition_Task +0x%X holds 0x%X, not the kind 0x%X", static_cast<unsigned>(kTableImm[i]),
                        static_cast<unsigned>(had), static_cast<unsigned>(kKinds[i]));
        const std::uint32_t to = Address(reinterpret_cast<const void*>(kStubs.transitions[i]));
        std::memcpy(code + kTableImm[i], &to, sizeof to);
    }
}

// Gfx_ClutStripRestore alone: no calls, 32 KB of memory. Twenty rounds.
unsigned CheckClutRestore(const void* theirs) {
    constexpr std::uint32_t kFrom = 0x80B580, kBytes = 0x8000;
    static unsigned char saved[kBytes], input[kBytes], out_theirs[kBytes];
    std::memcpy(saved, At(kFrom), kBytes);
    unsigned bad = 0;
    for (unsigned round = 0; round < 20; ++round) {
        for (unsigned i = 0; i < kBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input + i, &v, 4);
        }
        std::memcpy(At(kFrom), input, kBytes);
        reinterpret_cast<void (__cdecl*)()>(const_cast<void*>(theirs))();
        std::memcpy(out_theirs, At(kFrom), kBytes);
        std::memcpy(At(kFrom), input, kBytes);
        Gfx_ClutStripRestore();
        if (std::memcmp(out_theirs, At(kFrom), kBytes) != 0) ++bad;
    }
    std::memcpy(At(kFrom), saved, kBytes);
    return bad;
}

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("mode_flow: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[32];
        if (c.n_calls > 32) bof3::Fatal("mode_flow: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    PatchTable(clones[kTask]);

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Transition_Start), reinterpret_cast<const void*>(&Transition_Task),
        reinterpret_cast<const void*>(&Transition_Kind00), reinterpret_cast<const void*>(&Transition_Kind01),
        reinterpret_cast<const void*>(&Transition_Kind02), reinterpret_cast<const void*>(&Transition_Kind03),
        reinterpret_cast<const void*>(&Transition_Kind04), reinterpret_cast<const void*>(&Transition_Kind05),
        reinterpret_cast<const void*>(&Transition_Kind06), reinterpret_cast<const void*>(&Transition_Kind07),
        reinterpret_cast<const void*>(&Transition_Kind08), reinterpret_cast<const void*>(&Transition_Kind09),
        reinterpret_cast<const void*>(&Transition_Kind10), reinterpret_cast<const void*>(&Transition_Kind11),
        reinterpret_cast<const void*>(&Transition_Kind12), reinterpret_cast<const void*>(&Transition_Kind13),
        reinterpret_cast<const void*>(&Transition_Kind14), reinterpret_cast<const void*>(&Transition_Kind15),
        reinterpret_cast<const void*>(&Transition_Kind16), reinterpret_cast<const void*>(&Transition_Kind17),
        reinterpret_cast<const void*>(&Transition_Kind18), reinterpret_cast<const void*>(&Transition_Kind19),
        reinterpret_cast<const void*>(&Transition_Kind20), reinterpret_cast<const void*>(&Transition_FadeSub),
        reinterpret_cast<const void*>(&Transition_FadeAdd), reinterpret_cast<const void*>(&Transition_DrawTile),
        reinterpret_cast<const void*>(&Field_Task), reinterpret_cast<const void*>(&GameMode_Start),
        reinterpret_cast<const void*>(&GameMode_Enter), reinterpret_cast<const void*>(&Field_WaitTransition),
        reinterpret_cast<const void*>(&Game_ClockTick), reinterpret_cast<const void*>(&Area_Enter),
        reinterpret_cast<const void*>(&Area_EntryWalk), reinterpret_cast<const void*>(&Window_ResetAll),
        reinterpret_cast<const void*>(&Boot_Task), reinterpret_cast<const void*>(&Title_LoadTask),
        reinterpret_cast<const void*>(&File_LoadDone), reinterpret_cast<const void*>(&Gfx_ClutStripRestore),
        reinterpret_cast<const void*>(&MoveScript_TintReset), reinterpret_cast<const void*>(&Field_SlotRelease),
        reinterpret_cast<const void*>(&Field_SlotsReleaseAll), reinterpret_cast<const void*>(&Gfx_ClearRect)};

    static State saved, input, their_out, our_out;
    unsigned char* const saved_packet = Gfx_PacketNext;
    void* saved_modes[12];
    std::memcpy(saved_modes, GameMode_Handlers, sizeof saved_modes);
    unsigned char* saved_desc[kAreas];
    for (unsigned a = 0; a < kAreas; ++a) saved_desc[a] = Area_Descriptors[a];
    Gfx_PacketNext = g_prim;
    Capture(saved);
    g = kStubs;
    for (unsigned i = 0; i < 12; ++i) GameMode_Handlers[i] = reinterpret_cast<void*>(kModeStubs[i]);
    for (unsigned a = 0; a < kAreas; ++a) Area_Descriptors[a] = g_desc[a];

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        if (k == kClutRestore) continue;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.prim) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.desc) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.colour) b = static_cast<unsigned char>(Next());
        input.level = static_cast<std::int32_t>(Next());
        input.packet = Next() % 0x80u;
        input.result = 0;
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        g_seed = Next();
        g_sleep_limit = 1 + Next() % 8;
        Args args = Seed(k);
        if (k == kTile) args.a[0] = Address(&g_level);
        const unsigned load_waits = g_load_waits;
        Capture(input);

        bool finished[2];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            g_load_waits = load_waits;
            State& out = pass ? our_out : their_out;
            finished[pass] = Run(pass ? ours[k] : clones[k], args);
            Capture(out);
            out.result = kClones[k].ret == 0 ? 0u : kClones[k].ret == 1 ? (g_result & 0xFFu) : g_result;
        }
        calls += their_out.log_n;
        Cover(k, input, their_out, !finished[0]);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 || finished[0] != finished[1]) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      mode_flow self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    std::memcpy(GameMode_Handlers, saved_modes, sizeof saved_modes);
    for (unsigned a = 0; a < kAreas; ++a) Area_Descriptors[a] = saved_desc[a];
    Apply(saved);
    Gfx_PacketNext = saved_packet;

    const unsigned clut_bad = CheckClutRestore(clones[kClutRestore]);
    if (clut_bad) {
        ++bad_per[kClutRestore];
        bad += clut_bad;
        bof3::Log("shadow      mode_flow self-test MISMATCH: Gfx_ClutStripRestore in %u of 20 rounds", clut_bad);
    }

    bof3::Log("shadow      mode_flow self-test: %u rounds over %u functions (%u each) and 20 of Gfx_ClutStripRestore, "
              "%u calls to the stand-ins, %u MISMATCHES; the task words, the game state block, the field and camera "
              "bytes, the window records, the descriptors, the primitive pool, the packet cursor, the level, the result "
              "and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount - 1), kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      mode_flow: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned kinds = 0, modes = 0;
    for (unsigned i = 100; i < 121; ++i) kinds += c.logged[i] ? 1u : 0u;
    for (unsigned i = 130; i < 142; ++i) modes += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      mode_flow coverage: transition kinds dispatched %u of 21, game modes %u of 12, area hook %u; "
              "tiles %u, sleeps %u, rounds cut by the sleep %u; clock frames rolled %u, minutes moved %u, hours moved %u, "
              "capped %u, a countdown's hours moved %u, a countdown round 0:00:00 %u; area DAT loaded %u, own colour %u, "
              "scenario %u (0x10 skipped %u), dropped in %u, member tail %u, walks %u, TestFB %u; enter mode 8 %u, "
              "kind FE %u, transitions %u, area BD %u; swing-in finished %u; wait returned %u",
              kinds, modes, c.logged[150], c.logged[7], c.logged[1], c.cut_short, c.clock_frames, c.clock_minutes,
              c.clock_hours, c.clock_capped, c.count_hour_down, c.count_zero, c.area_loaded, c.area_own_colour,
              c.area_scenario, c.area_skip_10, c.area_drop_in, c.area_members_tail, c.logged[13], c.logged[42],
              c.enter_mode8, c.enter_fe, c.enter_transition, c.enter_bd, c.swing_in_done, c.wait_returned);
    if (bad) bof3::Fatal("the top-level task flow differs from the original in %u self-test rounds", bad);
}

}  // namespace mode_flow
