// BOF3X_SHADOW=msgbox: a differential fuzz of the forty-three functions of
// msgbox.cpp against byte-copies of Capcom's, once at start-up
// (docs/msgbox.md section 5).
//
// Every call out of a copy is re-aimed at a recording stand-in (the offsets
// below are capstone's, 2026-09-22); the six dispatchers build their tables
// on the stack out of `mov dword [esp+k], imm32`, so those immediates are
// rewritten in the copy the same way; and the two inline jump tables -
// MsgBox_Step's 23 codes at 0x497A70 and MsgBox_StatePrint's at 0x497E34 -
// are relocated into the copy, because their entries are absolute.
//
// One round: one function, random bytes in the MsgBoxState block, the two
// window records and the three text pens, a freshly generated message, then
// that function's own boundaries seeded; theirs, then from the same state
// ours; the regions, the primitive, the packet cursor, the return value and
// the stand-ins' log compared.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/msgbox_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace msgbox {
namespace {

// --- the log the stand-ins write -----------------------------------------
constexpr unsigned kLog = 64;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;
unsigned char g_prim[0x80];
unsigned char* g_msg;             // 0x800 bytes: the message every walk runs in
constexpr unsigned kMsgBytes = 0x800;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0,
            std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    const auto at = reinterpret_cast<std::uintptr_t>(p);
    const auto msg = reinterpret_cast<std::uintptr_t>(g_msg);
    const auto prim = reinterpret_cast<std::uintptr_t>(g_prim);
    if (at >= msg && at < msg + kMsgBytes) return 0x10000 + static_cast<std::uint32_t>(at - msg);
    if (at >= prim && at < prim + sizeof g_prim) return 0x20000 + static_cast<std::uint32_t>(at - prim);
    return static_cast<std::uint32_t>(at);
}

// --- what a callee may change that its caller reads again after it -------
// Every byte here is one some function in this file reads, or stores over,
// after a call. The two pointers are kept inside the message buffer and the
// step count small, so that a disturbed walk still terminates; the flag
// word's bit 4 (instant print) is always cleared, because setting it makes
// MsgBox_StatePrint's loop run until the message's next 0x10 - a start-up
// hang rather than a mismatch (docs/msgbox.md section 5).
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return;
    const std::uint32_t v = h >> 8;
    switch ((h >> 4) % 26) {
    case 0: SetB(kState, v % 8); break;
    case 1: SetB(kSubState, v % 8); break;
    case 2: SetB(kEffectKind, v % 6); break;
    case 3: SetB(kEffectPhase, v % 2); break;
    case 4: SetW(kFlags, v & 0xFFEFu); break;
    case 5: SetB(kDelay, v); break;
    case 6: SetB(kSubCount, v); break;
    case 7: SetW(kMessage, v % 64); break;
    case 8: SetW(kEffectTimer, v % 2 ? 0xFFFF : v); break;
    case 9: SetPtr(kAt, g_msg + v % 0x100); break;
    case 10: SetPtr(kResume, g_msg + v % 0x100); break;
    case 11: SetPtr(kBase, g_msg + v % 0x100); break;
    case 12: SetB(kColorHigh, v % 9); break;
    // Only ever RAISED: MsgBox_Step's loop ends when the drawn count EQUALS
    // this byte, so lowering it under the count already drawn would walk off
    // the end of the message - a start-up hang, not a mismatch.
    case 13: SetB(kStepCount, B(kStepCount) | (v % 8)); break;
    case 14: SetB(kOffsetX, v); break;
    case 15: SetB(kOffsetY, v); break;
    case 16: MsgBox_PenX = static_cast<short>(v); break;
    case 17: SetW(kPenY, v); break;
    case 18: MsgBox_LineX = static_cast<short>(v); break;
    case 19: SetW(kOriginY, v); break;
    case 20: SetB(kCursor, v); break;
    case 21: SetB(kChoiceLast, v); break;
    case 22: SetB(kWindow0State, v % 6); break;
    case 23: SetB(kWindow1State, v % 6); break;
    case 24: SetW(kEffectOff, v); break;
    default: SetW(kEffectWait, v); break;
    }
}

// --- the stand-ins --------------------------------------------------------
// One per distinct callee address. The void ones are a template, so the
// address each was stood in for is in the log without a table.
template <std::uint32_t A>
void __cdecl Slot() {
    Record(A);
    Disturb();
}

// Window_Alloc's stand-in claims the slot for real: the window record bytes
// are compared, so a caller that reads them back must see what it would have
// (the movement-script trap - a stand-in quieter than the callee).
unsigned __cdecl StubWindowAlloc(unsigned slot, unsigned kind) {
    Record(0x59E2D0, slot, kind);
    unsigned char* const rec = At(kWindows + (slot & 0xFF) * 0x24u);
    unsigned answer = slot | 0xFFu;
    if (rec[0] == 0) {
        rec[0] = 1;
        rec[1] = static_cast<unsigned char>(kind);
        rec[2] = 0;
        rec[3] = 0;
        answer = slot;
    }
    Disturb();
    return answer;
}

