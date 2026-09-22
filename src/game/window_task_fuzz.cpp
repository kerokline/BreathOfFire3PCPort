// BOF3X_SHADOW=window_task: a differential fuzz of the window/task layer and
// the area-change funnel, once at start-up.
//
// Twenty-four byte-copies, every call out re-aimed at a recording stand-in
// (the tail jumps included) and every stack-built dispatch table's immediates
// re-aimed in the copy - `mov [esp + k], imm32` carries an absolute address,
// so a copy that is not patched runs the ORIGINAL handlers, which are exactly
// what we are testing. One round: one of the twenty-four, random bytes in
// every region any of them touches, then each branch's boundaries seeded;
// theirs, then from the same state ours; the regions, the zone lists, the
// music sets, the primitive pool, the packet cursor, the result and the
// stand-ins' log compared. docs/window-task.md section 6.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/window_task_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace window_task {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 72;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

constexpr unsigned kPool = 0x600;
unsigned char g_prim[kPool];
constexpr unsigned kAreas = 4;          // the area numbers the zone lookups are fuzzed with
constexpr unsigned kZoneBytes = 0x40;   // eight 8-byte records, the last a catch-all
unsigned char g_zones[kAreas][kZoneBytes];
constexpr unsigned kSets = 12;          // as many music sets as BOF3.exe's table at 0x669A48 holds
unsigned char g_lists[kSets][16];

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

// A pointer as something both passes can compare: an offset inside whichever
// of our buffers - or the game's window records - it points into.
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p);
    const std::uint32_t pool = Address(g_prim), zones = Address(g_zones), lists = Address(g_lists);
    if (at >= pool && at < pool + sizeof g_prim) return 0x10000u + (at - pool);
    if (at >= zones && at < zones + sizeof g_zones) return 0x20000u + (at - zones);
    if (at >= lists && at < lists + sizeof g_lists) return 0x30000u + (at - lists);
    if (at >= window_task::at::kRecords && at < window_task::at::kRecordsEnd)
        return 0x40000u + (at - window_task::at::kRecords);
    return at;
}

// Every byte below is one some function reads again, or stores, after a call,
// so a read moved before a call or a store moved across one shows. The last
// entry repoints the current record itself - the thing these functions re-read
// after every call out.
struct Watch { std::uint32_t at; unsigned char lo, hi; };   // a value in lo..hi, or any when lo > hi
const Watch kWatch[] = {
    {window_task::at::kPlacement, 1, 0},
    {window_task::at::kListSet, 0, 11},
    {window_task::at::kPass, 0, 4},
    {window_task::at::kAreaTrack, 1, 0},
    {window_task::at::kMusicTrack, 1, 0},
    {window_task::at::kPendingKind, 1, 0},
    {window_task::at::kPendingFlags, 1, 0},
    {window_task::at::kPendingArea, 1, 0},
    {window_task::at::kPendingArea + 1, 1, 0},
};
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return;
    if (h % 23 == 0) {
        // the current record: a different one of the twenty-two
        const std::uint32_t record = window_task::at::kRecords + ((h >> 8) % 22u) * 0x24u;
        SetLong(At(window_task::at::kCurrent), static_cast<std::int32_t>(record));
        return;
    }
    const Watch& w = kWatch[(h >> 4) % (sizeof kWatch / sizeof kWatch[0])];
    const unsigned v = (h >> 12) & 0xFF;
    At(w.at)[0] = static_cast<unsigned char>(w.lo > w.hi ? v : w.lo + v % (w.hi - w.lo + 1u));
}

// --- the stand-ins ---------------------------------------------------------
// Each records what the real callee reads of its arguments - Window_DrawFrame
// and Window_DrawOutline keep only the low 16 bits of x and y and the low byte
// of w and h, Window_DrawLine the low 16 bits of all four - so what the
// original leaves in the upper halves of the registers it pushes is masked
// here exactly as the real callee masks it.

void __cdecl StubClassify() { Record(1); Disturb(); }
void __cdecl StubPickMusic(unsigned x, unsigned z, unsigned area) {
    Record(2, x & 0xFF, z & 0xFF, area & 0xFFFF);
    At(window_task::at::kAreaTrack)[0] = static_cast<unsigned char>(Hash() >> 8);
    Disturb();
}
const unsigned char* __cdecl StubZoneAt(unsigned x, unsigned z, unsigned area) {
    Record(3, x & 0xFF, z & 0xFF, area & 0xFFFF);
    Disturb();
    // a record of the area's own list, so that the caller's reads of +5..+7
    // land where the real lookup would have put them
    return g_zones[(area & 0xFFFFu) % kAreas] + (Hash() % 8u) * 8u;
}
unsigned char __cdecl StubFlagsTest(const unsigned char* bits, unsigned index) {
    const auto set = static_cast<unsigned char>(Hash() % 3 == 0 ? 1 : 0);
    Record(4, Address(bits), index & 0xFF, set);
    Disturb();
    return set;
}
void __cdecl StubDrawFrame(int x, int y, int w, int h) {
    Record(5, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<unsigned>(w) & 0xFF,
           static_cast<unsigned>(h) & 0xFF);
    Disturb();
}
void __cdecl StubDrawOutline(int x, int y, int w, int h) {
    Record(6, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), static_cast<unsigned>(w) & 0xFF,
           static_cast<unsigned>(h) & 0xFF);
    Disturb();
}
void __cdecl StubDrawLine(int x0, int y0, int x1, int y1, unsigned colour) {
    Record(7, static_cast<std::uint16_t>(x0) | static_cast<std::uint32_t>(static_cast<std::uint16_t>(y0)) << 16,
           static_cast<std::uint16_t>(x1) | static_cast<std::uint32_t>(static_cast<std::uint16_t>(y1)) << 16,
           colour & 0xFF);
    Disturb();
}
void __cdecl StubFreeCurrent() { Record(8); Disturb(); }
unsigned char __cdecl StubMsgBoxFrame() {
    Record(9);
    Disturb();
    return static_cast<unsigned char>(Hash() % 2 == 0 ? 0 : 1 + (Hash() >> 16 & 0x7F));
}

