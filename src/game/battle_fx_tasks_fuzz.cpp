// BOF3X_SHADOW=battle_fx_tasks: a differential fuzz of the battle effect
// tasks, once at start-up. docs/battle_fx_tasks.md section 4.
//
// Nineteen byte-copies, every call and tail jump out re-aimed at a recording
// stand-in (bof3::CloneCall with `expected`); the six tables the originals
// build on their stacks re-aimed inside the copies (their immediates checked
// first); BattleFx_DamagePopup's jump table relocated into its copy; the 151
// code pointers of Magic_Rows pointed at recorders. One round: one function,
// random bytes in every region any of them touches, the pointers and indices
// put back inside what the tables hold, each branch's boundaries seeded;
// theirs, then from the same state ours; the regions, the result and the
// stand-ins' log compared. Everything is put back afterwards.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/battle_fx_tasks_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_fx_tasks {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Slot(unsigned i) { return At(at::kTasks + i * at::kTaskSize); }
unsigned char* EnemyObj(unsigned i) { return At(at::kEnemies + i * at::kEnemySize); }
void SetPtr(std::uint32_t address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }
unsigned char* PtrAt(std::uint32_t address) { return At(static_cast<std::uint32_t>(Long(At(address)))); }
unsigned char* CurSlot() { return PtrAt(at::kTaskCurrent); }
unsigned char* Owner() { return PtrAt(at::kTaskOwner); }
unsigned char* OwnerOf(const unsigned char* slot) { return At(static_cast<std::uint32_t>(Long(slot + 0x80))); }

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 64;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

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

// A slot, or (for an owner) a slot or an enemy object.
unsigned char* AnySlot(std::uint32_t v) { return Slot(v % at::kTaskCount); }
unsigned char* AnyOwner(std::uint32_t v) { return v % 3 == 0 ? EnemyObj((v >> 2) % 8) : Slot((v >> 2) % at::kTaskCount); }

// Every byte below is one some function reads again after a call, or reads
// only after one - so a read moved before a call, or a store moved across
// one, shows. Pointers stay inside the arrays, indices inside the tables.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 14) {
    case 0: Sprite_Current = AnySlot(w); break;
    case 1: SetPtr(at::kTaskCurrent, AnySlot(w)); break;
    case 2: SetPtr(at::kTaskOwner, AnyOwner(w)); break;
    case 3: CurSlot()[1] = static_cast<unsigned char>(v % 6); break;
    case 4: CurSlot()[7] = static_cast<unsigned char>(v % 5); break;
    case 5: Sprite_Current[0] = static_cast<unsigned char>(v); break;
    case 6: At(at::kPhase)[0] = static_cast<unsigned char>(v % 7); break;
    case 7: At(at::kRoundCount)[0] = static_cast<unsigned char>(v); break;
    case 8: Owner()[0] = static_cast<unsigned char>(v); break;
    case 9: Owner()[5] = static_cast<unsigned char>(v % 11); break;
    case 10: CurSlot()[0x27] = static_cast<unsigned char>(v); break;
    case 11: SetWord(CurSlot() + 0x36 + 4 * (w % 2), h >> 16); break;
    case 12: SetLong(At(at::kAnimSet), static_cast<std::int32_t>(h)); break;
    default: SetWord(CurSlot() + 0x60, h >> 16); break;
    }
}

// --- the stand-ins ---------------------------------------------------------

// The stack tables' entries: each records both current pointers and the
// animation set pointer (which the pose task and the actor watch set before
// the call and put back after it).
template <unsigned N> void __cdecl StubHandler() {
    Record(N, Address(Sprite_Current), Address(CurSlot()), static_cast<std::uint32_t>(Long(At(at::kAnimSet))));
    Disturb();
}
// Magic_Rows' 151 code pointers.
template <unsigned N> void __cdecl StubRow() {
    Record(400 + N, Address(Sprite_Current), Address(CurSlot()));
    Disturb();
}
template <std::size_t... I> constexpr auto MakeRows(std::index_sequence<I...>) {
    return std::array<Handler, sizeof...(I)>{&StubRow<static_cast<unsigned>(I)>...};
}