// Text_DrawAt as MsgBox_Step calls it. The copy pushes whole registers whose
// upper halves are stale - Text_DrawAt reads x as a word and y as a word, so
// only sixteen bits of each can be observed and only those are logged.
const unsigned char* __cdecl StubDrawChar(int x, int y, int color, int count, const unsigned char* text) {
    Record(0x516B30, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y),
           static_cast<std::uint32_t>(color), static_cast<std::uint32_t>(count) ^ (Id(text) << 8));
    Disturb();
    return text + 1;
}
// 0x4987E0 reads `mov al, [esp+4]` and masks to 0xF: only this byte of the
// colour local can reach it, and the copy's other three are stack it never
// wrote (docs/msgbox.md section 2).
void __cdecl StubEffectDraw(unsigned color, const unsigned char* text) {
    Record(0x4987E0, color & 0xFFu, Id(text));
    Disturb();
}
// 0x498D20 takes both arguments with `movsx ecx, word ptr` (0x498D7C).
void __cdecl StubPageArrow(int x, int y) {
    Record(0x498D20, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y));
    Disturb();
}
// The pad auto-repeat: only bits 8..15 of its answer are tested, and the two
// its callers test are 0x10 and 0x40 - seeded here so both branches run.
unsigned __cdecl StubRepeat(unsigned pad) {
    const std::uint32_t h = Hash();
    Record(0x461EB0, pad & 0xFFFFu);
    Disturb();
    static const std::uint32_t kAnswers[] = {0, 0x1000, 0x4000, 0x5000, 0x1080, 0xFFFF, 0x4001};
    return kAnswers[h % 7] | (h & 0xFFFF0000u);
}
void __cdecl StubSound(unsigned short id) {
    Record(0x587740, id);
    Disturb();
}
const unsigned char* __cdecl StubDrawString(unsigned color, unsigned count, const unsigned char* text) {
    Record(0x516B70, color, count, Id(text));
    Disturb();
    return text + 2;
}
void __cdecl StubSetCode(unsigned char* prim) { Record(0x5A7760, Id(prim)); }
void __cdecl StubSetSemi(unsigned char* prim, unsigned abe) { Record(0x5A7780, Id(prim), abe); }
// Moves the packet cursor, as Gfx_CommitPrim does, so a cursor read too late
// would show.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(0x461E50, slot, size, Id(Gfx_PacketNext));
    Gfx_PacketNext = g_prim + (static_cast<unsigned>(Gfx_PacketNext - g_prim) + (size & 0x1F)) % 0x40u;
}