// The PSX library leaves, doing what the real ones do to the primitive so that
// the pool comparison covers their bytes too.
constexpr std::uint32_t kPointZeroOne = 0x3C23D70Au;
void PutDword(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(10, Id(prim), static_cast<std::uint32_t>(dfe) | static_cast<std::uint32_t>(dtd) << 16, tpage, Id(reinterpret_cast<const void*>(tw)));
    PutDword(prim + 4, 0xE8000000u | (tpage & 0xFFFFu));
    if (dfe) prim[6] = static_cast<unsigned char>(prim[6] | 1);
    if (dtd) prim[6] = static_cast<unsigned char>(prim[6] | 2);
    PutDword(prim + 8, static_cast<std::uint32_t>(tw));
    Disturb();
}
// The commit moves the packet cursor, as the real one does, so that a cursor
// read too early shows. It wraps inside our pool.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(11, slot & 0xFF, size & 0xFF, Id(Gfx_PacketNext));
    Gfx_PacketNext = g_prim + (static_cast<unsigned>(Gfx_PacketNext - g_prim) + (size & 0xFF)) % 0x100u;
    Disturb();
}
void __cdecl StubSetTile(unsigned char* prim) {
    Record(12, Id(prim));
    prim[7] = 0x60;
    PutDword(prim + 0x10, kPointZeroOne);
}
void __cdecl StubSetPolyGT4(unsigned char* prim) {
    Record(13, Id(prim));
    prim[7] = 0x3C;
    for (unsigned at = 0x10; at <= 0x4C; at += 0x14) PutDword(prim + at, kPointZeroOne);
}
void __cdecl StubSetLineF4(unsigned char* prim) {
    Record(14, Id(prim));
    prim[7] = 0x4C;
    for (unsigned at = 0x10; at <= 0x34; at += 0xC) PutDword(prim + at, kPointZeroOne);
}
void __cdecl StubSetLineF2(unsigned char* prim) {
    Record(15, Id(prim));
    prim[7] = 0x40;
    PutDword(prim + 0x10, kPointZeroOne);
    PutDword(prim + 0x1C, kPointZeroOne);
}
void __cdecl StubSetSemi(unsigned char* prim, unsigned abe) {
    Record(16, Id(prim), abe & 0xFF);
    prim[7] = static_cast<unsigned char>((abe & 1) ? prim[7] | 2 : prim[7] & 0xFD);
}
unsigned __cdecl StubGetTPage(unsigned tp, unsigned abr, int x, int y) {
    Record(17, tp, abr, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    return Hash() & 0xFFFFu;
}
unsigned __cdecl StubGetClut(int x, int y) {
    Record(18, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    return Hash() >> 8 & 0xFFFFu;
}

// The five unread callees of kinds 1 and 2, and every entry of the four
// stack-built tables. Distinct addresses, one template.
template <unsigned Id_> void __cdecl StubHandler() { Record(Id_); Disturb(); }

template <typename T> T Fn(const void* p) { return reinterpret_cast<T>(const_cast<void*>(p)); }

const Callees kStubs = {
    StubClassify,
    StubPickMusic,
    StubZoneAt,
    StubDrawFrame,
    StubDrawOutline,
    StubDrawLine,
    StubFreeCurrent,
    StubFlagsTest,
    StubMsgBoxFrame,
    StubDrawMode,
    StubCommit,
    StubSetTile,
    StubSetPolyGT4,
    StubSetLineF4,
    StubSetLineF2,
    StubSetSemi,
    StubGetTPage,
    StubGetClut,
    &StubHandler<20>,   // 0x596330, kind 1's set-up
    &StubHandler<21>,   // 0x596090, kind 1's list draw
    &StubHandler<22>,   // 0x596120, kind 1's cursor draw
    &StubHandler<23>,   // 0x596020, kind 2's list draw
    &StubHandler<24>,   // 0x5960D0, kind 2's cursor draw
    {&StubHandler<30>, &StubHandler<31>, &StubHandler<32>, &StubHandler<33>, &StubHandler<34>, &StubHandler<35>,
     &StubHandler<36>, &StubHandler<37>, &StubHandler<38>},
    {&StubHandler<40>, &StubHandler<41>, &StubHandler<42>},
    {&StubHandler<50>, &StubHandler<51>, &StubHandler<52>, &StubHandler<53>, &StubHandler<54>},
    {&StubHandler<60>, &StubHandler<61>, &StubHandler<62>, &StubHandler<63>, &StubHandler<64>},
    {&StubHandler<70>, &StubHandler<71>, &StubHandler<72>, &StubHandler<73>, &StubHandler<74>},
};

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case bof3::addr::Area_ClassifyPending: return f(&StubClassify);
    case bof3::addr::Area_PickMusic:       return f(&StubPickMusic);
    case bof3::addr::Area_ZoneAtIn:        return f(&StubZoneAt);
    case bof3::addr::Flags_Test:           return f(&StubFlagsTest);
    case bof3::addr::Window_DrawFrame:     return f(&StubDrawFrame);
    case bof3::addr::Window_DrawOutline:   return f(&StubDrawOutline);
    case bof3::addr::Window_DrawLine:      return f(&StubDrawLine);
    case bof3::addr::Window_FreeCurrent:   return f(&StubFreeCurrent);
    case kMsgBoxFrameTask:                 return f(&StubMsgBoxFrame);
    case bof3::addr::Gpu_SetDrawMode:      return f(&StubDrawMode);
    case bof3::addr::Gfx_CommitPrim:       return f(&StubCommit);
    case bof3::addr::Gpu_SetTile:          return f(&StubSetTile);
    case bof3::addr::Gpu_SetPolyGT4:       return f(&StubSetPolyGT4);
    case bof3::addr::Gpu_SetLineF4:        return f(&StubSetLineF4);
    case bof3::addr::Gpu_SetLineF2:        return f(&StubSetLineF2);
    case bof3::addr::Gpu_SetSemiTrans:     return f(&StubSetSemi);
    case bof3::addr::Gpu_GetTPage:         return f(&StubGetTPage);
    case bof3::addr::Gpu_GetClut:          return f(&StubGetClut);
    case kListSetUp:                       return f(&StubHandler<20>);
    case kListDraw:                        return f(&StubHandler<21>);
    case kListCursorDraw:                  return f(&StubHandler<22>);
    case kSetDraw:                         return f(&StubHandler<23>);
    case kSetCursorDraw:                   return f(&StubHandler<24>);
    default: bof3::Fatal("window_task: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// A `mov dword ptr [esp + k], imm32` in a COPY still names the original
// handler: re-aim it, the way text_draw.cpp relocates a jump table.
struct Imm { std::uint32_t offset, expected; Handler replacement; };
void PatchImm(void* copy, const char* name, const Imm* imms, int n) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (int i = 0; i < n; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + imms[i].offset, sizeof had);
        if (had != imms[i].expected)
            bof3::Fatal("window_task: %s +0x%X holds 0x%X, not the handler 0x%X", name, static_cast<unsigned>(imms[i].offset),
                        static_cast<unsigned>(had), static_cast<unsigned>(imms[i].expected));
        const std::uint32_t to = Address(reinterpret_cast<const void*>(imms[i].replacement));
        std::memcpy(code + imms[i].offset, &to, sizeof to);
    }
}

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls; int n_calls;
    const Imm* imms; int n_imms;
    unsigned args;      // how many of the five dwords the function takes
    unsigned ret;       // 0 nothing to compare, 1 the low byte, 2 the whole dword
};

