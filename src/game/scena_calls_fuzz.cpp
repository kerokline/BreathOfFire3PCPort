// BOF3X_SHADOW=scena_calls: group CALLS's 98 call-table entries through the
// scenario harness (scenario_harness.h), once at start-up.
// docs/scena_calls.md section 4.
//
// The clone table is tools/scenario_rows.py --unit CALLS --clones
// (2026-09-27, the tip 3e410e7; capstone, every jump internal, no jump table,
// nothing REFUSED), names given. Every entry is Shape::kEntry: reached only
// through Scenario_CallA / Scenario_CallB, ten random words (no entry reads
// one), void (none of the 153 + 119 call sites reads eax after the call).
//
// One chapter suffices: no entry reads the chapter bytes, the flag row or
// anything the chapter selects - every callee is a recorder, and the only
// thing the table a call came through decides is ecx, the entry's index,
// which two entries read (below). So one Run, chapter 9, one of the chapters
// whose tables hold both of them; the per-entry chapters and indexes are in
// docs/scena_calls.md section 2.
//
// ecx. 0x51A300 and 0x51AB50 push the caller's ecx as a local and read its
// bytes back when there are fewer than three members. The harness calls a
// clone with whatever ecx its own code left, so for those two this file hands
// the harness, as the original, a copy of Capcom's bytes behind one
// instruction of its own - `mov ecx, [g_calls_ecx]` - and, as ours, a naked
// stub that loads the same cell and jumps to our entry: both run with the ecx
// the seed chose (the entry's table indexes, a byte, or any word).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_calls.h"
#include "game/scena_calls_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

extern "C" {
std::uint32_t g_calls_ecx = 0;
__attribute__((naked)) void CallsSaveAndLeaveOurs() { asm("movl _g_calls_ecx, %ecx\n\tjmp _ScenaCall_SaveAndLeave"); }
__attribute__((naked)) void CallsLeaveAllParty7Ours() { asm("movl _g_calls_ecx, %ecx\n\tjmp _Scena15_LeaveAllParty7"); }
}