template <typename T>
T As(const void* p) {
    return reinterpret_cast<T>(const_cast<void*>(p));
}

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    // The typed ones.
    case 0x59E2D0: return f(&StubWindowAlloc);
    case 0x516B30: return f(&StubDrawChar);
    case 0x4987E0: return f(&StubEffectDraw);
    case 0x498D20: return f(&StubPageArrow);
    case 0x461EB0: return f(&StubRepeat);
    case 0x587740: return f(&StubSound);
    case 0x516B70: return f(&StubDrawString);
    case 0x5A7760: return f(&StubSetCode);
    case 0x5A7780: return f(&StubSetSemi);
    case 0x461E50: return f(&StubCommit);
    // The void ones, one template instance each.
    case 0x437CC0: return f(&Slot<0x437CC0>);
    case 0x497770: return f(&Slot<0x497770>);
    case 0x497840: return f(&Slot<0x497840>);
    case 0x497AD0: return f(&Slot<0x497AD0>);
    case 0x497B30: return f(&Slot<0x497B30>);
    case 0x497E90: return f(&Slot<0x497E90>);
    case 0x497EB0: return f(&Slot<0x497EB0>);
    case 0x497EE0: return f(&Slot<0x497EE0>);
    case 0x497F20: return f(&Slot<0x497F20>);
    case 0x497F40: return f(&Slot<0x497F40>);
    case 0x497F70: return f(&Slot<0x497F70>);
    case 0x497FD0: return f(&Slot<0x497FD0>);
    case 0x498050: return f(&Slot<0x498050>);
    case 0x4980B0: return f(&Slot<0x4980B0>);
    case 0x4980F0: return f(&Slot<0x4980F0>);
    case 0x498110: return f(&Slot<0x498110>);
    case 0x4981A0: return f(&Slot<0x4981A0>);
    case 0x4981C0: return f(&Slot<0x4981C0>);
    case 0x498230: return f(&Slot<0x498230>);
    case 0x498250: return f(&Slot<0x498250>);
    case 0x498280: return f(&Slot<0x498280>);
    case 0x4982C0: return f(&Slot<0x4982C0>);
    case 0x498310: return f(&Slot<0x498310>);
    case 0x498340: return f(&Slot<0x498340>);
    case 0x498350: return f(&Slot<0x498350>);
    case 0x4983C0: return f(&Slot<0x4983C0>);
    case 0x498410: return f(&Slot<0x498410>);
    case 0x498450: return f(&Slot<0x498450>);
    case 0x498470: return f(&Slot<0x498470>);
    case 0x4984A0: return f(&Slot<0x4984A0>);
    case 0x4984E0: return f(&Slot<0x4984E0>);
    case 0x498520: return f(&Slot<0x498520>);
    case 0x498560: return f(&Slot<0x498560>);
    case 0x498580: return f(&Slot<0x498580>);
    case 0x4985A0: return f(&Slot<0x4985A0>);
    case 0x4985E0: return f(&Slot<0x4985E0>);
    case 0x498610: return f(&Slot<0x498610>);
    case 0x498670: return f(&Slot<0x498670>);
    case 0x498740: return f(&Slot<0x498740>);
    case 0x498770: return f(&Slot<0x498770>);
    case 0x4987A0: return f(&Slot<0x4987A0>);
    default: bof3::Fatal("msgbox: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the clones -----------------------------------------------------------
constexpr std::uint32_t kStateAddr[8] = {0x497B30, 0x497E90, 0x497EB0, 0x497F40,
                                         0x498050, 0x4982C0, 0x498450, 0x498470};
constexpr std::uint32_t kSub2Addr[2] = {0x497EE0, 0x497F20};
constexpr std::uint32_t kSub3Addr[2] = {0x497F70, 0x497FD0};
constexpr std::uint32_t kSub4Addr[8] = {0x497F70, 0x4980B0, 0x4980F0, 0x498110,
                                        0x4981A0, 0x4981C0, 0x498230, 0x498250};
constexpr std::uint32_t kSub5Addr[6] = {0x498310, 0x498340, 0x498350, 0x4983C0, 0x498410, 0x497FD0};
constexpr std::uint32_t kSub7Addr[2] = {0x4984A0, 0x497F20};
constexpr std::uint32_t kEffectAddr[6] = {0x437CC0, 0x498520, 0x4985A0, 0x4985A0, 0x498670, 0x498740};
constexpr std::uint32_t kShakeAddr[2] = {0x498560, 0x498580};
constexpr std::uint32_t kGrowAddr[2] = {0x4985E0, 0x498610};
constexpr std::uint32_t kRiseAddr[2] = {0x498770, 0x4987A0};

struct Site { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Site* calls; int n_calls;      // E8 / E9, re-aimed by CloneOriginal
    const Site* imms; int n_imms;        // mov dword [esp+k], imm32
    move_script::Table table;            // {0, 0, 0} when there is none
    std::uint32_t ret_mask;              // bits of eax the round compares
};

constexpr Site kOpenCalls[] = {{0x24, 0x497770}};
constexpr Site kResetCalls[] = {{0x6F, 0x59E2D0}};
constexpr Site kFrameCalls[] = {{0x28, 0x497AD0}, {0x2D, 0x4984E0}, {0x3B, 0x497840}};
constexpr Site kStepCalls[] = {{0x194, 0x4987E0}, {0x1E2, 0x516B30}};
constexpr Site kPrintCalls[] = {{0x7C, 0x587740}};
constexpr Site kArrowCalls[] = {{0x4E, 0x498D20}};
constexpr Site kScrollCalls[] = {{0x71, 0x498280}};
constexpr Site kChoiceOpenCalls[] = {{0xB, 0x59E2D0}};
constexpr Site kChoiceInputCalls[] = {{0x3C, 0x461EB0}};
constexpr Site kChoiceReopenCalls[] = {{0x10, 0x498280}};
constexpr Site kMenuOpenCalls[] = {{0xB, 0x59E2D0}};
constexpr Site kMenuInputCalls[] = {{0x1B, 0x461EB0}};
constexpr Site kDrawAtCalls[] = {{0x2C, 0x516B70}};
constexpr Site kEmitCalls[] = {{0xF9, 0x5A7760}, {0x106, 0x5A7780}, {0x10F, 0x461E50}};

constexpr Site kDispatchImms[] = {{0x0C, 0x497B30}, {0x19, 0x497E90}, {0x21, 0x497EB0}, {0x29, 0x497F40},
                                  {0x31, 0x498050}, {0x39, 0x4982C0}, {0x41, 0x498450}, {0x49, 0x498470}};
constexpr Site kState2Imms[] = {{0x09, 0x497EE0}, {0x16, 0x497F20}};
constexpr Site kState3Imms[] = {{0x09, 0x497F70}, {0x16, 0x497FD0}};
constexpr Site kState4Imms[] = {{0x09, 0x497F70}, {0x16, 0x4980B0}, {0x1E, 0x4980F0}, {0x26, 0x498110},
                                {0x2E, 0x4981A0}, {0x36, 0x4981C0}, {0x3E, 0x498230}, {0x46, 0x498250}};
constexpr Site kState5Imms[] = {{0x09, 0x498310}, {0x16, 0x498340}, {0x1E, 0x498350},
                                {0x26, 0x4983C0}, {0x2E, 0x498410}, {0x36, 0x497FD0}};
constexpr Site kState7Imms[] = {{0x09, 0x4984A0}, {0x16, 0x497F20}};
// 0x4984E0 loads 0x4985A0 into eax once (mov eax, imm32 at +3, its immediate
// at +4) and stores it into entries 2 and 3.
constexpr Site kEffectImms[] = {{0x04, 0x4985A0}, {0x0C, 0x437CC0}, {0x1E, 0x498520},
                                {0x2B, 0x498670}, {0x33, 0x498740}};
constexpr Site kShakeImms[] = {{0x09, 0x498560}, {0x16, 0x498580}};
constexpr Site kGrowImms[] = {{0x09, 0x4985E0}, {0x16, 0x498610}};
constexpr Site kRiseImms[] = {{0x09, 0x498770}, {0x16, 0x4987A0}};

enum : unsigned {
    kOpenScript, kReset, kFrameTask, kStep, kDispatch, kPrint, kDelayState, kState2, kState2Press,
    kState2Closed, kState3, kState3Arrow, kState3Step, kState4, kChoiceOpen, kChoiceWaitOpen,
    kChoiceInput, kChoiceWaitShut, kChoiceReopen, kChoiceDone, kReopen, kState5, kMenuOpen,
    kMenuWaitOpen, kMenuInput, kMenuDone, kState6, kState7, kState7Delay, kEffectTask, kShake,
    kShakeOut, kShakeBack, kGrow, kGrowStart, kGrowStep, kWander, kRise, kRiseStart, kRiseStep,
    kWindowAlloc, kDrawAt, kEmitGlyph, kCount
};

#define MB_N(a) (static_cast<int>(sizeof(a) / sizeof((a)[0])))
const Clone kClones[kCount] = {
    {"Msg_OpenScript", 0x4976D0, 0x32, kOpenCalls, MB_N(kOpenCalls), nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_Reset", 0x497770, 0x7F, kResetCalls, MB_N(kResetCalls), nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_FrameTask", 0x4977F0, 0x4C, kFrameCalls, MB_N(kFrameCalls), nullptr, 0, {0, 0, 0}, 0xFF},
    {"MsgBox_Step", 0x497840, 0x28C, kStepCalls, MB_N(kStepCalls), nullptr, 0, {0x75, 0x230, 23}, 0},
    {"MsgBox_StateDispatch", 0x497AD0, 0x55, nullptr, 0, kDispatchImms, MB_N(kDispatchImms), {0, 0, 0}, 0},
    {"MsgBox_StatePrint", 0x497B30, 0x360, kPrintCalls, MB_N(kPrintCalls), nullptr, 0, {0x68, 0x304, 23}, 0},
    {"MsgBox_StateDelay", 0x497E90, 0x16, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_State2", 0x497EB0, 0x22, nullptr, 0, kState2Imms, MB_N(kState2Imms), {0, 0, 0}, 0},
    {"MsgBox_State2Press", 0x497EE0, 0x33, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_State2Closed", 0x497F20, 0x11, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_State3", 0x497F40, 0x22, nullptr, 0, kState3Imms, MB_N(kState3Imms), {0, 0, 0}, 0},
    {"MsgBox_State3Arrow", 0x497F70, 0x57, kArrowCalls, MB_N(kArrowCalls), nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_State3Step", 0x497FD0, 0x76, kScrollCalls, MB_N(kScrollCalls), nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_State4", 0x498050, 0x52, nullptr, 0, kState4Imms, MB_N(kState4Imms), {0, 0, 0}, 0},
    {"MsgBox_ChoiceOpen", 0x4980B0, 0x35, kChoiceOpenCalls, MB_N(kChoiceOpenCalls), nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_ChoiceWaitOpen", 0x4980F0, 0x19, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_ChoiceInput", 0x498110, 0x82, kChoiceInputCalls, MB_N(kChoiceInputCalls), nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_ChoiceWaitShut", 0x4981A0, 0x19, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_ChoiceReopen", 0x498230, 0x1E, kChoiceReopenCalls, MB_N(kChoiceReopenCalls), nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_ChoiceDone", 0x498250, 0x27, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_Reopen", 0x498280, 0x40, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_State5", 0x4982C0, 0x42, nullptr, 0, kState5Imms, MB_N(kState5Imms), {0, 0, 0}, 0},
    {"MsgBox_MenuOpen", 0x498310, 0x27, kMenuOpenCalls, MB_N(kMenuOpenCalls), nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_MenuWaitOpen", 0x498340, 0x10, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_MenuInput", 0x498350, 0x61, kMenuInputCalls, MB_N(kMenuInputCalls), nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_MenuDone", 0x498410, 0x31, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_State6", 0x498450, 0x11, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_State7", 0x498470, 0x22, nullptr, 0, kState7Imms, MB_N(kState7Imms), {0, 0, 0}, 0},
    {"MsgBox_State7Delay", 0x4984A0, 0x34, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_EffectTask", 0x4984E0, 0x3F, nullptr, 0, kEffectImms, MB_N(kEffectImms), {0, 0, 0}, 0},
    {"MsgBox_EffectShake", 0x498520, 0x40, nullptr, 0, kShakeImms, MB_N(kShakeImms), {0, 0, 0}, 0},
    {"MsgBox_ShakeOut", 0x498560, 0x1A, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_ShakeBack", 0x498580, 0x1A, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_EffectGrow", 0x4985A0, 0x34, nullptr, 0, kGrowImms, MB_N(kGrowImms), {0, 0, 0}, 0},
    {"MsgBox_GrowStart", 0x4985E0, 0x27, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_GrowStep", 0x498610, 0x55, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_EffectWander", 0x498670, 0xCA, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_EffectRise", 0x498740, 0x22, nullptr, 0, kRiseImms, MB_N(kRiseImms), {0, 0, 0}, 0},
    {"MsgBox_RiseStart", 0x498770, 0x21, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"MsgBox_RiseStep", 0x4987A0, 0x38, nullptr, 0, nullptr, 0, {0, 0, 0}, 0},
    {"Window_Alloc", 0x59E2D0, 0x40, nullptr, 0, nullptr, 0, {0, 0, 0}, 0xFFFFFFFFu},
    {"Text_DrawAt", 0x516B30, 0x35, kDrawAtCalls, MB_N(kDrawAtCalls), nullptr, 0, {0, 0, 0}, 0xFFFFFFFFu},
    {"Text_EmitGlyph", 0x516D50, 0x11B, kEmitCalls, MB_N(kEmitCalls), nullptr, 0, {0, 0, 0}, 0},
};
#undef MB_N

// --- the state a round compares ------------------------------------------
struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {kState, 0x30},          // the whole MsgBoxState block
    {kWindows, 0x48},        // window records 0 and 1
    {0x8034F2, 4},           // Text_PenY, Text_PenX
    {0x7E1BE4, 2},           // Text_LineX
    {0x7E1BEC, 2},           // Input_Pressed (the stand-ins disturb it through nothing else)
};
constexpr unsigned kRegionBytes = 0x30 + 0x48 + 4 + 2 + 2;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char prim[sizeof g_prim];
    std::uint32_t packet, ret;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s, std::uint32_t ret) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.prim, g_prim, sizeof g_prim);
    s.packet = Id(Gfx_PacketNext);
    s.ret = ret;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_prim, s.prim, sizeof g_prim);
    Gfx_PacketNext = g_prim + (s.packet - 0x20000);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x9E3779B9u;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool Often() { return Next() % 3 != 0; }

// The message every walk runs in. Mostly glyphs - so that MsgBox_Step reaches
// its character count and MsgBox_StatePrint reaches a glyph, which is what
// ends its loop - with every control code sprinkled through the first quarter
// and every argument byte kept at 7 or below, so that a substitution's
// pointer stays inside memory the image maps. Codes 0x10 and 0x11 are written
// in pairs: an odd one would leave instant print on and the loop would not
// end (docs/msgbox.md section 2).
void BuildMessage() {
    for (unsigned i = 0; i < 0x200; ++i) g_msg[i] = static_cast<unsigned char>(0x30 + Next() % 0x40);
    unsigned at = 0;
    while (at < 0x100) {
        at += 1 + Next() % 4;
        if (at >= 0x100) break;
        static const unsigned char kCodes[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                               0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x12, 0x13,
                                               0x14, 0x15, 0x16, 0x2A, 0x3C};
        const unsigned char c = kCodes[Next() % (sizeof kCodes)];
        g_msg[at++] = c;
        if (c == 0x14) {
            g_msg[at++] = static_cast<unsigned char>(Next() % 8);
            g_msg[at++] = static_cast<unsigned char>(Next() % 8);
            g_msg[at++] = static_cast<unsigned char>(Next() & 0xFF);   // rows | column << 4
        } else if (c == 0x04 || c == 0x05 || c == 0x07 || c == 0x08 || c == 0x0A || c == 0x0C ||
                   c == 0x0F || c == 0x16) {
            g_msg[at++] = static_cast<unsigned char>(Next() % 8);
        }
        if (Next() % 6 == 0 && at + 6 < 0x100) {   // an instant-print span, opened and closed
            g_msg[at++] = 0x10;
            at += 1 + Next() % 3;
            g_msg[at++] = static_cast<unsigned char>(Next() % 2 ? 0x10 : 0x11);
        }
        if (Next() % 5 == 0 && at + 2 < 0x100) {   // a two-byte glyph
            g_msg[at++] = static_cast<unsigned char>(0x80 | Next() % 8);
            g_msg[at++] = static_cast<unsigned char>(Next() & 0xFF);
        }
    }
    // 0x200..0x800 is a backstop of plain glyphs, filled once: every walk
    // that runs past the generated part ends there.
}

// The pool the two 0x08 substitutions and the two openers read: sixty-four
// u16 offsets at the front, all landing inside the block, and zeros after
// them - so a substitution that runs on reads code 0x00 and returns.
void BuildPool() {
    std::memset(At(kScriptPool), 0, 0x280);
    for (unsigned i = 0; i < 64; ++i) SetW(kScriptPool + 2 * i, 0x80 + 4 * i);
}

struct Coverage {
    unsigned ran[kCount];
    unsigned printed, substituted, wrapped, drawn, scrolled, effect_ended, cursor_moved, window_taken;
} g_cover;

// Each function's own boundaries, on top of the random bytes. `args` are the
// seven dwords every one is called with (cdecl: the ones it does not take are
// ignored); every dispatch index is put in range, because out of range the
// original calls off its own stack frame and ours refuses.
void Seed(unsigned k, std::uint32_t (&args)[7]) {
    SetPtr(kBase, g_msg + Next() % 0x40);
    SetPtr(kAt, g_msg + Next() % 0x40);
    SetPtr(kResume, g_msg + Next() % 0x40);
    SetW(kFlags, W(kFlags) & 0xFFEFu);       // instant print off: see BuildMessage
    SetB(kStepCount, Next() % 9);
    SetB(kState, Next() % 8);
    SetB(kSubState, Next() % 8);
    SetB(kEffectKind, Next() % 6);
    SetB(kEffectPhase, Next() % 2);
    SetB(kNameIndex, Next() % 8);
    SetW(kMessage, Often() ? Next() % 64 : (Next() % 2 ? 0xFFFF : Next() & 0xFFFF));
    SetL(kPoolSelector, (Next() % 3) * 0x20);
    if (Often()) SetW(kEffectTimer, Next() % 3 == 0 ? 0xFFFF : Next() % 4);
    if (Often()) SetB(kDelay, Next() % 2 ? Next() % 3 : Next() & 0xFF);
    if (Often()) SetB(kSubCount, Next() % 2 ? 1 + Next() % 3 : Next() & 0xFF);
    if (Often()) SetB(kColorHigh, Next() % 2 ? Next() % 9 : Next() & 0xFF);
    if (Often()) SetB(kWindow0State, Next() % 6);
    if (Often()) SetB(kWindow1State, Next() % 6);
    if (Often()) Input_Pressed = static_cast<unsigned short>(Next() % 2 ? 0 : Next() & 0xFFFF);

    switch (k) {
    case kOpenScript: {
        static const std::uint32_t kIds[] = {0, 1, 0x3F, 0x40, 0x7FFF, 0x8000, 0xFFFF};
        args[0] = Often() ? kIds[Next() % 7] : Next();
        break;
    }
    case kReset:
        // A leading 0x0C, and the two bytes either side of it.
        if (Often()) {
            unsigned char* const at = const_cast<unsigned char*>(Ptr(kAt));
            at[0] = Next() % 2 ? 0x0C : static_cast<unsigned char>(0x0B + Next() % 3);
        }
        break;
    case kStep:
        SetB(kStepCount, Next() % 4 == 0 ? 0 : 1 + Next() % 8);
        if (Often()) MsgBox_LineX = static_cast<short>(Next() % 0x200);
        if (Often()) SetW(kOriginY, Next() % 0x200);
        if (Often()) SetB(kFlags, (B(kFlags) & ~8u) | (Next() % 2 ? 8 : 0));
        break;
    case kDispatch: SetB(kState, Next() % 8); break;
    case kState2:
    case kState3:
    case kState7: SetB(kSubState, Next() % 2); break;
    case kState4: SetB(kSubState, Next() % 8); break;
    case kState5: SetB(kSubState, Next() % 6); break;
    case kEffectTask: SetB(kEffectKind, Next() % 6); break;
    case kShake:
    case kGrow:
    case kRise: SetB(kEffectPhase, Next() % 2); break;
    case kPrint:
        if (Often()) SetB(kTextSpeed, Next() % 3);
        if (Often()) Input_Held = static_cast<unsigned short>(Next() % 2 ? 0x20 : 0);
        if (Often()) SetW(kFlags, (W(kFlags) & ~0x101u) | (Next() % 2 ? 0x100 : 0) | (Next() % 2));
        break;
    case kDelayState:
    case kState7Delay:
    case kState3Step: {
        static const unsigned char kDelays[] = {0, 1, 2, 0xFF, 0x80};
        if (Often()) SetB(kDelay, kDelays[Next() % 5]);
        if (k == kState3Step) {
            static const unsigned char kRows[] = {5, 6, 7, 0, 0xFF};
            if (Often()) SetB(kColorHigh, kRows[Next() % 5]);
            if (Often()) SetB(kState, Next() % 2 ? 4 + Next() % 2 : Next() % 8);
        }
        break;
    }
    case kChoiceInput:
    case kMenuInput: {
        static const unsigned char kCursors[] = {0, 1, 0x7F, 0x80, 0xFF};
        static const unsigned char kLasts[] = {0, 1, 2, 0x7F, 0x80, 0xFF};
        if (Often()) SetB(kCursor, kCursors[Next() % 5]);
        if (Often()) SetB(kChoiceLast, kLasts[Next() % 6]);
        if (Often()) Input_Pressed = static_cast<unsigned short>(Next() % 2 ? 0 : 1u << (Next() % 16));
        if (Often()) Field_ConfirmButtons = static_cast<unsigned short>(1u << (Next() % 16));
        if (Often()) Field_CancelButtons = static_cast<unsigned short>(1u << (Next() % 16));
        break;
    }
    case kState2Press:
        if (Often()) Input_Pressed = static_cast<unsigned short>(Next() % 2 ? 0 : Next() & 0xFFFF);
        if (Often()) SetB(kFlags, (B(kFlags) & 0x3Fu) | (Next() % 4) << 6);
        break;
    case kState3Arrow:
        if (Often()) Input_Pressed = static_cast<unsigned short>(Next() % 2 ? 0 : Next() & 0xFFFF);
        if (Often()) SetB(kFlags, (B(kFlags) & ~0x40u) | (Next() % 2 ? 0x40 : 0));
        break;
    case kShakeOut:
    case kShakeBack: {
        static const unsigned char kOff[] = {0xFE, 0xFF, 0, 1, 2, 3, 0xFD};
        if (Often()) SetB(kOffsetX, kOff[Next() % 7]);
        break;
    }
    case kWander: {
        static const unsigned char kOff[] = {0xFD, 0xFE, 0, 2, 3, 4};
        static const unsigned char kY[] = {0xF6, 0xF7, 0, 1, 0xFF};
        if (Often()) SetB(kOffsetX, kOff[Next() % 6]);
        if (Often()) SetB(kOffsetY, kY[Next() % 5]);
        if (Often()) SetW(kEffectWait, Next() % 3);
        if (Often()) SetW(kEffectOff, Next() % 2 ? 1 : 0xFFFF);
        break;
    }
    case kGrowStep: {
        static const std::uint16_t kOff[] = {0, 1, 2, 0xFFFE, 0xFFFF, 0x7FFF, 0x8000};
        if (Often()) SetW(kEffectOff, kOff[Next() % 7]);
        if (Often()) SetW(kEffectTimer, Next() % 2 ? 0 : Next() & 0xFFFF);
        if (Often()) SetB(kEffectKind, Next() % 2 ? 2 : 3);
        break;
    }
    case kRiseStep: {
        static const unsigned char kY[] = {0, 1, 0x7F, 0x80, 0xFF};
        if (Often()) SetB(kOffsetY, kY[Next() % 5]);
        if (Often()) SetW(kEffectWait, Next() % 2 ? Next() % 0x400 : Next() & 0xFFFF);
        if (Often()) SetW(kEffectOff, Next() % 2 ? Next() % 0x200 : Next() & 0xFFFF);
        break;
    }
    case kChoiceWaitOpen:
    case kChoiceWaitShut:
    case kChoiceReopen:
    case kChoiceDone:
    case kMenuWaitOpen:
    case kMenuDone:
    case kState6:
    case kState2Closed:
        if (Often()) SetB(kWindow0State, Next() % 6);
        if (Often()) SetB(kWindow1State, Next() % 6);
        break;
    case kWindowAlloc:
        args[0] = Next() % 2 ? Next() % 2 : (Next() & 0xFFFFFF00u) | (Next() % 2);
        args[1] = Next();
        if (Often()) At(kWindows + (args[0] & 0xFF) * 0x24u)[0] = static_cast<unsigned char>(Next() % 2);
        break;
    case kDrawAt:
        args[0] = Next() % 2 ? Next() % 0x200 : Next();
        args[1] = Next() % 2 ? Next() % 0x200 : Next();
        args[2] = Next() % 2 ? Next() % 0x100 : Next();
        args[3] = Next() % 2 ? Next() % 0x40 : Next();
        args[4] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_msg + Next() % 0x100));
        break;
    case kEmitGlyph: {
        static const std::uint32_t kEdge[] = {0, 1, 0xB, 0xC, 0xD, 0x7F, 0x80, 0xFF, 0xFFFF};
        for (int i = 0; i < 7; ++i) args[i] = Next() % 2 ? kEdge[Next() % 9] : Next();
        break;
    }
    default:
        break;
    }
}