constexpr Call kChangeAreaCalls[] = {{0x2B, bof3::addr::Area_ClassifyPending}, {0x40, bof3::addr::Area_PickMusic}};
constexpr Call kZoneIdCalls[] = {{0x11, bof3::addr::Area_ZoneAtIn}};
constexpr Call kPickMusicCalls[] = {{0x23, bof3::addr::Area_ZoneAtIn}, {0x78, bof3::addr::Flags_Test}};
constexpr Call kGrowCalls[] = {{0x18C, bof3::addr::Window_DrawOutline}};
constexpr Call kKind0FrameCalls[] = {{0x2D, bof3::addr::Window_DrawFrame}, {0x35, kMsgBoxFrameTask}};
constexpr Call kShrinkCalls[] = {{0x1FA, bof3::addr::Window_DrawOutline}, {0x20E, kMsgBoxFrameTask}};
constexpr Call kKind0CloseCalls[] = {{0x00, kMsgBoxFrameTask}, {0x0E, bof3::addr::Window_FreeCurrent}};
constexpr Call kKind1OpenCalls[] = {{0x00, kListSetUp}};
constexpr Call kKind1FrameCalls[] = {{0x27, bof3::addr::Window_DrawFrame}, {0x2F, kListDraw}, {0x34, kListCursorDraw}};
constexpr Call kFreeStateCalls[] = {{0x00, bof3::addr::Window_FreeCurrent}};
constexpr Call kKind2FrameCalls[] = {{0x27, bof3::addr::Window_DrawFrame}, {0x2F, kSetDraw}, {0x34, kSetCursorDraw}};
constexpr Call kDrawFrameCalls[] = {
    {0x015, bof3::addr::Gpu_SetDrawMode},  {0x01E, bof3::addr::Gfx_CommitPrim},   {0x02A, bof3::addr::Gpu_SetTile},
    {0x09B, bof3::addr::Gpu_SetSemiTrans}, {0x0A4, bof3::addr::Gfx_CommitPrim},   {0x0DF, bof3::addr::Gpu_SetDrawMode},
    {0x0EB, bof3::addr::Gfx_CommitPrim},   {0x0F7, bof3::addr::Gpu_SetPolyGT4},   {0x107, bof3::addr::Gpu_GetTPage},
    {0x123, bof3::addr::Gpu_GetClut},      {0x1D3, bof3::addr::Gpu_SetSemiTrans}, {0x1DC, bof3::addr::Gfx_CommitPrim},
    {0x218, bof3::addr::Gpu_SetDrawMode},  {0x224, bof3::addr::Gfx_CommitPrim},   {0x230, bof3::addr::Gpu_SetLineF4},
    {0x283, bof3::addr::Gfx_CommitPrim},   {0x2A3, bof3::addr::Window_DrawLine},  {0x2B4, bof3::addr::Window_DrawLine},
    {0x2CD, bof3::addr::Window_DrawLine},  {0x2E1, bof3::addr::Window_DrawLine},  {0x2F2, bof3::addr::Window_DrawLine}};