// BattleFx_RollingDigits reads the CLUT row's low byte; Battle_DrawNumber the
// low words of x, y and the value and the CLUT row's low byte;
// Battle_DrawLabel the same, and the cell's low byte.
void __cdecl StubDigits(unsigned clut) { Record(1, clut & 0xFF); Disturb(); }
void __cdecl StubNumber(int x, int y, unsigned clut, unsigned value) {
    Record(2, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF, clut & 0xFF, value & 0xFFFF);
    Disturb();
}
void __cdecl StubLabel(int x, int y, unsigned clut, unsigned cell) {
    Record(3, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF, clut & 0xFF, cell & 0xFF);
    Disturb();
}
// The free moves the phase half the time: the follower reads it after the
// first free.
void __cdecl StubFree() {
    Record(4, Address(CurSlot()));
    if (Hash() % 2) At(at::kPhase)[0] = static_cast<unsigned char>((Hash() >> 8) % 3);
    Disturb();
}
// The actor test moves the owner pointer and the owner's actor half the time:
// the watch re-reads both after it.
unsigned char __cdecl StubActorIsOut(unsigned a) {
    Record(5, a & 0xFF);
    const std::uint32_t h = Hash();
    if (h % 2) SetPtr(at::kTaskOwner, AnyOwner(h >> 8));
    if ((h >> 1) % 2) Owner()[5] = static_cast<unsigned char>((h >> 16) % 11);
    Disturb();
    const std::uint32_t r = Hash();
    return static_cast<unsigned char>(r % 3 == 0 ? (r >> 8) & 0xFF : r % 3 == 1 ? 1 : 0);
}
void __cdecl StubSetAnimation(unsigned char a) { Record(6, a, Address(Sprite_Current)); Disturb(); }
void __cdecl StubQueueOverlay() { Record(7, Address(Sprite_Current)); Disturb(); }
unsigned char __cdecl StubScriptTick() {
    Record(8, Address(Sprite_Current));
    Disturb();
    const std::uint32_t r = Hash();
    return static_cast<unsigned char>(r % 2 ? 0 : r >> 8);
}
// The screen update moves the owner pointer or its byte +0 half the time:
// the follower tests the owner's bit 0 after it.
void __cdecl StubUpdateScreen() {
    Record(9, Address(Sprite_Current), Address(Owner()));
    const std::uint32_t h = Hash();
    if (h % 2) SetPtr(at::kTaskOwner, AnyOwner(h >> 8));
    if ((h >> 1) % 2) Owner()[0] = static_cast<unsigned char>(Owner()[0] ^ 1);
    Disturb();
}
void __cdecl StubScriptFlags() { Record(10, At(at::kScriptVar7)[0], At(at::kScriptVar7 + 1)[0]); Disturb(); }
void __cdecl StubAfterArea() { Record(11); Disturb(); }
// The transition moves 0x904AA2 half the time: the hook adds to it after.
void __cdecl StubTransition(unsigned char k) {
    Record(12, k);
    if (Hash() % 2) At(at::kRoundCount)[0] = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
}

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x432F10: return f(&StubDigits);
    case 0x444480: return f(&StubNumber);
    case 0x4445A0: return f(&StubLabel);
    case 0x4351F0: return f(&StubFree);
    case 0x4456C0: return f(&StubActorIsOut);
    case 0x5891F0: return f(&StubSetAnimation);
    case 0x5890E0: return f(&StubQueueOverlay);
    case 0x5893A0: return f(&StubScriptTick);
    case 0x588F20: return f(&StubUpdateScreen);
    case 0x57C7C0: return f(&StubScriptFlags);
    case 0x446E20: return f(&StubAfterArea);
    case 0x495040: return f(&StubTransition);
    default: bof3::Fatal("battle_fx_tasks: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

template <unsigned Base, std::size_t... I> constexpr auto MakeHandlers(std::index_sequence<I...>) {
    return std::array<Handler, sizeof...(I)>{&StubHandler<Base + static_cast<unsigned>(I)>...};
}
constexpr auto kFxStubs = MakeHandlers<100>(std::make_index_sequence<19>{});
constexpr auto kMagicStubs = MakeHandlers<200>(std::make_index_sequence<110>{});
constexpr auto kPopupStubs = MakeHandlers<320>(std::make_index_sequence<5>{});
constexpr auto kPoseStubs = MakeHandlers<330>(std::make_index_sequence<2>{});
constexpr auto kWatchStubs = MakeHandlers<340>(std::make_index_sequence<5>{});
constexpr auto kFollowStubs = MakeHandlers<350>(std::make_index_sequence<1>{});
constexpr auto kRowStubs = MakeRows(std::make_index_sequence<at::kMagicRowCount>{});

Callees Stubs() {
    Callees s{};
    std::memcpy(s.fx, kFxStubs.data(), sizeof s.fx);
    std::memcpy(s.magic_fx, kMagicStubs.data(), sizeof s.magic_fx);
    std::memcpy(s.popup, kPopupStubs.data(), sizeof s.popup);
    std::memcpy(s.pose, kPoseStubs.data(), sizeof s.pose);
    std::memcpy(s.watch, kWatchStubs.data(), sizeof s.watch);
    std::memcpy(s.follow, kFollowStubs.data(), sizeof s.follow);
    s.rolling_digits = StubDigits;
    s.draw_number = StubNumber;
    s.draw_label = StubLabel;
    s.free_current = StubFree;
    s.actor_is_out = StubActorIsOut;
    s.set_animation = StubSetAnimation;
    s.queue_overlay = StubQueueOverlay;
    s.script_tick = StubScriptTick;
    s.update_screen = StubUpdateScreen;
    s.script_flags_set40 = StubScriptFlags;
    s.after_area_script = StubAfterArea;
    s.transition_start = StubTransition;
    return s;
}

// The stack-built tables: the offset of each imm32 in the copy (capstone,
// 2026-09-25) and the handler it names.
struct Imm { std::uint32_t offset, value; };
constexpr Imm kFxImm[19] = {{0xF, 0x437CC0}, {0x17, 0x432B70}, {0x22, 0x432F90}, {0x2A, 0x433190}, {0x32, 0x4332B0},
                            {0x3A, 0x433380}, {0x42, 0x433460}, {0x4A, 0x4337F0}, {0x52, 0x43C740}, {0x5A, 0x4348E0},
                            {0x62, 0x434B90}, {0x6A, 0x433970}, {0x72, 0x433B80}, {0x7A, 0x434D70}, {0x82, 0x434F40},
                            {0x8A, 0x452680}, {0x92, 0x452AD0}, {0x9A, 0x434310}, {0xA2, 0x452B60}};
constexpr Imm kMagicImm[110] = {
    {0xA, 0x437CC0}, {0x12, 0x49AB60}, {0x1A, 0x4FB260}, {0x22, 0x4D6E30}, {0x2A, 0x4AB570}, {0x32, 0x4C0620},
    {0x3A, 0x4A8360}, {0x42, 0x4D80C0}, {0x4A, 0x4F1500}, {0x52, 0x4A3C80}, {0x5A, 0x4CCAA0}, {0x62, 0x4D0730},
    {0x6A, 0x4CBAE0}, {0x72, 0x4BDC40}, {0x7A, 0x4CAC40}, {0x82, 0x4DAF00}, {0x8A, 0x4D5780}, {0x92, 0x4B5B10},
    {0x9A, 0x4B16C0}, {0xA2, 0x4B6A40}, {0xAA, 0x4C9E30}, {0xB2, 0x4C75B0}, {0xBA, 0x4C5110}, {0xC2, 0x4C8970},
    {0xCA, 0x4F3640}, {0xD2, 0x4D2E30}, {0xDA, 0x4D3F30}, {0xE2, 0x4CDA00}, {0xEA, 0x4B7A10}, {0xF2, 0x4C2A50},
    {0xFA, 0x4AAD50}, {0x102, 0x4A2460}, {0x10D, 0x4C4930}, {0x118, 0x4A1400}, {0x123, 0x4C1730}, {0x12E, 0x4A00C0},
    {0x139, 0x4BBF40}, {0x144, 0x4BCDF0}, {0x14F, 0x4BB1A0}, {0x15A, 0x4C3700}, {0x165, 0x4C58A0}, {0x170, 0x49EBC0},
    {0x17B, 0x49DA80}, {0x186, 0x4BEC00}, {0x191, 0x4ACCE0}, {0x19C, 0x4CF770}, {0x1A7, 0x4A3510}, {0x1B2, 0x4DA3D0},
    {0x1BD, 0x4D9630}, {0x1C8, 0x4A99A0}, {0x1D3, 0x4AA1A0}, {0x1DE, 0x4D18C0}, {0x1E9, 0x4AD1C0}, {0x1F4, 0x4E8700},
    {0x1FF, 0x4E9270}, {0x20A, 0x49E240}, {0x215, 0x4DD9F0}, {0x220, 0x4B7F40}, {0x22B, 0x4B4590}, {0x236, 0x4A4DA0},
    {0x241, 0x4A5C10}, {0x24C, 0x4A66E0}, {0x257, 0x4A6EC0}, {0x262, 0x4AF1F0}, {0x26D, 0x4AFF00}, {0x278, 0x4AEA40},
    {0x283, 0x4BFAE0}, {0x28E, 0x4FAFF0}, {0x299, 0x4EB640}, {0x2A4, 0x4B5810}, {0x2AF, 0x49C170}, {0x2BA, 0x4EF7D0},
    {0x2C5, 0x4FB0A0}, {0x2D0, 0x499170}, {0x2DB, 0x4B2120}, {0x2E6, 0x4E7630}, {0x2F1, 0x4C8E70}, {0x2FC, 0x4C6560},
    {0x307, 0x4E1AE0}, {0x312, 0x499780}, {0x31D, 0x4ABE00}, {0x330, 0x4F1F70}, {0x33B, 0x4ED310}, {0x349, 0x49C4C0},
    {0x354, 0x4EE460}, {0x35F, 0x4ADAF0}, {0x36A, 0x4E81A0}, {0x375, 0x4D6300}, {0x380, 0x4EDEC0}, {0x38B, 0x4A46F0},
    {0x396, 0x4DC260}, {0x3A1, 0x4DF820}, {0x3AC, 0x4E0A60}, {0x3B7, 0x43FE90}, {0x3C2, 0x4AC0E0}, {0x3CD, 0x4F9FB0},
    {0x3D8, 0x4F7380}, {0x3E3, 0x4F6440}, {0x3EE, 0x4BD2B0}, {0x3F9, 0x4F8860}, {0x404, 0x4E6BA0}, {0x40F, 0x4F93A0},
    {0x41A, 0x4E4540}, {0x425, 0x4F5400}, {0x430, 0x4B30F0}, {0x43B, 0x4E5220}, {0x446, 0x4E33B0}, {0x451, 0x4F4C40},
    {0x45C, 0x4E9B70}, {0x467, 0x4EA0F0}};
constexpr Imm kPopupImm[5] = {{0xF, 0x432C40}, {0x17, 0x432DB0}, {0x22, 0x432DE0}, {0x2A, 0x432E50}, {0x32, 0x432EA0}};
constexpr Imm kPoseImm[2] = {{0x19, 0x4331D0}, {0x24, 0x433290}};
constexpr Imm kWatchImm[5] = {{0xC, 0x4334C0}, {0x16, 0x433550}, {0x1E, 0x433640}, {0x26, 0x433650}, {0x2E, 0x433790}};
constexpr Imm kFollowImm[1] = {{0xD, 0x433810}};

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    const Imm* imms;
    unsigned n_imms;
    const Handler* imm_to;   // the stand-ins the immediates are re-aimed at
    move_script::Table table;
};