namespace scena_calls {
namespace {

namespace sh = scenario_harness;

struct Row {
    const char* name;
    std::uint32_t base, size;
    const sh::CallSite* calls;
    int n_calls;
    const void* ours;
};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
// tools/scenario_rows.py --unit CALLS --clones, 2026-09-27, names given.
constexpr sh::CallSite kCalls5198F0[] = {{0x1D, 0x5367E0}, {0x40, 0x533EF0}};
constexpr sh::CallSite kCalls519940[] = {{0x1D, 0x5367E0}, {0x2B, 0x533EF0}, {0x32, 0x533EF0}, {0x39, 0x533EF0}};
constexpr sh::CallSite kCalls519990[] = {{0x2, 0x533EF0}};
constexpr sh::CallSite kCalls5199A0[] = {{0x2, 0x534030}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls5199B0[] = {{0x35, 0x534030}, {0x3C, 0x534030}, {0x44, 0x533E00}};
constexpr sh::CallSite kCalls519A00[] = {{0x8, 0x5367E0}, {0x36, 0x533EF0}};
constexpr sh::CallSite kCalls519A40[] = {{0x2, 0x533EF0}};
constexpr sh::CallSite kCalls519A50[] = {{0x2, 0x534030}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls519A60[] = {{0x8, 0x5367E0}, {0x36, 0x533EF0}, {0x3D, 0x533EF0}};
constexpr sh::CallSite kCalls519AB0[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}};
constexpr sh::CallSite kCalls519AE0[] = {{0x17, 0x533CE0}, {0x1E, 0x533EF0}, {0x25, 0x533EF0}, {0x2C, 0x533EF0}};
constexpr sh::CallSite kCalls519B20[] = {{0x2, 0x533EF0}};
constexpr sh::CallSite kCalls519B30[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}};
constexpr sh::CallSite kCalls519B50[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}};
constexpr sh::CallSite kCalls519B70[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}};
constexpr sh::CallSite kCalls519B90[] = {{0x2, 0x533EF0}};
constexpr sh::CallSite kCalls519BA0[] = {{0x2, 0x534030}, {0x9, 0x534030}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls519BC0[] = {{0x2, 0x534030}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls519BD0[] = {{0x2, 0x534030}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls519BE0[] = {{0x2, 0x534030}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls519BF0[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x25, 0x533E00}};
constexpr sh::CallSite kCalls519C20[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls519C50[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls519C80[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x25, 0x533E00}};
constexpr sh::CallSite kCalls519CB0[] = {{0x2, 0x533EF0}};
constexpr sh::CallSite kCalls519CC0[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}};
constexpr sh::CallSite kCalls519CE0[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}};
constexpr sh::CallSite kCalls519D00[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}};
constexpr sh::CallSite kCalls519D30[] = {{0x2, 0x534030}, {0x9, 0x534030}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls519D50[] = {{0x2, 0x534030}, {0x9, 0x534030}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls519D70[] = {{0x2, 0x534030}, {0x9, 0x534030}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls519D90[] = {{0x2, 0x534030}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls519DA0[] = {{0x2, 0x534030}, {0x9, 0x534030}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls519DC0[] = {{0x2, 0x534030}, {0x9, 0x534030}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls519DE0[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}};
constexpr sh::CallSite kCalls519E10[] = {{0x6, 0x534030}};
constexpr sh::CallSite kCalls519E20[] = {{0x2, 0x534030}};
constexpr sh::CallSite kCalls519EA0[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls519ED0[] = {{0x1E, 0x533EF0}};
constexpr sh::CallSite kCalls519F00[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x25, 0x533E00}};
constexpr sh::CallSite kCalls519F30[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}, {0x2C, 0x533E00}};
constexpr sh::CallSite kCalls519FA0[] = {{0x8, 0x5367E0}, {0x10, 0x519F70}};
constexpr sh::CallSite kCalls519FC0[] = {{0x2, 0x534030}};
constexpr sh::CallSite kCalls519FD0[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A000[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A030[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x25, 0x533E00}};
constexpr sh::CallSite kCalls51A060[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A090[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A0C0[] = {{0x2, 0x533CE0}, {0x9, 0x533EF0}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls51A0E0[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x25, 0x533E00}};
constexpr sh::CallSite kCalls51A110[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A140[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A170[] = {{0x31, 0x5367E0}, {0x3F, 0x533EF0}, {0x46, 0x533EF0}, {0x4C, 0x533EF0}, {0x54, 0x533E00}};
constexpr sh::CallSite kCalls51A1D0[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A200[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x25, 0x533E00}};
constexpr sh::CallSite kCalls51A230[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A260[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A290[] = {{0x2, 0x534030}, {0x9, 0x534030}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls51A2B0[] = {{0x2, 0x534030}, {0x9, 0x534030}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls51A2D0[] = {{0x2, 0x534030}, {0x9, 0x534030}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls51A2F0[] = {{0x2, 0x534030}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls51A300[] = {{0x3E, 0x534030}, {0x4A, 0x533E00}};
constexpr sh::CallSite kCalls51A360[] = {{0x2, 0x534030}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls51A370[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}};
constexpr sh::CallSite kCalls51A3A0[] = {{0x2B, 0x534030}, {0x4A, 0x5367E0}, {0x58, 0x533EF0}, {0x5F, 0x533EF0}};
constexpr sh::CallSite kCalls51A410[] = {{0x2, 0x533EF0}};
constexpr sh::CallSite kCalls51A420[] = {{0x2, 0x533EF0}};
constexpr sh::CallSite kCalls51A430[] = {{0x2B, 0x5367E0}, {0x43, 0x533EF0}};
constexpr sh::CallSite kCalls51A490[] = {{0x17, 0x534030}, {0x1F, 0x533E00}};
constexpr sh::CallSite kCalls51A4C0[] = {{0x2E, 0x534030}, {0x3A, 0x533E00}};
constexpr sh::CallSite kCalls51A510[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51A540[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x25, 0x533E00}};
constexpr sh::CallSite kCalls51A570[] = {{0x8, 0x5367E0}, {0xF, 0x533CE0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x25, 0x533E00}};
constexpr sh::CallSite kCalls51A5A0[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}};
constexpr sh::CallSite kCalls51A5D0[] = {{0x2E, 0x534030}, {0x42, 0x5367E0}, {0x50, 0x533EF0}, {0x58, 0x533E00}};
constexpr sh::CallSite kCalls51A630[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}};
constexpr sh::CallSite kCalls51A650[] = {{0x2C, 0x5367E0}, {0x39, 0x533EF0}, {0x3F, 0x533EF0}, {0x45, 0x533EF0}};
constexpr sh::CallSite kCalls51A6C0[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}, {0x2C, 0x533E00}};
constexpr sh::CallSite kCalls51A700[] = {{0x2A, 0x534030}, {0x3E, 0x5367E0}, {0x4C, 0x533EF0}, {0x54, 0x533E00}};
constexpr sh::CallSite kCalls51A760[] = {{0x2, 0x533EF0}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls51A770[] = {{0x2, 0x533EF0}, {0xA, 0x533E00}};
constexpr sh::CallSite kCalls51A780[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}};
constexpr sh::CallSite kCalls51A7B0[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}};
constexpr sh::CallSite kCalls51A7E0[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}};
constexpr sh::CallSite kCalls51A810[] = {{0x38, 0x534030}, {0x52, 0x5367E0}, {0x60, 0x533EF0}, {0x68, 0x533E00}};
constexpr sh::CallSite kCalls51A890[] = {{0x1D, 0x5367E0}, {0x2B, 0x533EF0}, {0x32, 0x533EF0}, {0x39, 0x533EF0}, {0x41, 0x533E00}};
constexpr sh::CallSite kCalls51A8E0[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}, {0x2C, 0x533E00}};
constexpr sh::CallSite kCalls51A930[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}, {0x2C, 0x533E00}};
constexpr sh::CallSite kCalls51A970[] = {{0xC, 0x5367E0}, {0x1A, 0x533EF0}, {0x21, 0x533EF0}, {0x2D, 0x533EF0}, {0x35, 0x533E00}};
constexpr sh::CallSite kCalls51A9B0[] = {{0x2B, 0x534030}, {0x3F, 0x5367E0}, {0x4D, 0x533EF0}, {0x55, 0x533E00}};
constexpr sh::CallSite kCalls51AA10[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1E, 0x533E00}};
constexpr sh::CallSite kCalls51AA40[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}, {0x2C, 0x533E00}};
constexpr sh::CallSite kCalls51AA80[] = {{0x3B, 0x5367E0}, {0x49, 0x533EF0}, {0x50, 0x533EF0}, {0x56, 0x533EF0}, {0x5E, 0x533E00}};
constexpr sh::CallSite kCalls51AAF0[] = {{0x42, 0x534030}};
constexpr sh::CallSite kCalls51AB50[] = {{0x35, 0x534030}, {0x49, 0x5367E0}, {0x57, 0x533EF0}, {0x5F, 0x533E00}};
constexpr sh::CallSite kCalls51ABC0[] = {{0x2, 0x533EF0}, {0x9, 0x533EF0}, {0x11, 0x533E00}};
constexpr sh::CallSite kCalls51ABE0[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}, {0x2C, 0x533E00}};
constexpr sh::CallSite kCalls51AC20[] = {{0x8, 0x5367E0}, {0x16, 0x533EF0}, {0x1D, 0x533EF0}, {0x24, 0x533EF0}};
#define CALLS_ROW(name, base, size, calls) {#name, base, size, calls, SH_N(calls), reinterpret_cast<const void*>(&::name)}
const Row kRows[] = {
    CALLS_ROW(Scena01_Set934Party9, 0x5198F0, 0x49, kCalls5198F0),
    CALLS_ROW(Scena01_Party034, 0x519940, 0x42, kCalls519940),
    CALLS_ROW(ScenaCall_Join4, 0x519990, 0x9, kCalls519990),
    CALLS_ROW(ScenaCall_Leave4, 0x5199A0, 0xF, kCalls5199A0),
    CALLS_ROW(Scena02_Leave43, 0x5199B0, 0x49, kCalls5199B0),
    CALLS_ROW(Scena03_Set012Party0, 0x519A00, 0x3F, kCalls519A00),
    CALLS_ROW(ScenaCall_Join1, 0x519A40, 0x9, kCalls519A40),
    CALLS_ROW(ScenaCall_Leave1, 0x519A50, 0xF, kCalls519A50),
    CALLS_ROW(Scena04_Set015Party01, 0x519A60, 0x46, kCalls519A60),
    CALLS_ROW(ScenaCall_Party015, 0x519AB0, 0x2D, kCalls519AB0),
    CALLS_ROW(Scena05_ReloadJoin516, 0x519AE0, 0x35, kCalls519AE0),
    CALLS_ROW(Scena05_Join0, 0x519B20, 0x9, kCalls519B20),
    CALLS_ROW(Scena05_Set015ReloadJoin5, 0x519B30, 0x1F, kCalls519B30),
    CALLS_ROW(Scena05_Set015ReloadJoin1, 0x519B50, 0x1F, kCalls519B50),
    CALLS_ROW(Scena05_Set016ReloadJoin6, 0x519B70, 0x1F, kCalls519B70),
    CALLS_ROW(ScenaCall_Join2, 0x519B90, 0x9, kCalls519B90),
    CALLS_ROW(Scena05_Leave51, 0x519BA0, 0x16, kCalls519BA0),
    CALLS_ROW(ScenaCall_Leave5, 0x519BC0, 0xF, kCalls519BC0),
    CALLS_ROW(ScenaCall_Leave6, 0x519BD0, 0xF, kCalls519BD0),
    CALLS_ROW(Scena05_Leave0, 0x519BE0, 0xF, kCalls519BE0),
    CALLS_ROW(Scena06_Set015ReloadJoin05, 0x519BF0, 0x2A, kCalls519BF0),
    CALLS_ROW(Scena06_Set015ReloadJoin5, 0x519C20, 0x23, kCalls519C20),
    CALLS_ROW(Scena06_Set015ReloadJoin1, 0x519C50, 0x23, kCalls519C50),
    CALLS_ROW(Scena06_Set015ReloadJoin15, 0x519C80, 0x2A, kCalls519C80),
    CALLS_ROW(ScenaCall_Join6, 0x519CB0, 0x9, kCalls519CB0),
    CALLS_ROW(ScenaCall_Set012ReloadJoin1, 0x519CC0, 0x1F, kCalls519CC0),
    CALLS_ROW(Scena06_Set012ReloadJoin2, 0x519CE0, 0x1F, kCalls519CE0),
    CALLS_ROW(Scena06_Set012ReloadJoin12, 0x519D00, 0x26, kCalls519D00),
    CALLS_ROW(Scena06_Leave02, 0x519D30, 0x16, kCalls519D30),
    CALLS_ROW(Scena06_Leave05, 0x519D50, 0x16, kCalls519D50),
    CALLS_ROW(Scena06_Leave06, 0x519D70, 0x16, kCalls519D70),
    CALLS_ROW(ScenaCall_Leave2, 0x519D90, 0xF, kCalls519D90),
    CALLS_ROW(Scena06_Leave62, 0x519DA0, 0x16, kCalls519DA0),
    CALLS_ROW(Scena06_Leave56, 0x519DC0, 0x16, kCalls519DC0),
    CALLS_ROW(Scena07_Party012, 0x519DE0, 0x2D, kCalls519DE0),
    CALLS_ROW(Scena07_LeaveThird, 0x519E10, 0xD, kCalls519E10),
    CALLS_ROW(Scena07_Leave2Out1562, 0x519E20, 0x78, kCalls519E20),
    CALLS_ROW(Scena08_Set72APartyA, 0x519EA0, 0x23, kCalls519EA0),
    CALLS_ROW(Scena08_Party7, 0x519ED0, 0x25, kCalls519ED0),
    CALLS_ROW(Scena08_Set724Party72, 0x519F00, 0x2A, kCalls519F00),
    CALLS_ROW(ScenaCall_Party728, 0x519F30, 0x31, kCalls519F30),
    CALLS_ROW(Scena08_SetParty784, 0x519FA0, 0x15, kCalls519FA0),
    CALLS_ROW(Scena08_Leave2, 0x519FC0, 0x9, kCalls519FC0),
    CALLS_ROW(Scena09_Set782ReloadJoin8, 0x519FD0, 0x23, kCalls519FD0),
    CALLS_ROW(Scena09_Set782ReloadJoin2, 0x51A000, 0x23, kCalls51A000),
    CALLS_ROW(ScenaCall_Set785ReloadJoin85, 0x51A030, 0x2A, kCalls51A030),
    CALLS_ROW(ScenaCall_Set785ReloadJoin5, 0x51A060, 0x23, kCalls51A060),
    CALLS_ROW(ScenaCall_Set785ReloadJoin8, 0x51A090, 0x23, kCalls51A090),
    CALLS_ROW(Scena09_ReloadJoin6, 0x51A0C0, 0x16, kCalls51A0C0),
    CALLS_ROW(ScenaCall_Set765ReloadJoin56, 0x51A0E0, 0x2A, kCalls51A0E0),
    CALLS_ROW(ScenaCall_Set765ReloadJoin6, 0x51A110, 0x23, kCalls51A110),
    CALLS_ROW(Scena09_Set765ReloadJoin5, 0x51A140, 0x23, kCalls51A140),
    CALLS_ROW(ScenaCall_Party72Saved, 0x51A170, 0x5C, kCalls51A170),
    CALLS_ROW(Scena09_Set748ReloadJoin8, 0x51A1D0, 0x23, kCalls51A1D0),
    CALLS_ROW(Scena09_Set748ReloadJoin48, 0x51A200, 0x2A, kCalls51A200),
    CALLS_ROW(Scena09_Set748ReloadJoin4, 0x51A230, 0x23, kCalls51A230),
    CALLS_ROW(Scena09_Set748ReloadJoin7, 0x51A260, 0x23, kCalls51A260),
    CALLS_ROW(Scena09_Leave42, 0x51A290, 0x16, kCalls51A290),
    CALLS_ROW(Scena09_Leave28, 0x51A2B0, 0x16, kCalls51A2B0),
    CALLS_ROW(Scena09_Leave48, 0x51A2D0, 0x16, kCalls51A2D0),
    CALLS_ROW(ScenaCall_Leave8, 0x51A2F0, 0xF, kCalls51A2F0),
    CALLS_ROW(ScenaCall_SaveAndLeave, 0x51A300, 0x53, kCalls51A300),
    CALLS_ROW(Scena09_Leave7, 0x51A360, 0xF, kCalls51A360),
    CALLS_ROW(Scena10_Party728, 0x51A370, 0x2D, kCalls51A370),
    CALLS_ROW(Scena10_Party75, 0x51A3A0, 0x6A, kCalls51A3A0),
    CALLS_ROW(Scena10_Join8, 0x51A410, 0x9, kCalls51A410),
    CALLS_ROW(ScenaCall_Join5, 0x51A420, 0x9, kCalls51A420),
    CALLS_ROW(Scena10_RestoreSaved, 0x51A430, 0x55, kCalls51A430),
    CALLS_ROW(Scena10_Leave5, 0x51A490, 0x24, kCalls51A490),
    CALLS_ROW(Scena10_LeaveAll, 0x51A4C0, 0x43, kCalls51A4C0),
    CALLS_ROW(ScenaCall_Set725ReloadJoin5, 0x51A510, 0x23, kCalls51A510),
    CALLS_ROW(ScenaCall_Set742ReloadJoin42, 0x51A540, 0x2A, kCalls51A540),
    CALLS_ROW(Scena12_Set752ReloadJoin25, 0x51A570, 0x2A, kCalls51A570),
    CALLS_ROW(Scena13_Party765, 0x51A5A0, 0x2D, kCalls51A5A0),
    CALLS_ROW(Scena13_SaveParty7, 0x51A5D0, 0x60, kCalls51A5D0),
    CALLS_ROW(Scena13_Set758Party7, 0x51A630, 0x1F, kCalls51A630),
    CALLS_ROW(Scena13_RestoreSaved, 0x51A650, 0x6A, kCalls51A650),
    CALLS_ROW(Scena13_Party758, 0x51A6C0, 0x31, kCalls51A6C0),
    CALLS_ROW(Scena13_LeaveAllParty7, 0x51A700, 0x5D, kCalls51A700),
    CALLS_ROW(Scena13_Join4, 0x51A760, 0xF, kCalls51A760),
    CALLS_ROW(Scena13_Join2, 0x51A770, 0xF, kCalls51A770),
    CALLS_ROW(Scena14_Party748, 0x51A780, 0x2D, kCalls51A780),
    CALLS_ROW(Scena14_Party746, 0x51A7B0, 0x2D, kCalls51A7B0),
    CALLS_ROW(Scena14_Party756, 0x51A7E0, 0x2D, kCalls51A7E0),
    CALLS_ROW(Scena14_SaveLeaveAllPartyA, 0x51A810, 0x71, kCalls51A810),
    CALLS_ROW(Scena14_Out10Party748, 0x51A890, 0x46, kCalls51A890),
    CALLS_ROW(Scena14_Party015Lists785, 0x51A8E0, 0x47, kCalls51A8E0),
    CALLS_ROW(Scena14_Party726, 0x51A930, 0x31, kCalls51A930),
    CALLS_ROW(Scena14_Party74First, 0x51A970, 0x3A, kCalls51A970),
    CALLS_ROW(Scena14_SaveLeaveAllParty7, 0x51A9B0, 0x5D, kCalls51A9B0),
    CALLS_ROW(Scena14_Set726Party7, 0x51AA10, 0x23, kCalls51AA10),
    CALLS_ROW(ScenaCall_Party784, 0x51AA40, 0x31, kCalls51AA40),
    CALLS_ROW(Scena14_Party74Other, 0x51AA80, 0x68, kCalls51AA80),
    CALLS_ROW(Scena14_RecountLeaveAll, 0x51AAF0, 0x53, kCalls51AAF0),
    CALLS_ROW(Scena15_LeaveAllParty7, 0x51AB50, 0x68, kCalls51AB50),
    CALLS_ROW(Scena15_Join42, 0x51ABC0, 0x16, kCalls51ABC0),
    CALLS_ROW(Scena15_Party782, 0x51ABE0, 0x31, kCalls51ABE0),
    CALLS_ROW(ScenaCall_Party034, 0x51AC20, 0x2D, kCalls51AC20),
};
#undef CALLS_ROW
constexpr unsigned kCount = sizeof kRows / sizeof kRows[0];
static_assert(kCount == 98, "the band's 98 starts not yet ours");

constexpr std::uint32_t kSaveAndLeave = 0x51A300, kLeaveAllParty7 = 0x51AB50;

// --- the two that read ecx ---------------------------------------------------

// `mov ecx, [g_calls_ecx]` (8B 0D disp32), then Capcom's bytes with each call
// site's rel32 re-pointed from here at the callee the original reaches - the
// harness then re-aims them at the recorders as for any clone.
constexpr std::uint32_t kPrefix = 6;
alignas(16) unsigned char g_theirs[2][0x80];
sh::CallSite g_sites[2][8];

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

std::uint32_t Prefixed(unsigned slot, const Row& r) {
    unsigned char* const buf = g_theirs[slot];
    if (r.size + kPrefix > sizeof g_theirs[slot] || r.n_calls > 8) bof3::Fatal("scena_calls: %s does not fit its copy", r.name);
    buf[0] = 0x8B;
    buf[1] = 0x0D;
    const std::uint32_t cell = Key(&g_calls_ecx);
    std::memcpy(buf + 2, &cell, 4);
    std::memcpy(buf + kPrefix, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(r.base)), r.size);
    for (int i = 0; i < r.n_calls; ++i) {
        const std::uint32_t at = r.calls[i].offset + kPrefix;
        const std::int32_t rel = static_cast<std::int32_t>(r.calls[i].target - (Key(buf) + at + 5));
        std::memcpy(buf + at + 1, &rel, 4);
        g_sites[slot][i] = {at, r.calls[i].target};
    }
    return Key(buf);
}

// --- the callees -----------------------------------------------------------------
//
// The harness's standard set holds Party_Join (kU8, kFlag) and PartySet_Load
// (four whole words). Listed here: PartySet_Load again, with bytes (it and
// PartySet_Find read a, b, c and the mode as bytes, and four entries pass
// dwords whose upper bytes are stale stack or the caller's ecx),
// Field_PartyLoad and SE's Scena08_PartyJoin784 (neither in the standard set),
// Party_Join again for the stir, and the two callees nobody names.

unsigned char Pick4(std::uint32_t n) {
    switch (n % 4) {
    case 0: return 7;
    case 1: return 2;
    case 2: return 4;
    default: return static_cast<unsigned char>((n >> 2) % 12);
    }
}

// Moves one cell an entry reads again after a call: the member count (0..4),
// a member's id or its +0 bit 0, a saved byte, a list byte, an
// MoveScript_EffectState index (0..7), a record's +9 / +0xB. The harness's
// disturbance reaches a group cell about one call in 24, too seldom for a
// read before a call to be told from one after it, so each callee's recorder
// does it too, from the recorders' stream (the same on both passes).
void Move(std::uint32_t n) {
    const std::uint32_t v = n >> 8;
    switch (n % 8) {
    case 0: Field_MemberCount = static_cast<unsigned char>(v % 5); break;
    case 1: sh::Mem(at::kMemberIds + (v % 3) * at::kMemberStride)[0] = Pick4(v >> 4); break;
    case 2: sh::Mem(at::kSaved + v % 6)[0] = Pick4(v >> 4); break;
    case 3: sh::Mem(at::kPartyLists + v % 6)[0] = Pick4(v >> 4); break;
    case 4: MoveScript_EffectState[v % 24] = static_cast<unsigned char>((v >> 5) % 8); break;
    case 5: {
        unsigned char* const r = sh::Mem(bof3::addr::CharacterRecords + (v % 8) * at::kRecordStride);
        r[0xB] = static_cast<unsigned char>(r[0xB] ^ 1);
        r[9] = static_cast<unsigned char>(v >> 4);
        break;
    }
    case 6: {
        unsigned char* const m = ObjTrio + (v % 3) * at::kMemberStride;
        m[0] = static_cast<unsigned char>(m[0] ^ 1);
        break;
    }
    default: break;
    }
}
std::uint32_t Stir(const std::uint32_t*, std::uint32_t answer) {
    Move(sh::Noise());
    return answer;
}

#define CALLS_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
const sh::Callee kCallees[] = {
    {CALLS_OURS(PartySet_Load), 4, {0xFF, 0xFF, 0xFF, 0xFF}, kG, 0, 0, {}, &Stir},
    {CALLS_OURS(Party_Join), 1, {0xFF}, sh::Answer::kFlag, 0, 0, {}, &Stir},
    {CALLS_OURS(Field_PartyLoad), 1, {0xFF}, kG, 0, 0, {}, &Stir},
    {CALLS_OURS(Scena08_PartyJoin784), 0, {}, kG, 0, 0, {}, &Stir},
    {"0x534030", at::kLeave, at::kLeave, 1, {0xFF}, kG, 0, 0, {}, &Stir},
    {"0x533E00", at::kPalettes, at::kPalettes, 0, {}, kG, 0, 0, {}, &Stir},
};

// --- the state -------------------------------------------------------------------

// Beyond the harness's 22 (which hold Cond_Flags with both party lists,
// Field_MemberCount, ObjTrio, and 0x903A04..0x903A13).
const sh::Region kRegions[] = {
    {0x903A14, 4},                                               // the saved second list's last two bytes, and two spare
    {Key(MoveScript_EffectState), 24},                   // the id -> record map, seeded 0..7
    {bof3::addr::CharacterRecords, 8 * at::kRecordStride},       // the eight records it names
};

void Disturb(std::uint32_t h) { Move(h); }

// --- the seed --------------------------------------------------------------------

sh::Clone g_clones[kCount];

void Seed(unsigned k) {
    // the count 0..4 (a count of 5 or more overwrites 0x51A300's and
    // 0x51AB50's return address: those two cannot be run past 4)
    Field_MemberCount = static_cast<unsigned char>(sh::Next() % 5);
    // the ids the entries compare with 7, 2 and 4, and others
    for (unsigned i = 0; i < 3; ++i) sh::Mem(at::kMemberIds + i * at::kMemberStride)[0] = Pick4(sh::Next());
    for (unsigned i = 0; i < 6; ++i) sh::Mem(at::kSaved + i)[0] = Pick4(sh::Next());
    if (sh::Half())
        for (unsigned i = 0; i < 6; ++i) sh::Mem(at::kPartyLists + i)[0] = Pick4(sh::Next());
    // every index names one of the eight records in the region
    for (unsigned i = 0; i < 24; ++i) MoveScript_EffectState[i] = static_cast<unsigned char>(sh::Next() % 8);
    // the caller's ecx: the entry's index in the tables that hold it (Scenario_CallA
    // / B leave n & 0xFF), another byte, or any word
    const std::uint32_t base = kRows[k].base;
    if (base == kSaveAndLeave || base == kLeaveAllParty7) {
        const std::uint32_t r = sh::Next();
        switch (r % 4) {
        case 0: g_calls_ecx = base == kSaveAndLeave ? 6 : 0; break;
        case 1: g_calls_ecx = base == kSaveAndLeave ? 3 : 0; break;
        case 2: g_calls_ecx = (r >> 8) & 0xFF; break;
        default: g_calls_ecx = sh::Next(); break;
        }
    }
}

}  // namespace

void SelfTest() {
    unsigned slot = 0;
    for (unsigned k = 0; k < kCount; ++k) {
        const Row& r = kRows[k];
        g_clones[k] = {r.name, r.base, r.size, r.calls, r.n_calls, nullptr, 0, nullptr, 0, r.ours, 0, false, sh::Shape::kEntry};
        if (r.base == kSaveAndLeave || r.base == kLeaveAllParty7) {
            g_clones[k].base = Prefixed(slot, r);
            g_clones[k].size = r.size + kPrefix;
            g_clones[k].calls = g_sites[slot];
            g_clones[k].ours = reinterpret_cast<const void*>(r.base == kSaveAndLeave ? &CallsSaveAndLeaveOurs : &CallsLeaveAllParty7Ours);
            ++slot;
        }
    }
    sh::Group group = {
        "scena_calls", g_clones, kCount, kCallees, sizeof kCallees / sizeof kCallees[0],
        nullptr, 0, kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 3000,
    };
    group.chapter = 9;
    sh::Run(group);
}

}  // namespace scena_calls