constexpr Call kOutlineCalls[] = {
    {0x0B, bof3::addr::Gpu_SetLineF4}, {0x99, bof3::addr::Gfx_CommitPrim}, {0xB1, bof3::addr::Window_DrawLine}};
constexpr Call kLineCalls[] = {{0x08, bof3::addr::Gpu_SetLineF2}, {0x5E, bof3::addr::Gfx_CommitPrim}};

const Imm kRunImms[] = {
    {0x0F, bof3::addr::Window_Kind0States, &StubHandler<40>},
    {0x17, bof3::addr::Window_Kind1States, &StubHandler<41>},
    {0x22, bof3::addr::Window_Kind2States, &StubHandler<42>},
};
const Imm kKind0Imms[] = {
    {0x0F, bof3::addr::Window_Kind0Open,  &StubHandler<50>},
    {0x17, bof3::addr::Window_Grow,       &StubHandler<51>},
    {0x22, bof3::addr::Window_Kind0Frame, &StubHandler<52>},
    {0x2A, bof3::addr::Window_Shrink,     &StubHandler<53>},
    {0x32, bof3::addr::Window_Kind0Close, &StubHandler<54>},
};
const Imm kKind1Imms[] = {
    {0x0F, bof3::addr::Window_Kind1Open,  &StubHandler<60>},
    {0x17, bof3::addr::Window_Grow,       &StubHandler<61>},
    {0x22, bof3::addr::Window_Kind1Frame, &StubHandler<62>},
    {0x2A, bof3::addr::Window_Shrink,     &StubHandler<63>},
    {0x32, bof3::addr::Window_FreeState,  &StubHandler<64>},
};
const Imm kKind2Imms[] = {
    {0x0F, bof3::addr::Window_Kind2Open,  &StubHandler<70>},
    {0x17, bof3::addr::Window_Grow,       &StubHandler<71>},
    {0x22, bof3::addr::Window_Kind2Frame, &StubHandler<72>},
    {0x2A, bof3::addr::Window_Shrink,     &StubHandler<73>},
    {0x32, bof3::addr::Window_FreeState,  &StubHandler<74>},
};
const Imm kRecordImms[] = {
    {0x0F, kRecordHandlers[0], &StubHandler<30>}, {0x19, kRecordHandlers[1], &StubHandler<31>},
    {0x24, kRecordHandlers[2], &StubHandler<32>}, {0x2C, kRecordHandlers[3], &StubHandler<33>},
    {0x34, kRecordHandlers[4], &StubHandler<34>}, {0x3C, kRecordHandlers[5], &StubHandler<35>},
    {0x44, kRecordHandlers[6], &StubHandler<36>}, {0x4C, kRecordHandlers[7], &StubHandler<37>},
    {0x54, kRecordHandlers[8], &StubHandler<38>},
};

enum : unsigned {
    kChangeArea, kClassify, kZoneId, kPickMusic, kZoneAt,
    kRun, kKind0States, kKind0Open, kGrow, kKind0Frame, kShrink, kKind0Close,
    kKind1States, kKind1Open, kKind1Frame, kFreeState, kKind2States, kKind2Open, kKind2Frame,
    kDrawFrame, kOutline, kLine, kRunRecords, kFree, kCount
};

#define WT_C(name, base, size, calls, args, ret) \
    {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), nullptr, 0, args, ret}
#define WT_I(name, base, size, imms, args, ret) \
    {name, base, size, nullptr, 0, imms, static_cast<int>(sizeof imms / sizeof imms[0]), args, ret}