// The MsgBoxState block is the first region; the two window records the
// second, at +0x30.
unsigned Block(const State& s, std::uint32_t address) { return s.memory[address - kState]; }
unsigned Window(const State& s, std::uint32_t address) { return s.memory[0x30 + address - kWindows]; }

void Cover(unsigned k, const State& before, const State& after) {
    ++g_cover.ran[k];
    if (k == kStep || k == kPrint) {
        if (Block(after, kStepCount) != Block(before, kStepCount)) ++g_cover.printed;
        if (Block(after, kFlags) & 1) ++g_cover.substituted;
    }
    if (k == kStep && after.log_n != 0) ++g_cover.drawn;
    if (k == kState3Step && Block(after, kColorHigh) != Block(before, kColorHigh)) ++g_cover.scrolled;
    if (k == kPrint && Block(after, kState) != Block(before, kState)) ++g_cover.wrapped;
    if ((k >= kEffectTask && k <= kRiseStep) && Block(before, kEffectKind) != 0 &&
        Block(after, kEffectKind) == 0)
        ++g_cover.effect_ended;
    if ((k == kChoiceInput || k == kMenuInput) && Block(after, kCursor) != Block(before, kCursor))
        ++g_cover.cursor_moved;
    if ((k == kChoiceOpen || k == kMenuOpen || k == kReset || k == kWindowAlloc) &&
        (Window(after, kWindows) != Window(before, kWindows) ||
         Window(after, kWindows + 0x24) != Window(before, kWindows + 0x24)))
        ++g_cover.window_taken;
}