constexpr Call kPopupCalls[] = {{0x65, 0x432F10}, {0x88, 0x444480}, {0xAC, 0x4445A0}};
constexpr Call kEndCalls[] = {{0x66, 0x4351F0}};
constexpr Call kPoseStartCalls[] = {{0xAA, 0x5891F0}, {0xB2, 0x5890E0}};
constexpr Call kPosePlayCalls[] = {{0x0, 0x5893A0}, {0x9, 0x4351F0}, {0xE, 0x5890E0}};
constexpr Call kWatchTestCalls[] = {{0x24, 0x4456C0}, {0x57, 0x4456C0}};
constexpr Call kFollowOwnerCalls[] = {{0x137, 0x588F20}, {0x146, 0x4351F0}, {0x154, 0x4351F0}};
constexpr Call kHookScriptCalls[] = {{0x23, 0x57C7C0}, {0x28, 0x446E20}};
constexpr Call kHookTransitionCalls[] = {{0x16, 0x495040}};

enum : unsigned {
    kDispatch, kMagicDispatch, kMagicRow, kPopup, kPopupStart, kPopupHold, kPopupRise, kPopupBounce, kPopupEnd,
    kPoseTask, kPoseStart, kPosePlay, kStepReset, kWatch, kWatchTest, kFollow, kFollowOwner, kHookScript,
    kHookTransition, kCount
};