#define WT_P(name, base, size, args, ret) {name, base, size, nullptr, 0, nullptr, 0, args, ret}
const Clone kClones[kCount] = {
    WT_C("Field_ChangeArea", 0x594E00, 0x52, kChangeAreaCalls, 4, 0),
    WT_P("Area_ClassifyPending", 0x595350, 0x33, 0, 0),
    WT_C("Area_ZoneIdAt", 0x595390, 0x1D, kZoneIdCalls, 2, 1),
    WT_C("Area_PickMusic", 0x5953B0, 0x98, kPickMusicCalls, 3, 0),
    WT_P("Area_ZoneAtIn", 0x595450, 0x53, 3, 2),
    WT_I("Window_Run", 0x5954B0, 0x2E, kRunImms, 0, 0),
    WT_I("Window_Kind0States", 0x5954E0, 0x3E, kKind0Imms, 0, 0),
    WT_P("Window_Kind0Open", 0x595520, 0x14D, 0, 0),
    WT_C("Window_Grow", 0x595670, 0x1A3, kGrowCalls, 0, 0),
    WT_C("Window_Kind0Frame", 0x595820, 0x3A, kKind0FrameCalls, 0, 0),
    WT_C("Window_Shrink", 0x595860, 0x213, kShrinkCalls, 0, 0),
    WT_C("Window_Kind0Close", 0x595A80, 0x14, kKind0CloseCalls, 0, 0),
    WT_I("Window_Kind1States", 0x595AA0, 0x3E, kKind1Imms, 0, 0),
    WT_C("Window_Kind1Open", 0x595AE0, 0x0E, kKind1OpenCalls, 0, 0),
    WT_C("Window_Kind1Frame", 0x595AF0, 0x39, kKind1FrameCalls, 0, 0),
    WT_C("Window_FreeState", 0x595B30, 0x05, kFreeStateCalls, 0, 0),
    WT_I("Window_Kind2States", 0x595B40, 0x3E, kKind2Imms, 0, 0),
    WT_P("Window_Kind2Open", 0x595B80, 0x89, 0, 0),
    WT_C("Window_Kind2Frame", 0x595C10, 0x39, kKind2FrameCalls, 0, 0),
    WT_C("Window_DrawFrame", 0x595C50, 0x302, kDrawFrameCalls, 4, 0),
    WT_C("Window_DrawOutline", 0x595F60, 0xBE, kOutlineCalls, 4, 0),
    WT_C("Window_DrawLine", 0x596150, 0x68, kLineCalls, 5, 0),
    WT_I("Field_RunTaskRecords", 0x59E230, 0x94, kRecordImms, 0, 0),
    WT_P("Window_FreeCurrent", 0x59E310, 0x1D, 0, 0),
};
#undef WT_C
#undef WT_I
#undef WT_P

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {window_task::at::kRecords, 0x318},     // the 22 window records
    {window_task::at::kCurrent, 4},         // the record the layer is running (a pointer: Fix sets it)
    {window_task::at::kPass, 1},
    {0x7DEE40, 0x40},                       // MsgBoxState: the list dword, set, count, cursor, placement and rows
    {window_task::at::kPendingArea, 2},
    {window_task::at::kPendingKind, 1},
    {window_task::at::kPendingX, 4},
    {window_task::at::kPendingZ, 4},
    {window_task::at::kPendingFlags, 1},    // Gfx_BufferIndex is the byte after it: one byte, exactly
    {window_task::at::kAreaTrack, 1},
    {window_task::at::kMusicTrack, 1},
    {0x66C7D8, 1},                          // Field_Request
    {0x9039A2, 2},                          // Field_ScriptFlags
    {window_task::at::kClutRow, 1},
    {0x904EFC, 2},                          // Game_AreaNumber
    {0x903F90, 0x108},                      // Cond_Flags
};
constexpr unsigned kRegionBytes = 0x318 + 4 + 1 + 0x40 + 2 + 1 + 4 + 4 + 1 + 1 + 1 + 1 + 2 + 1 + 2 + 0x108;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char prim[kPool];
    unsigned char zones[sizeof g_zones];
    unsigned char lists[sizeof g_lists];
    std::uint32_t packet;       // Gfx_PacketNext, as an offset into the pool
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.prim, g_prim, sizeof g_prim);
    std::memcpy(s.zones, g_zones, sizeof g_zones);
    std::memcpy(s.lists, g_lists, sizeof g_lists);
    s.packet = static_cast<std::uint32_t>(Gfx_PacketNext - g_prim);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_prim, s.prim, sizeof g_prim);
    std::memcpy(g_zones, s.zones, sizeof g_zones);
    std::memcpy(g_lists, s.lists, sizeof g_lists);
    Gfx_PacketNext = g_prim + s.packet;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x1F123BB5u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