using Fn7 = std::uint32_t(__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                    std::uint32_t, std::uint32_t, std::uint32_t);

void PatchImms(void* copy, const Clone& c) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (int i = 0; i < c.n_imms; ++i) {
        std::uint32_t v;
        std::memcpy(&v, code + c.imms[i].offset, sizeof v);
        if (v != c.imms[i].target)
            bof3::Fatal("msgbox: %s + 0x%X holds 0x%X, not the handler 0x%X", c.name,
                        static_cast<unsigned>(c.imms[i].offset), static_cast<unsigned>(v),
                        static_cast<unsigned>(c.imms[i].target));
        v = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(StubFor(c.imms[i].target)));
        std::memcpy(code + c.imms[i].offset, &v, sizeof v);
    }
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 43000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("msgbox: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    static unsigned char message[kMsgBytes];
    g_msg = message;
    std::memset(g_msg + 0x200, 0x31, kMsgBytes - 0x200);

    // The clones, with every call, every stack-table immediate and both
    // inline jump tables moved into the copy.
    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[8];
        if (c.n_calls > 8) bof3::Fatal("msgbox: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target)};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        PatchImms(clones[k], c);
        if (c.table.entries) move_script::Relocate(clones[k], c.base, c.size, c.table);
    }

    // Ours, called through the same stand-ins, so that each function is
    // tested alone and inject order cannot matter.
    const Callees saved_callees = g;
    Callees s = kOriginals;
    for (unsigned i = 0; i < 8; ++i) s.state[i] = As<void(__cdecl*)()>(StubFor(kStateAddr[i]));
    for (unsigned i = 0; i < 2; ++i) s.sub2[i] = As<void(__cdecl*)()>(StubFor(kSub2Addr[i]));
    for (unsigned i = 0; i < 2; ++i) s.sub3[i] = As<void(__cdecl*)()>(StubFor(kSub3Addr[i]));
    for (unsigned i = 0; i < 8; ++i) s.sub4[i] = As<void(__cdecl*)()>(StubFor(kSub4Addr[i]));
    for (unsigned i = 0; i < 6; ++i) s.sub5[i] = As<void(__cdecl*)()>(StubFor(kSub5Addr[i]));
    for (unsigned i = 0; i < 2; ++i) s.sub7[i] = As<void(__cdecl*)()>(StubFor(kSub7Addr[i]));
    for (unsigned i = 0; i < 6; ++i) s.effect[i] = As<void(__cdecl*)()>(StubFor(kEffectAddr[i]));
    for (unsigned i = 0; i < 2; ++i) s.shake[i] = As<void(__cdecl*)()>(StubFor(kShakeAddr[i]));
    for (unsigned i = 0; i < 2; ++i) s.grow[i] = As<void(__cdecl*)()>(StubFor(kGrowAddr[i]));
    for (unsigned i = 0; i < 2; ++i) s.rise[i] = As<void(__cdecl*)()>(StubFor(kRiseAddr[i]));
    s.reset = As<void(__cdecl*)()>(StubFor(0x497770));
    s.window_alloc = &StubWindowAlloc;
    s.dispatch = As<void(__cdecl*)()>(StubFor(0x497AD0));
    s.effect_task = As<void(__cdecl*)()>(StubFor(0x4984E0));
    s.step = As<void(__cdecl*)()>(StubFor(0x497840));
    s.reopen = As<void(__cdecl*)()>(StubFor(0x498280));
    s.sound = &StubSound;
    s.draw_char = &StubDrawChar;
    s.effect_draw = &StubEffectDraw;
    s.page_arrow = &StubPageArrow;
    s.repeat = &StubRepeat;
    s.draw_string = &StubDrawString;
    s.set_code6c = &StubSetCode;
    s.set_semitrans = &StubSetSemi;
    s.commit = &StubCommit;

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Msg_OpenScript),        reinterpret_cast<const void*>(&MsgBox_Reset),
        reinterpret_cast<const void*>(&MsgBox_FrameTask),      reinterpret_cast<const void*>(&MsgBox_Step),
        reinterpret_cast<const void*>(&MsgBox_StateDispatch),  reinterpret_cast<const void*>(&MsgBox_StatePrint),
        reinterpret_cast<const void*>(&MsgBox_StateDelay),     reinterpret_cast<const void*>(&MsgBox_State2),
        reinterpret_cast<const void*>(&MsgBox_State2Press),    reinterpret_cast<const void*>(&MsgBox_State2Closed),
        reinterpret_cast<const void*>(&MsgBox_State3),         reinterpret_cast<const void*>(&MsgBox_State3Arrow),
        reinterpret_cast<const void*>(&MsgBox_State3Step),     reinterpret_cast<const void*>(&MsgBox_State4),
        reinterpret_cast<const void*>(&MsgBox_ChoiceOpen),     reinterpret_cast<const void*>(&MsgBox_ChoiceWaitOpen),
        reinterpret_cast<const void*>(&MsgBox_ChoiceInput),    reinterpret_cast<const void*>(&MsgBox_ChoiceWaitShut),
        reinterpret_cast<const void*>(&MsgBox_ChoiceReopen),   reinterpret_cast<const void*>(&MsgBox_ChoiceDone),
        reinterpret_cast<const void*>(&MsgBox_Reopen),         reinterpret_cast<const void*>(&MsgBox_State5),
        reinterpret_cast<const void*>(&MsgBox_MenuOpen),       reinterpret_cast<const void*>(&MsgBox_MenuWaitOpen),
        reinterpret_cast<const void*>(&MsgBox_MenuInput),      reinterpret_cast<const void*>(&MsgBox_MenuDone),
        reinterpret_cast<const void*>(&MsgBox_State6),         reinterpret_cast<const void*>(&MsgBox_State7),
        reinterpret_cast<const void*>(&MsgBox_State7Delay),    reinterpret_cast<const void*>(&MsgBox_EffectTask),
        reinterpret_cast<const void*>(&MsgBox_EffectShake),    reinterpret_cast<const void*>(&MsgBox_ShakeOut),
        reinterpret_cast<const void*>(&MsgBox_ShakeBack),      reinterpret_cast<const void*>(&MsgBox_EffectGrow),
        reinterpret_cast<const void*>(&MsgBox_GrowStart),      reinterpret_cast<const void*>(&MsgBox_GrowStep),
        reinterpret_cast<const void*>(&MsgBox_EffectWander),   reinterpret_cast<const void*>(&MsgBox_EffectRise),
        reinterpret_cast<const void*>(&MsgBox_RiseStart),      reinterpret_cast<const void*>(&MsgBox_RiseStep),
        reinterpret_cast<const void*>(&Window_Alloc),          reinterpret_cast<const void*>(&Text_DrawAt),
        reinterpret_cast<const void*>(&Text_EmitGlyph),
    };

    // Everything the rounds write, put back at the end.
    static unsigned char saved_block[kRegionBytes], saved_pool[0x280];
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(saved_block + at, At(r.at), r.size); at += r.size; }
    std::memcpy(saved_pool, At(kScriptPool), sizeof saved_pool);
    const unsigned char saved_name = B(kNameIndex), saved_speed = B(kTextSpeed);
    const std::uint16_t saved_held = Input_Held,
                        saved_confirm = Field_ConfirmButtons,
                        saved_cancel = Field_CancelButtons;
    unsigned char* const saved_packet = Gfx_PacketNext;
    const std::uint32_t saved_selector = L(kPoolSelector);
    Gfx_PacketNext = g_prim;
    BuildPool();
    g = s;

    static State input, their_out, our_out;
    unsigned bad = 0, calls = 0, per[kCount] = {}, bad_per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        BuildMessage();
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, packet); ++i) bytes[i] = static_cast<unsigned char>(Next());
        input.packet = 0x20000 + Next() % 0x20;
        input.ret = 0;
        g_seed = Next();
        std::uint32_t args[7] = {Next(), Next(), Next(), Next(), Next(), Next(), Next()};
        Apply(input);
        Seed(k, args);
        Capture(input, 0);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const void* const fn = pass ? ours[k] : clones[k];
            const std::uint32_t ret = reinterpret_cast<Fn7>(const_cast<void*>(fn))(
                args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
            Capture(pass ? our_out : their_out, ret & kClones[k].ret_mask);
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      msgbox self-test MISMATCH: round %u, %s, log %u / %u, ret %08X / %08X",
                          round, kClones[k].name, their_out.log_n, our_out.log_n,
                          static_cast<unsigned>(their_out.ret), static_cast<unsigned>(our_out.ret));
        }
    }

    g = saved_callees;
    at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), saved_block + at, r.size); at += r.size; }
    std::memcpy(At(kScriptPool), saved_pool, sizeof saved_pool);
    SetB(kNameIndex, saved_name);
    SetB(kTextSpeed, saved_speed);
    Input_Held = static_cast<unsigned short>(saved_held);
    Field_ConfirmButtons = static_cast<unsigned short>(saved_confirm);
    Field_CancelButtons = static_cast<unsigned short>(saved_cancel);
    SetL(kPoolSelector, saved_selector);
    Gfx_PacketNext = saved_packet;

    bof3::Log("shadow      msgbox self-test: %u rounds (%u per function, %u functions), %u calls to the "
              "stand-ins, %u MISMATCHES; the MsgBoxState block, both window records, the three text pens, "
              "Input_Pressed, the primitive, the packet cursor, the return and the stand-ins' log compared",
              kRounds, per[0], static_cast<unsigned>(kCount), calls, bad);
    if (bad) {
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      msgbox: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    }
    const Coverage& c = g_cover;
    bof3::Log("shadow      msgbox coverage: characters stepped %u, in a substitution %u, a glyph drawn %u, "
              "the print state moved %u, a row scrolled %u, an effect ended %u, the cursor moved %u, "
              "a window taken %u",
              c.printed, c.substituted, c.drawn, c.wrapped, c.scrolled, c.effect_ended, c.cursor_moved,
              c.window_taken);
    if (bad) bof3::Fatal("the message box differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace msgbox