#define CE_N(a) static_cast<int>(sizeof a / sizeof a[0])
const Clone kClones[kCount] = {
    {"BattleFx_Dispatch", 0x4352A0, 0xAE, nullptr, 0, kFxImm, 19, kFxStubs.data(), {}},
    {"BattleMagicFx_Dispatch", 0x435350, 0x476, nullptr, 0, kMagicImm, 110, kMagicStubs.data(), {}},
    {"BattleMagicRow_Run", 0x4378B0, 0x1D, nullptr, 0, nullptr, 0, nullptr, {}},
    {"BattleFx_DamagePopup", 0x432B70, 0xC8, kPopupCalls, CE_N(kPopupCalls), kPopupImm, 5, kPopupStubs.data(), {0x57, 0xB8, 4}},
    {"BattleFx_PopupStart", 0x432C40, 0x16C, nullptr, 0, nullptr, 0, nullptr, {}},
    {"BattleFx_PopupHold", 0x432DB0, 0x2B, nullptr, 0, nullptr, 0, nullptr, {}},
    {"BattleFx_PopupRise", 0x432DE0, 0x64, nullptr, 0, nullptr, 0, nullptr, {}},
    {"BattleFx_PopupBounce", 0x432E50, 0x4C, nullptr, 0, nullptr, 0, nullptr, {}},
    {"BattleFx_PopupEnd", 0x432EA0, 0x6C, kEndCalls, CE_N(kEndCalls), nullptr, 0, nullptr, {}},
    {"BattleFx_PoseTask", 0x433190, 0x3A, nullptr, 0, kPoseImm, 2, kPoseStubs.data(), {}},
    {"BattleFx_PoseStart", 0x4331D0, 0xB7, kPoseStartCalls, CE_N(kPoseStartCalls), nullptr, 0, nullptr, {}},
    {"BattleFx_PosePlay", 0x433290, 0x13, kPosePlayCalls, CE_N(kPosePlayCalls), nullptr, 0, nullptr, {}},
    {"BattleFx_StepReset", 0x4332E0, 0x12, nullptr, 0, nullptr, 0, nullptr, {}},
    {"BattleFx_ActorWatch", 0x433460, 0x5B, nullptr, 0, kWatchImm, 5, kWatchStubs.data(), {}},
    {"BattleFx_ActorWatchTest", 0x4334C0, 0x8A, kWatchTestCalls, CE_N(kWatchTestCalls), nullptr, 0, nullptr, {}},
    {"BattleFx_Follow", 0x4337F0, 0x1A, nullptr, 0, kFollowImm, 1, kFollowStubs.data(), {}},
    {"BattleFx_FollowOwner", 0x433810, 0x15A, kFollowOwnerCalls, CE_N(kFollowOwnerCalls), nullptr, 0, nullptr, {}},
    {"BattleHook_Area189Script", 0x437720, 0x2E, kHookScriptCalls, CE_N(kHookScriptCalls), nullptr, 0, nullptr, {}},
    {"BattleHook_Area189Transition", 0x437750, 0x2B, kHookTransitionCalls, CE_N(kHookTransitionCalls), nullptr, 0, nullptr, {}},
};
#undef CE_N

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kTasks, 0x22A0},      // the 48 slots, 0x93B8C4, 0x93B940, the eight enemy objects
    {0x937F88, 4},             // Sprite_Current
    {at::kPhase, 0xA0},        // the battle's globals to 0x904B40
    {at::kMembers, 0x3E4},     // ObjTrio
    {at::kAnimSet, 4},
    {at::kLastArea, 2},
    {at::kScriptVar7, 2},
};
constexpr unsigned kRegionBytes = 0x22A0 + 4 + 0xA0 + 0x3E4 + 4 + 2 + 2;