// Random bytes make almost every record dead and almost every dispatch index
// wild: put each one back inside the range the original's stack tables hold,
// and give the zone lists a record that matches everything so that the
// lookup's endless loop always ends.
void Fix() {
    for (std::uint32_t a = window_task::at::kRecords; a < window_task::at::kRecordsEnd; a += 0x24) {
        unsigned char* const r = At(a);
        r[0] = static_cast<unsigned char>(Often() ? 1 + Next() % 3 : 0);
        r[1] = static_cast<unsigned char>(Next() % 9);
        r[2] = static_cast<unsigned char>(Next() % 3);
        r[3] = static_cast<unsigned char>(Next() % 5);
        r[0xF] = static_cast<unsigned char>(Next() % 2 ? Next() % 3 : Next() % 5);
    }
    SetLong(At(window_task::at::kCurrent),
            static_cast<std::int32_t>(window_task::at::kRecords + (Next() % 22u) * 0x24u));
    At(window_task::at::kPass)[0] = static_cast<unsigned char>(Next() % 4 ? Next() % 3 : Next() % 6);
    Game_AreaNumber = static_cast<std::uint16_t>(Next() % kAreas);
    // the zone lists: eight records, the last one matching everything
    for (unsigned a = 0; a < kAreas; ++a) {
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const rec = g_zones[a] + i * 8;
            for (unsigned b = 0; b < 8; ++b) rec[b] = static_cast<unsigned char>(Next());
            if (i == 7) { rec[0] = 0; rec[1] = 0; rec[2] = 0xFF; rec[3] = 0xFF; }
            // the music set index has to stay inside the twelve records we own
            rec[6] = static_cast<unsigned char>(Next() % kSets);
            rec[7] = static_cast<unsigned char>(Often() ? 0 : 1 + Next() % 0xFF);
        }
        SetLong(At(window_task::at::kZoneLists + a * 4u), static_cast<std::int32_t>(Address(g_zones[a])));
    }
    // the music sets: a pointer and a count each, then count 3-byte entries
    // and the default byte after them
    for (unsigned s = 0; s < kSets; ++s) {
        for (unsigned b = 0; b < sizeof g_lists[s]; ++b) g_lists[s][b] = static_cast<unsigned char>(Next());
        const unsigned count = Next() % 5;
        SetLong(At(window_task::at::kMusicSets + s * 8u), static_cast<std::int32_t>(Address(g_lists[s])));
        At(window_task::at::kMusicSets + s * 8u)[4] = static_cast<unsigned char>(count);
    }
}

struct Args { std::uint32_t a[5]; };

// Each branch's boundaries, on top of the random bytes.
Args Seed(unsigned k) {
    Args args;
    for (std::uint32_t& v : args.a) v = Next();
    unsigned char* const r = At(static_cast<std::uint32_t>(Long(At(window_task::at::kCurrent))));
    static const std::uint16_t kAreaKindValues[] = {0x10, 0x21, 0x2D, 0x41, 0x57, 0x58, 0x68, 0x73, 0x79, 0x97, 0x98};
    static const unsigned char kHalves[] = {0, 1, 2, 0x20, 0x36, 0x47, 0x78, 0xC8, 0xFF};
    static const std::uint16_t kSteps[] = {0, 1, 2, 0x10, 0x66, 0xE3, 0x7FFF, 0x8000, 0xFFFF};
    switch (k) {
    case kChangeArea:
        args.a[0] = Often() ? kAreaKindValues[Next() % 11] : Next() % kAreas;
        // a 16.16 position whose integer part lands inside a zone box
        args.a[1] = (Next() % 0x100u) << 16 | (Next() & 0xFFFFu);
        args.a[2] = (Next() % 0x100u) << 16 | (Next() & 0xFFFFu);
        args.a[3] = Next() & 0xFF;
        if (Half()) Field_ScriptFlags = static_cast<std::uint16_t>(Next() % 2 ? Field_ScriptFlags | 0x80 : Field_ScriptFlags & ~0x80);
        break;
    case kClassify:
        if (Often()) SetWord(At(window_task::at::kPendingArea), kAreaKindValues[Next() % 11]);
        else if (Half()) SetWord(At(window_task::at::kPendingArea), Next() % 0x100);
        break;
    case kZoneId:
        args.a[0] = Often() ? Next() % 0x100 : Next();
        args.a[1] = Often() ? Next() % 0x100 : Next();
        break;
    case kPickMusic:
        args.a[0] = Often() ? Next() % 0x100 : Next();
        args.a[1] = Often() ? Next() % 0x100 : Next();
        args.a[2] = Next() % kAreas;
        Field_ScriptFlags = static_cast<std::uint16_t>(Next() % 4 == 0 ? Field_ScriptFlags | 0x80 : Field_ScriptFlags & ~0x80);
        break;
    case kZoneAt:
        args.a[0] = Often() ? Next() % 0x100 : Next();
        args.a[1] = Often() ? Next() % 0x100 : Next();
        args.a[2] = Next() % kAreas;
        break;
    case kKind0Open:
        // the two high bits of the placement code, and an index inside the tables
        At(window_task::at::kPlacement)[0] =
            static_cast<unsigned char>((Next() % 8u) | (Half() ? 0x80 : 0) | (Half() ? 0x40 : 0));
        break;
    case kKind2Open:
        At(window_task::at::kPlacement)[0] = static_cast<unsigned char>(Often() ? Next() % 8 : Next());
        break;
    case kGrow:
    case kShrink: {
        r[0xA] = kHalves[Next() % 9];
        r[0xB] = kHalves[Next() % 9];
        SetWord(r + 0x14, Often() ? static_cast<unsigned>(Next() % 0x40) : kSteps[Next() % 9]);
        SetWord(r + 0x16, Often() ? static_cast<unsigned>(Next() % 0x40) : kSteps[Next() % 9]);
        // a size right at the edge the settle test compares
        static const int kDelta[] = {-0x11, -0x10, -1, 0, 1, 0xF, 0x10, 0x11};
        if (Often()) SetWord(r + 0x10, static_cast<unsigned>((r[0xA] << 4) + kDelta[Next() % 8]));
        if (Often()) SetWord(r + 0x12, static_cast<unsigned>((r[0xB] << 4) + kDelta[Next() % 8]));
        if (k == kShrink) {
            static const std::uint16_t kSmall[] = {0, 1, 0x10, 0xFFFF, 0xFFF0, 0xFFEF, 0x8000, 0x7FFF};
            if (Often()) SetWord(r + 0x10, kSmall[Next() % 8]);
            if (Often()) SetWord(r + 0x12, kSmall[Next() % 8]);
        }
        r[0xD] = static_cast<unsigned char>(Next() % 4);
        At(window_task::at::kPlacement)[0] = static_cast<unsigned char>(Often() ? Next() % 8 : Next());
        At(window_task::at::kListSet)[0] = static_cast<unsigned char>(Often() ? Next() % 8 : Next());
        break;
    }
    case kKind0Frame:
    case kKind1Frame:
    case kKind2Frame:
        r[0xD] = static_cast<unsigned char>(Next() % 4);
        break;
    case kDrawFrame:
    case kOutline:
        args.a[0] = Half() ? static_cast<std::uint32_t>(Next() % 0x200) : Next();
        args.a[1] = Half() ? static_cast<std::uint32_t>(Next() % 0x200) : Next();
        args.a[2] = Half() ? Next() % 0x100 : Next();
        args.a[3] = Half() ? Next() % 0x100 : Next();
        break;
    case kLine:
        for (unsigned i = 0; i < 4; ++i) args.a[i] = Half() ? static_cast<std::uint32_t>(Next() % 0x200) : Next();
        args.a[4] = Half() ? Next() % 0x100 : Next();
        break;
    default:
        break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned area_kind, area_other, music_kept, music_direct, music_list, music_default;
    unsigned grow_x, grow_y, grow_done, shrink_x, shrink_y, shrink_done, drew, box_alt, box_hidden;
    unsigned records_run, freed, pass_skipped;
} g_cover;
bool Logged(const State& s, std::uint32_t what) {
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i)
        if (s.log[i].what == what) return true;
    return false;
}
unsigned LogCount(const State& s, std::uint32_t what) {
    unsigned n = 0;
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i)
        if (s.log[i].what == what) ++n;
    return n;
}
unsigned Byte(const State& s, std::uint32_t address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + (address - r.at)];
        at += r.size;
    }
    return 0x100;
}
void Cover(unsigned k, const State& in, const State& out) {
    switch (k) {
    case kClassify:
        if (Byte(out, window_task::at::kPendingKind) == 0xB) ++g_cover.area_kind;
        else ++g_cover.area_other;
        break;
    case kPickMusic: {
        // no zone lookup is the "keep the current track" path; a lookup with
        // no flag test is byte +7 zero or an empty list; a lookup whose last
        // test answered 1 found its track, and one whose last answered 0 fell
        // off the end onto the byte after the list
        unsigned tests = 0, last = 0;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 4) { ++tests; last = out.log[i].c; }
        if (!Logged(out, 3)) ++g_cover.music_kept;
        else if (tests == 0) ++g_cover.music_direct;
        else if (last) ++g_cover.music_list;
        else ++g_cover.music_default;
        break;
    }
    case kGrow:
    case kShrink:
        if (Logged(out, 6)) ++g_cover.drew;
        break;
    case kRunRecords:
        g_cover.records_run += LogCount(out, 30);
        if (Byte(in, window_task::at::kPass) > 2) ++g_cover.pass_skipped;
        break;
    case kKind0Close:
        if (Logged(out, 8)) ++g_cover.freed;
        break;
    default:
        break;
    }
    // the grow and shrink settle counters, read off the record they ran on
    if (k == kGrow || k == kShrink) {
        const std::uint32_t record = static_cast<std::uint32_t>(Long(in.memory + 0x318));   // kRegions[1], the current record
        const unsigned slot = (record - window_task::at::kRecords) / 0x24u;
        if (slot < 22) {
            const unsigned before = in.memory[slot * 0x24 + 3], after = out.memory[slot * 0x24 + 3];
            if (after != before) { if (k == kGrow) ++g_cover.grow_done; else ++g_cover.shrink_done; }
            const unsigned w_before = in.memory[slot * 0x24 + 0x10] | in.memory[slot * 0x24 + 0x11] << 8;
            const unsigned w_after = out.memory[slot * 0x24 + 0x10] | out.memory[slot * 0x24 + 0x11] << 8;
            const unsigned h_before = in.memory[slot * 0x24 + 0x12] | in.memory[slot * 0x24 + 0x13] << 8;
            const unsigned h_after = out.memory[slot * 0x24 + 0x12] | out.memory[slot * 0x24 + 0x13] << 8;
            if (w_after == 0 || (k == kGrow && w_after == static_cast<unsigned>(in.memory[slot * 0x24 + 0xA] << 4)))
                if (w_after != w_before) { if (k == kGrow) ++g_cover.grow_x; else ++g_cover.shrink_x; }
            if (h_after == 0 || (k == kGrow && h_after == static_cast<unsigned>(in.memory[slot * 0x24 + 0xB] << 4)))
                if (h_after != h_before) { if (k == kGrow) ++g_cover.grow_y; else ++g_cover.shrink_y; }
            if ((in.memory[slot * 0x24 + 0xD] & 2) != 0) ++g_cover.box_alt;
            if ((in.memory[slot * 0x24 + 0xD] & 1) != 0) ++g_cover.box_hidden;
        }
    }
}