struct State {
    unsigned char memory[kRegionBytes];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x1B873593u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

// Random bytes put back inside what the tables and buffers hold: the three
// pointers, every object's owner and actor, the owners' pose words, and the
// indices the stack tables are read by.
void Fix() {
    unsigned char* const cur = Slot(Next() % at::kTaskCount);
    SetPtr(at::kTaskCurrent, cur);
    Sprite_Current = Half() ? cur : Slot(Next() % at::kTaskCount);   // BattleTask_RunAll sets both; a mix-up shows
    SetPtr(at::kTaskOwner, AnyOwner(Next()));
    for (unsigned i = 0; i < at::kTaskCount; ++i) {
        unsigned char* const t = Slot(i);
        SetPtr(Address(t + 0x80), AnyOwner(Next()));
        t[5] = static_cast<unsigned char>(Next() % 11);
        SetWord(t + 0x2C, Next() % 8);
        t[1] = static_cast<unsigned char>(Next() % 5);
    }
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const e = EnemyObj(i);
        e[5] = static_cast<unsigned char>(Next() % 11);
        SetWord(e + 0x2C, Next() % 8);
    }
}

struct Args { std::uint32_t a[4]; };

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    unsigned char* const t = CurSlot();
    unsigned char* const sc = Sprite_Current;
    unsigned char* const o = Owner();
    switch (k) {
    case kDispatch: t[5] = static_cast<unsigned char>(Next() % 19); break;
    case kMagicDispatch: t[5] = static_cast<unsigned char>(Next() % 110); break;
    case kMagicRow:
        sc[5] = static_cast<unsigned char>(Often() ? Next() % at::kMagicRowCount : Next() % 2 ? 0 : 150);
        if (Half()) At(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(At(at::kRoundFlags + 1)[0] ^ 8);
        break;
    case kPopup:
        t[1] = static_cast<unsigned char>(Often() ? 2 + Next() % 2 : Next() % 5);
        if (Half()) sc[0] = static_cast<unsigned char>(sc[0] | 1);
        t[7] = static_cast<unsigned char>(Half() ? 0 : Often() ? Next() % 4 : Next() % 2 ? 4 : Next());
        break;
    case kPopupStart: {
        unsigned char* const owner = OwnerOf(t);
        owner[5] = static_cast<unsigned char>(Half() ? 2 + Next() % 2 : Next() % 11);
        if (Half()) t[0xB] = 1;
        static const std::int32_t kValues[] = {-1, 0, 1, 9, 10, 11, 99, 100, 101, 999, 9999, -100, -10,
                                               static_cast<std::int32_t>(0x80000000u), 0x7FFFFFFF};
        if (Often()) SetLong(t + 0x60, kValues[Next() % (sizeof kValues / sizeof kValues[0])]);
        break;
    }
    case kPopupHold:
    case kPopupRise:
    case kPopupEnd: {
        static const unsigned char kTimers[] = {0, 0, 1, 0xFF, 2};
        if (Often()) t[9] = kTimers[Next() % 5];
        break;
    }
    case kPopupBounce: {
        const int y = static_cast<short>(Word(t + 0x3A)), x = static_cast<short>(Word(t + 0x36));
        if (Often()) SetLong(t + 0x1C, y + static_cast<int>(Next() % 3) - 1);
        if (Often()) SetLong(t + 0x18, x + static_cast<int>(Next() % 2));
        break;
    }
    case kPoseTask: sc[1] = static_cast<unsigned char>(Next() % 2); break;
    case kPoseStart:
        sc[7] = static_cast<unsigned char>(Half() ? 6 : Next() % 12);
        sc[8] = static_cast<unsigned char>(Often() ? Next() % 4 : Next());
        sc[0x2D] = static_cast<unsigned char>(Next());   // the word +0x2C is cleared whole
        break;
    case kWatch:
        sc[1] = static_cast<unsigned char>(Next() % 5);
        At(at::kPhase)[0] = static_cast<unsigned char>(Half() ? 5 : Next() % 7);
        break;
    case kWatchTest: {
        if (Half()) At(at::kRoundFlags + 1)[0] = static_cast<unsigned char>(At(at::kRoundFlags + 1)[0] ^ 4);
        o[5] = static_cast<unsigned char>(Half() ? 2 + Next() % 2 : Next() % 11);
        if (Next() % 4 == 0) At(at::kTurnGate)[0] = o[5];
        // the status bytes the test reads, at each bit of 0x58 alone, at its
        // neighbours and at none; the test is only reached past the actor test
        static const unsigned char kStatus[] = {0, 0x40, 0x10, 0x08, 0x20, 0x80, 0x04, 0x58};
        for (unsigned i = 0; i < 3; ++i) {
            At(at::kMembers + 0x90 + i * at::kMemberSize)[0] = kStatus[Next() % 8];
            At(at::kMembers + 0x91 + i * at::kMemberSize)[0] = kStatus[Next() % 8];
        }
        for (unsigned i = 0; i < 8; ++i) {
            At(at::kEnemies + 0x92 + i * at::kEnemySize)[0] = kStatus[Next() % 8];
            At(at::kEnemies + 0x93 + i * at::kEnemySize)[0] = kStatus[Next() % 8];
        }
        break;
    }
    case kFollow: sc[1] = 0; break;
    case kFollowOwner:
        if (Half()) o[0] = static_cast<unsigned char>(o[0] ^ 1);
        if (Half()) At(at::kPhase)[0] = 1;
        break;
    case kHookScript:
    case kHookTransition:
        if (Often()) SetWord(At(at::kLastArea), Half() ? 0xBD : 0x1BD);
        break;
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[600];
    unsigned popup_digits, popup_number, popup_label, popup_none;
    unsigned start_member, start_enemy, digits[3];
    unsigned bounce_end, bounce_move, timer_zero, watch_stepped, follow_freed2, hooks_fired;
} g_cover;
unsigned Byte(const State& s, std::uint32_t address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + (address - r.at)];
        at += r.size;
    }
    return 0x100;
}
void Cover(unsigned k, const State& in, const State& out) {
    unsigned counts[16] = {};
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i) {
        if (out.log[i].what < 600) ++g_cover.logged[out.log[i].what];
        if (out.log[i].what < 16) ++counts[out.log[i].what];
    }
    const std::uint32_t cur = static_cast<std::uint32_t>(Byte(in, at::kTaskCurrent)) | Byte(in, at::kTaskCurrent + 1) << 8 |
                              Byte(in, at::kTaskCurrent + 2) << 16 | Byte(in, at::kTaskCurrent + 3) << 24;
    switch (k) {
    case kPopup:
        if (counts[1]) ++g_cover.popup_digits;
        else if (counts[2]) ++g_cover.popup_number;
        else if (counts[3]) ++g_cover.popup_label;
        else ++g_cover.popup_none;
        break;
    case kPopupStart: {
        const unsigned d = Byte(out, cur + 0xA);
        if (d < 3) ++g_cover.digits[d];
        break;
    }
    case kPopupBounce:
        if (Byte(out, cur + 9) == 0xA && Byte(in, cur + 1) != Byte(out, cur + 1)) ++g_cover.bounce_end;
        else ++g_cover.bounce_move;
        break;
    case kPopupHold:
    case kPopupRise:
    case kPopupEnd:
        if (Byte(in, cur + 9) == 0) ++g_cover.timer_zero;
        break;
    case kWatchTest: {
        const std::uint32_t sc = static_cast<std::uint32_t>(Byte(in, 0x937F88)) | Byte(in, 0x937F89) << 8 |
                                 Byte(in, 0x937F8A) << 16 | Byte(in, 0x937F8B) << 24;
        if (Byte(in, sc + 1) != Byte(out, sc + 1)) ++g_cover.watch_stepped;
        break;
    }
    case kFollowOwner:
        if (counts[4] == 2) ++g_cover.follow_freed2;
        break;
    case kHookScript:
    case kHookTransition:
        if (out.log_n) ++g_cover.hooks_fired;
        break;
    default:
        break;
    }
}