using Fn5 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 33600;   // 1,400 rounds per function
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("window_task: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[24];
        if (c.n_calls > 24) bof3::Fatal("window_task: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target)};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.n_imms) PatchImm(clones[k], c.name, c.imms, c.n_imms);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Field_ChangeArea),     reinterpret_cast<const void*>(&Area_ClassifyPending),
        reinterpret_cast<const void*>(&Area_ZoneIdAt),        reinterpret_cast<const void*>(&Area_PickMusic),
        reinterpret_cast<const void*>(&Area_ZoneAtIn),        reinterpret_cast<const void*>(&Window_Run),
        reinterpret_cast<const void*>(&Window_Kind0States),   reinterpret_cast<const void*>(&Window_Kind0Open),
        reinterpret_cast<const void*>(&Window_Grow),          reinterpret_cast<const void*>(&Window_Kind0Frame),
        reinterpret_cast<const void*>(&Window_Shrink),        reinterpret_cast<const void*>(&Window_Kind0Close),
        reinterpret_cast<const void*>(&Window_Kind1States),   reinterpret_cast<const void*>(&Window_Kind1Open),
        reinterpret_cast<const void*>(&Window_Kind1Frame),    reinterpret_cast<const void*>(&Window_FreeState),
        reinterpret_cast<const void*>(&Window_Kind2States),   reinterpret_cast<const void*>(&Window_Kind2Open),
        reinterpret_cast<const void*>(&Window_Kind2Frame),    reinterpret_cast<const void*>(&Window_DrawFrame),
        reinterpret_cast<const void*>(&Window_DrawOutline),   reinterpret_cast<const void*>(&Window_DrawLine),
        reinterpret_cast<const void*>(&Field_RunTaskRecords), reinterpret_cast<const void*>(&Window_FreeCurrent)};

    static State saved, input, their_out, our_out;
    unsigned char* const saved_packet = Gfx_PacketNext;
    const unsigned short saved_cw = GetControlWord();
    std::uint32_t saved_zone[kAreas], saved_sets[kSets * 2];
    for (unsigned a = 0; a < kAreas; ++a)
        saved_zone[a] = static_cast<std::uint32_t>(Long(At(window_task::at::kZoneLists + a * 4u)));
    for (unsigned s = 0; s < kSets * 2; ++s)
        saved_sets[s] = static_cast<std::uint32_t>(Long(At(window_task::at::kMusicSets + s * 4u)));
    Gfx_PacketNext = g_prim;
    Capture(saved);
    g = kStubs;

    unsigned bad = 0, calls = 0, per[kCount] = {}, bad_per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        for (unsigned i = 0; i < kRegionBytes; ++i) input.memory[i] = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.prim) b = static_cast<unsigned char>(Next());
        std::memcpy(input.zones, g_zones, sizeof input.zones);
        std::memcpy(input.lists, g_lists, sizeof input.lists);
        input.packet = Next() % 0x80u;
        input.result = 0;
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        g_seed = Next();
        const Args args = Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? ours[k] : clones[k];
            SetControlWord(kGameControlWord);
            const std::uint32_t r = reinterpret_cast<Fn5>(const_cast<void*>(fn))(args.a[0], args.a[1], args.a[2],
                                                                                args.a[3], args.a[4]);
            Capture(out);
            // a byte return is `al` alone - the rest of eax is the zone record's
            // address, which no caller reads; a pointer return is compared as an
            // offset inside the buffer both passes were given
            out.result = kClones[k].ret == 0 ? 0u : kClones[k].ret == 1 ? (r & 0xFFu) : Id(reinterpret_cast<const void*>(r));
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      window_task self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result);
        }
    }
    g = kOriginals;
    Apply(saved);
    Gfx_PacketNext = saved_packet;
    SetControlWord(saved_cw);
    for (unsigned a = 0; a < kAreas; ++a)
        SetLong(At(window_task::at::kZoneLists + a * 4u), static_cast<std::int32_t>(saved_zone[a]));
    for (unsigned s = 0; s < kSets * 2; ++s)
        SetLong(At(window_task::at::kMusicSets + s * 4u), static_cast<std::int32_t>(saved_sets[s]));

    bof3::Log("shadow      window_task self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the 22 window records, the current record, the pass byte, MsgBoxState, the pending area "
              "block, the flags, Cond_Flags, the zone lists, the music sets, the primitive pool, the packet cursor, "
              "the result and the stand-ins' log compared",
              kRounds, static_cast<unsigned>(kCount), per[0], calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      window_task: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      window_task coverage: pending area one of the eleven %u / not %u; music kept %u, straight "
              "from the zone or an empty list %u, off a flag that was set %u, the byte after the list %u; grow settled x %u y %u both %u, shrink "
              "settled x %u y %u both %u; an outline drawn %u, narrow box %u, frame hidden %u; records run %u, pass "
              "above 2 %u, record freed %u",
              c.area_kind, c.area_other, c.music_kept, c.music_direct, c.music_list, c.music_default, c.grow_x,
              c.grow_y, c.grow_done, c.shrink_x, c.shrink_y, c.shrink_done, c.drew, c.box_alt, c.box_hidden,
              c.records_run, c.pass_skipped, c.freed);
    if (bad) bof3::Fatal("the window/task layer differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace window_task