using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

void PatchImms(void* copy, const char* name, const Imm* imms, unsigned n, const Handler* to) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (unsigned i = 0; i < n; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + imms[i].offset, sizeof had);
        if (had != imms[i].value)
            bof3::Fatal("battle_fx_tasks: %s +0x%X holds 0x%X, not the handler 0x%X", name, static_cast<unsigned>(imms[i].offset),
                        static_cast<unsigned>(had), static_cast<unsigned>(imms[i].value));
        const std::uint32_t target = Address(reinterpret_cast<const void*>(to[i]));
        std::memcpy(code + imms[i].offset, &target, sizeof target);
    }
}

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 1000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_fx_tasks: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    // our tables must name what the originals' immediates name
    for (unsigned i = 0; i < 19; ++i)
        if (Address(reinterpret_cast<const void*>(kOriginals.fx[i])) != kFxImm[i].value)
            bof3::Fatal("battle_fx_tasks: kOriginals.fx[%u] is not 0x%X", i, static_cast<unsigned>(kFxImm[i].value));
    for (unsigned i = 0; i < 110; ++i)
        if (Address(reinterpret_cast<const void*>(kOriginals.magic_fx[i])) != kMagicImm[i].value)
            bof3::Fatal("battle_fx_tasks: kOriginals.magic_fx[%u] is not 0x%X", i, static_cast<unsigned>(kMagicImm[i].value));
    const struct { const Handler* ours; const Imm* imms; unsigned n; } kSmall[] = {
        {kOriginals.popup, kPopupImm, 5}, {kOriginals.pose, kPoseImm, 2}, {kOriginals.watch, kWatchImm, 5}, {kOriginals.follow, kFollowImm, 1}};
    for (const auto& s : kSmall)
        for (unsigned i = 0; i < s.n; ++i)
            if (Address(reinterpret_cast<const void*>(s.ours[i])) != s.imms[i].value)
                bof3::Fatal("battle_fx_tasks: a state table entry is not 0x%X", static_cast<unsigned>(s.imms[i].value));

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[4];
        if (c.n_calls > 4) bof3::Fatal("battle_fx_tasks: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.table.entries) move_script::Relocate(clones[k], c.base, c.size, c.table);
        if (c.n_imms) PatchImms(clones[k], c.name, c.imms, c.n_imms, c.imm_to);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&BattleFx_Dispatch), reinterpret_cast<const void*>(&BattleMagicFx_Dispatch),
        reinterpret_cast<const void*>(&BattleMagicRow_Run), reinterpret_cast<const void*>(&BattleFx_DamagePopup),
        reinterpret_cast<const void*>(&BattleFx_PopupStart), reinterpret_cast<const void*>(&BattleFx_PopupHold),
        reinterpret_cast<const void*>(&BattleFx_PopupRise), reinterpret_cast<const void*>(&BattleFx_PopupBounce),
        reinterpret_cast<const void*>(&BattleFx_PopupEnd), reinterpret_cast<const void*>(&BattleFx_PoseTask),
        reinterpret_cast<const void*>(&BattleFx_PoseStart), reinterpret_cast<const void*>(&BattleFx_PosePlay),
        reinterpret_cast<const void*>(&BattleFx_StepReset), reinterpret_cast<const void*>(&BattleFx_ActorWatch),
        reinterpret_cast<const void*>(&BattleFx_ActorWatchTest), reinterpret_cast<const void*>(&BattleFx_Follow),
        reinterpret_cast<const void*>(&BattleFx_FollowOwner), reinterpret_cast<const void*>(&BattleHook_Area189Script),
        reinterpret_cast<const void*>(&BattleHook_Area189Transition)};

    static State saved, input, their_out, our_out;
    std::uint32_t saved_rows[at::kMagicRowCount];
    for (unsigned i = 0; i < at::kMagicRowCount; ++i) saved_rows[i] = static_cast<std::uint32_t>(Long(At(at::kMagicRows + i * 8 + 4)));
    Capture(saved);
    g = Stubs();
    for (unsigned i = 0; i < at::kMagicRowCount; ++i)
        SetPtr(at::kMagicRows + i * 8 + 4, reinterpret_cast<const void*>(kRowStubs[i]));

    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
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
            const void* const fn = pass ? ours[k] : clones[k];
            reinterpret_cast<Fn4>(const_cast<void*>(fn))(0, 0, 0, 0);
            Capture(out);
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      battle_fx_tasks self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    for (unsigned i = 0; i < at::kMagicRowCount; ++i) SetLong(At(at::kMagicRows + i * 8 + 4), static_cast<std::int32_t>(saved_rows[i]));
    Apply(saved);

    bof3::Log("shadow      battle_fx_tasks self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the task slots, the enemy objects, the battle's globals, the party records, the animation "
              "set pointer, the area word, the script bytes and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerFunction, calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      battle_fx_tasks: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned fx = 0, magic = 0, rows = 0;
    for (unsigned i = 100; i < 119; ++i) fx += c.logged[i] ? 1u : 0u;
    for (unsigned i = 200; i < 310; ++i) magic += c.logged[i] ? 1u : 0u;
    for (unsigned i = 400; i < 551; ++i) rows += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      battle_fx_tasks coverage: kind-0 effects %u of 19, magic effects %u of 110, magic rows %u of 151; "
              "popup states %u %u %u %u %u; faces: digits %u, number %u, label %u, none %u; digit counts 0 %u, 1 %u, 2 %u; "
              "bounce ended %u, moved %u; timers at 0 %u; watch stepped %u; pose states %u %u, watch states %u %u %u %u %u, follow %u; "
              "actor tests %u, frees %u (twice in one follow %u), screen updates %u, animations %u, overlays %u, ticks %u; "
              "hooks fired %u, script flags %u, transitions %u",
              fx, magic, rows, c.logged[320], c.logged[321], c.logged[322], c.logged[323], c.logged[324], c.popup_digits,
              c.popup_number, c.popup_label, c.popup_none, c.digits[0], c.digits[1], c.digits[2], c.bounce_end, c.bounce_move,
              c.timer_zero, c.watch_stepped, c.logged[330], c.logged[331], c.logged[340], c.logged[341], c.logged[342], c.logged[343],
              c.logged[344], c.logged[350], c.logged[5], c.logged[4], c.follow_freed2, c.logged[9], c.logged[6], c.logged[7],
              c.logged[8], c.hooks_fired, c.logged[10], c.logged[12]);
    if (bad) bof3::Fatal("the battle effect tasks differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_fx_tasks
